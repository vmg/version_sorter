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
#include "ksort.h"

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

struct version_number {
	const char *original;
	uint64_t num_flags;
	int original_idx;
	int size;
	union version_comp {
		uint32_t number;
		struct strchunk {
			uint16_t offset;
			uint16_t len;
		} string;
	} comp[1];
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
	int n, max_n = (a->size < b->size) ? a->size : b->size;

	for (n = 0; n < max_n; ++n) {
		int num_a = (a->num_flags & (1 << n)) != 0;
		int num_b = (b->num_flags & (1 << n)) != 0;

		if (num_a == num_b) {
			const union version_comp *ca = &a->comp[n]; 
			const union version_comp *cb = &b->comp[n];
			int cmp = 0;

			if (num_a) {
				cmp = (int)ca->number - (int)cb->number;
			} else {
				cmp = strchunk_cmp(
						a->original, &ca->string,
						b->original, &cb->string);
			}

			if (cmp) return cmp;
		} else {
			return num_a ? 1 : -1;
		}
	}

	if (a->size < b->size)
		return (b->num_flags & (1 << n)) ? -1 : 1;

	if (a->size > b->size)
		return (a->num_flags & (1 << n)) ? 1 : -1;

	return 0;
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
		 sizeof(union version_comp) * new_size));
}

static struct version_number *
parse_version_number(const char *string)
{
	struct version_number *version = NULL;
	uint16_t offset;
	int comp_n = 0, comp_alloc = 4;

	version = resize_version(version, comp_alloc);
	version->original = string;
	version->num_flags = 0x0;

	for (offset = 0; string[offset] && comp_n < 64;) {
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

			version->comp[comp_n].number = number;
			version->num_flags |= (1 << comp_n);
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

			version->comp[comp_n].string.offset = start;
			version->comp[comp_n].string.len = len;
			comp_n++;
			continue;
		}

		offset++;
	}

	version->size = comp_n;
	return version;
}

typedef struct version_number *version_ptr;
#define VERSION_LT(a, b) (compare_version_number(a, b) < 0)
KSORT_INIT(version, version_ptr, VERSION_LT)

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

	ks_mergesort(version, length, versions);
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
