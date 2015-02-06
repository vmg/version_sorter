/*
 *  version_sorter.c
 *  version_sorter
 *
 *  Created by K. Adam Christensen on 10/10/09.
 *  Copyright 2009. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "version_sorter.h"


static VersionSortingItem * version_sorting_item_init(const char *, int);
static void version_sorting_item_free(VersionSortingItem *);
static void version_sorting_item_add_piece(VersionSortingItem *, char *);
static void parse_version_word(VersionSortingItem *);
static void create_normalized_version(VersionSortingItem *, const int, const int);
static int compare_by_version(const void *, const void *);
static enum scan_state scan_state_get(const char);


VersionSortingItem *
version_sorting_item_init(const char *original, int idx)
{
    VersionSortingItem *vsi = malloc(sizeof(VersionSortingItem));
    if (vsi == NULL) {
        DIE("ERROR: Not enough memory to allocate VersionSortingItem")
    }
    vsi->head = NULL;
    vsi->tail = NULL;
    vsi->node_len = 0;
    vsi->widest_len = 0;
    vsi->original = original;
    vsi->original_len = strlen(original);
    vsi->original_idx = idx;
    vsi->normalized = NULL;
    parse_version_word(vsi);

    return vsi;
}

void
version_sorting_item_free(VersionSortingItem *vsi)
{
    VersionPiece *cur;
    while ((cur = vsi->head)) {
        vsi->head = cur->next;
        free(cur->str);
        free(cur);
    }
    if (vsi->normalized != NULL) {
      free(vsi->normalized);
    }
    free(vsi);
}

void
version_sorting_item_add_piece(VersionSortingItem *vsi, char *str)
{
    VersionPiece *piece = malloc(sizeof(VersionPiece));
    if (piece == NULL) {
        DIE("ERROR: Not enough memory to allocate string linked list node")
    }
    piece->str = str;
    piece->len = strlen(str);
    piece->next = NULL;

    if (vsi->head == NULL) {
        vsi->head = piece;
        vsi->tail = piece;
    } else {
        vsi->tail->next = piece;
        vsi->tail = piece;
    }
    vsi->node_len++;
    if (piece->len > vsi->widest_len) {
        vsi->widest_len = piece->len;
    }
}

enum scan_state
scan_state_get(const char c)
{
    if (isdigit(c)) {
        return digit;
    } else if (isalpha(c)) {
        return alpha;
    } else if (c == '-') {
        return pre;
    } else {
        return other;
    }

}

void
parse_version_word(VersionSortingItem *vsi)
{
    int start = 0, end = 0, size = 0;
    char current_char;
    char *part;
    enum scan_state current_state, previous_state;

    previous_state = other;
    while (1) {
        current_char = vsi->original[end];
        current_state = scan_state_get(current_char);

        if (current_state != previous_state && (previous_state == digit || previous_state == alpha)) {
            size = end - start;

            part = malloc((size+1) * sizeof(char));
            if (part == NULL) {
                DIE("ERROR: Not enough memory to allocate word")
            }

            memcpy(part, vsi->original+start, size);
            part[size] = '\0';

            version_sorting_item_add_piece(vsi, part);
            start = end;
        }

        if (current_char == '\0') break;

        end++;

        if (current_state == other || current_state == pre) {
            start = end;
        }

        if (current_state == pre) {
            part = malloc((3+1) * sizeof(char));
            strcpy(part, "pre");
            version_sorting_item_add_piece(vsi, part);
        }

        previous_state = current_state;
    }
}

void
create_normalized_version(VersionSortingItem *vsi, const int widest_len, const int max_pieces)
{
    VersionPiece *cur;
    int i, pos, normalized_size = max_pieces * (widest_len + 1);

    char *result = malloc((normalized_size + 1) * sizeof(char));
    if (result == NULL) {
        DIE("ERROR: Unable to allocate memory")
    }
    pos = 0;

    for (cur = vsi->head; cur; cur = cur->next) {
        if (isdigit(cur->str[0])) {
          // left-pad digits with zeroes
          for (i = 0; i < (widest_len + 1 - cur->len); i++) result[pos++] = '0';
          memcpy(result + pos, cur->str, cur->len);
          pos += cur->len;
        } else {
          // prefix words with "-", right-pad them with zeroes
          result[pos++] = '-';
          memcpy(result + pos, cur->str, cur->len);
          pos += cur->len;
          for (i = 0; i < (widest_len - cur->len); i++) result[pos++] = '0';
        }
    }

    while (pos < normalized_size) result[pos++] = '0';
    result[pos] = '\0';

    vsi->normalized = result;
}

int
compare_by_version(const void *a, const void *b)
{
    return strcmp((*(const VersionSortingItem **)a)->normalized, (*(const VersionSortingItem **)b)->normalized);
}

int*
version_sorter_sort(char **list, size_t list_len)
{
    int i, widest_len = 0, max_pieces = 0;
    VersionSortingItem *vsi;
    VersionSortingItem **sorting_list = calloc(list_len, sizeof(VersionSortingItem *));
    int *ordering = calloc(list_len, sizeof(int));

    for (i = 0; i < list_len; i++) {
        vsi = version_sorting_item_init(list[i], i);
        if (vsi->widest_len > widest_len) {
            widest_len = vsi->widest_len;
        }
        if (vsi->node_len > max_pieces) {
            max_pieces = vsi->node_len;
        }
        sorting_list[i] = vsi;
    }

    for (i = 0; i < list_len; i++) {
        create_normalized_version(sorting_list[i], widest_len, max_pieces);
    }

    qsort((void *) sorting_list, list_len, sizeof(VersionSortingItem *), &compare_by_version);

    for (i = 0; i < list_len; i++) {
        vsi = sorting_list[i];
        list[i] = (char *) vsi->original;
        ordering[i] = vsi->original_idx;
        version_sorting_item_free(vsi);
    }
    free(sorting_list);

    return ordering;
}

enum {
	VERSION_COMP_END = 0,
	VERSION_COMP_NUMBER = 1,
	VERSION_COMP_STRING = 2
};

struct version_number {
	const char *original;
	int original_idx;
	struct version_comp{
		uint32_t flags;
		union {
			uint32_t number;
			struct strchunk {
				uint16_t offset;
				uint16_t len;
			} string;
		} as;
	} components[1];
};

static int
strchunk_cmp(const char *original_a, const struct strchunk *a,
		const char *original_b, const struct strchunk *b)
{
	size_t len = (a->len < b->len) ? a->len : b->len;
	int cmp = memcmp(original_a + a->offset, original_b + b->offset, len);
	return cmp ? cmp : (int)(a->len - b->len);
}


static int
compare_version_number(const struct version_number *a,
		const struct version_number *b)
{
	int n = 0;

	for (n = 0;; ++n) {
		const struct version_comp *ca = &a->components[n];
		const struct version_comp *cb = &b->components[n];

		if (ca->flags == cb->flags) {
			int cmp = 0;

			switch(ca->flags) {
			case VERSION_COMP_END:
				/* Equal */
				return 0;

			case VERSION_COMP_NUMBER:
				cmp = (int)ca->as.number - (int)cb->as.number;
				break;

			case VERSION_COMP_STRING:
				cmp = strchunk_cmp(
					a->original, &ca->as.string,
					b->original, &cb->as.string);
				break;
			}

			if (cmp)
				return cmp;
		} else {
			if (ca->flags == VERSION_COMP_END || ca->flags == VERSION_COMP_NUMBER) {
				if (cb->flags == VERSION_COMP_STRING)
					return 1;
				return -1;
			}

			if (cb->flags == VERSION_COMP_END || ca->flags == VERSION_COMP_STRING) {
				if (ca->flags == VERSION_COMP_STRING)
					return -1;
				return +1;
			}

			assert(0); /* unreachable? */
		}
	}
}

static int
compare_callback(const void *a, const void *b)
{
    return compare_version_number(
		(*(const struct version_number **)a),
		(*(const struct version_number **)b));
}

static struct version_number *
resize_version(struct version_number *version, uint16_t new_size)
{
	return xrealloc(version,
		(sizeof(struct version_number) +
		 sizeof(struct version_comp) * new_size));
}

static struct version_number *
parse_version_number(const char *string)
{
	struct version_number *version = NULL;
	uint16_t offset;
	uint16_t comp_n = 0, comp_alloc = 0;

	for (offset = 0; string[offset];) {
		if (comp_n >= comp_alloc) {
			comp_alloc += 4;
			version = resize_version(version, comp_alloc);
		}

		if (isdigit(string[offset])) {
			uint32_t number = 0;
			uint32_t old_number;

			while (isdigit(string[offset])) {
				old_number = number;
				number = (10 * number) + (string[offset] - '0');

				if (number < old_number)
					rb_raise(rb_eRuntimeError,
						"overflow when comparing numbers in version string");

				offset++;
			}

			version->components[comp_n].as.number = number;
			version->components[comp_n].flags = VERSION_COMP_NUMBER;
			comp_n++;
			continue;
		}

		if (string[offset] == '-' || isalpha(string[offset])) {
			uint16_t start = offset;
			uint16_t len = 0;

			if (string[offset] == '-') {
				offset++;
				len++;
			}

			while (isalpha(string[offset])) {
				offset++;
				len++;
			}

			version->components[comp_n].as.string.offset = start;
			version->components[comp_n].as.string.len = len;
			version->components[comp_n].flags = VERSION_COMP_STRING;
			comp_n++;
			continue;
		}

		offset++;
	}

	version->components[comp_n].flags = VERSION_COMP_END;
	version->original = string;
	return version;
}

VALUE version_sort_rb(VALUE rb_self, VALUE rb_version_array)
{
	struct version_number **versions;
	long length, i;
	VALUE rb_result_array;

	Check_Type(rb_version_array, T_ARRAY);

	length = RARRAY_LEN(rb_version_array);
	versions = xcalloc(length, sizeof(struct version_number *));

	for (i = 0; i < length; ++i) {
		VALUE rb_version = RARRAY_AREF(rb_version_array, i);
		versions[i] = parse_version_number(StringValuePtr(rb_version));
		versions[i]->original_idx = i;
	}

    qsort(versions, length, sizeof(struct version_number *), &compare_callback);
	rb_result_array = rb_ary_new2(length);
	rb_ary_resize(rb_result_array, length);

	for (i = 0; i < length; ++i) {
		VALUE rb_version = RARRAY_AREF(rb_version_array, versions[i]->original_idx);
		RARRAY_ASET(rb_result_array, i, rb_version);
		xfree(versions[i]);
	}
	xfree(versions);
	return rb_result_array;
}

#if 0
static long qsort_partition(
	struct version_number **dst,
	const long left, const long right, const long pivot)
{
	struct version_number *value = dst[pivot];
	long index = left;
	long i;
	int all_same = 1;

	SORT_SWAP(dst[pivot], dst[right]);

	for (i = left; i < right; i++) {
		int cmp = SORT_CMP(dst[i], value);

		if (cmp != 0)
			all_same &= 0;

		if (cmp < 0) {
			SORT_SWAP(dst[i], dst[index]);
			index++;
		}
	}

	SORT_SWAP(dst[right], dst[index]);

	if (all_same)
		return -1;

	return index;
}

static void qsort_r(struct version_number **dst,
		const long left, const long right)
{
	long pivot;
	long new_pivot;

	if (right <= left)
		return;

	if ((right - left + 1) < 16) {
		binary_insertion_sort(&dst[left], right - left + 1);
		return;
	}

	pivot = left + ((right - left) >> 1);
	new_pivot = qsort_partition(dst, left, right, pivot);

	if (new_pivot < 0)
		return;

	qsort_r(dst, left, new_pivot - 1);
	qsort_r(dst, new_pivot + 1, right);
}

void quicksort(struct version_number **dst, const long size)
{
	if (size)
		qsort_r(dst, 0, size - 1);
}

#endif
