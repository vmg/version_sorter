/*
 *  rb_version_sorter.c
 *  version_sorter
 *
 *  Created by K. Adam Christensen on 10/12/09.
 *  Copyright 2009. All rights reserved.
 *
 */

#include <ruby.h>
#include "version_sorter.h"

static VALUE rb_version_sorter_module;

static VALUE rb_sort(VALUE, VALUE);
static VALUE rb_rsort(VALUE, VALUE);


VALUE
rb_sort(VALUE obj, VALUE list)
{
    long len = RARRAY_LEN(list);
    long i;
    char **c_list = calloc(len, sizeof(char *));
    int *ordering;
    VALUE rb_str, dest;

    for (i = 0; i < len; i++) {
        rb_str = rb_ary_entry(list, i);
        c_list[i] = StringValuePtr(rb_str);
    }
    ordering = version_sorter_sort(c_list, len);

    dest = rb_ary_new2(len);
    for (i = 0; i < len; i++) {
        rb_ary_store(dest, i, rb_ary_entry(list, ordering[i]));
    }
    free(ordering);

    return dest;
}

VALUE
rb_rsort(VALUE obj, VALUE list)
{
    VALUE dest = rb_sort(obj, list);
    rb_ary_reverse(dest);
    return dest;
}

extern VALUE version_sort_rb(VALUE rb_version_array);

void
Init_version_sorter(void)
{
    rb_version_sorter_module = rb_define_module("VersionSorter");
    rb_define_module_function(rb_version_sorter_module, "sort", rb_sort, 1);
    rb_define_module_function(rb_version_sorter_module, "rsort", rb_rsort, 1);
    rb_define_module_function(rb_version_sorter_module, "sort_", version_sort_rb, 1);
}
