/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Functions and structures that allocate or manage generic memory. */

#include "c_shared.h"

/* This is the only file that legitimately uses standard library allocation
   and freeing functions */
#ifdef PLUTOCRACY_LIBC_ERRORS
#undef calloc
#undef free
#undef malloc
#undef realloc
#endif

/* When memory checking is enabled, this structure is prepended to every
   allocated block. There is also a no-mans-land chunk, filled with a specific
   byte, at the end of every allocated block as well. */
#define NO_MANS_LAND_BYTE 0x5a
#define NO_MANS_LAND_SIZE 64
typedef struct c_mem_tag {
        struct c_mem_tag *next;
        const char *alloc_file, *alloc_func, *free_file, *free_func;
        void *data;
        size_t size;
        int alloc_line, free_line, freed;
        char no_mans_land[NO_MANS_LAND_SIZE];
} c_mem_tag_t;

static c_mem_tag_t *mem_root;
static size_t mem_bytes, mem_bytes_max;
static int mem_calls;

/******************************************************************************\
 Initialize an array.
\******************************************************************************/
void C_array_init_full(c_array_t *array, int item_size, int cap)
{
        array->item_size = item_size;
        array->len = 0;
        if (cap < 0)
                cap = 0;
        array->capacity = cap;
        array->data = cap > 0 ? C_malloc(cap * item_size) : 0;
}

/******************************************************************************\
 Ensure that enough space is allocated for [n] elements.
\******************************************************************************/
void C_array_reserve(c_array_t *array, int n)
{
        array->data = C_realloc(array->data, array->item_size * n);
        array->capacity = n;
}

/******************************************************************************\
 Append something to an array. [item] may be NULL or point to a structure of
 size [item_size] to copy into the new location. Returns the new item's index.
\******************************************************************************/
int C_array_append(c_array_t *array, void *item)
{
        if (array->len >= array->capacity) {
                if (array->len > array->capacity)
                        C_error("Invalid array");
                if (array->capacity < 1)
                        array->capacity = 1;
                C_array_reserve(array, array->capacity * 2);
        }
        if (item)
                memcpy((char *)array->data + array->len * array->item_size,
                       item, array->item_size);
        return array->len++;
}

/******************************************************************************\
 Realloc so the array isn't overallocated, and return the pointer to the
 dynamic memory, otherwise cleaning up.
\******************************************************************************/
void *C_array_steal(c_array_t *array)
{
        void *result;

        result = C_realloc(array->data, array->len * array->item_size);
        C_zero(array);
        return result;
}

/******************************************************************************\
 Clean up after the array.
\******************************************************************************/
void C_array_cleanup(c_array_t *array)
{
        C_free(array->data);
        memset(array, 0, sizeof (*array));
}

/******************************************************************************\
 Allocates new memory, similar to realloc_checked().
\******************************************************************************/
static void *malloc_checked(const char *file, int line, const char *function,
                            size_t size)
{
        c_mem_tag_t *tag;
        size_t real_size;

        real_size = size + sizeof (c_mem_tag_t) + NO_MANS_LAND_SIZE;
        tag = malloc(real_size);
        tag->data = (char *)tag + sizeof (c_mem_tag_t);
        tag->size = size;
        tag->alloc_file = file;
        tag->alloc_line = line;
        tag->alloc_func = function;
        tag->freed = FALSE;
        memset(tag->no_mans_land, NO_MANS_LAND_BYTE, NO_MANS_LAND_SIZE);
        memset((char *)tag->data + size, NO_MANS_LAND_BYTE, NO_MANS_LAND_SIZE);
        tag->next = NULL;
        if (mem_root)
                tag->next = mem_root;
        mem_root = tag;
        mem_bytes += size;
        mem_calls++;
        if (mem_bytes > mem_bytes_max)
                mem_bytes_max = mem_bytes;
        return tag->data;
}

/******************************************************************************\
 Finds the memory tag that holds [ptr].
\******************************************************************************/
static c_mem_tag_t *find_tag(const void *ptr, c_mem_tag_t **prev_tag)
{
        c_mem_tag_t *tag, *prev;

        prev = NULL;
        tag = mem_root;
        while (tag && tag->data != ptr) {
                prev = tag;
                tag = tag->next;
        }
        if (prev_tag)
                *prev_tag = prev;
        return tag;
}

/******************************************************************************\
 Reallocate [ptr] to [size] bytes large. Abort on error. String all the
 allocated chunks into a linked list and tracks information about the
 memory and where it was allocated from. This is used later in C_free() and
 C_check_leaks() to detect various errors.
\******************************************************************************/
static void *realloc_checked(const char *file, int line, const char *function,
                             void *ptr, size_t size)
{
        c_mem_tag_t *tag, *prev_tag, *old_tag;
        size_t real_size;

        if (!ptr)
                return malloc_checked(file, line, function, size);
        if (!(tag = find_tag(ptr, &prev_tag)))
                C_error_full(file, line, function,
                             "Trying to reallocate unallocated address (0x%x)",
                             ptr);
        old_tag = tag;
        real_size = size + sizeof (c_mem_tag_t) + NO_MANS_LAND_SIZE;
        tag = realloc((char *)ptr - sizeof (c_mem_tag_t), real_size);
        if (!tag)
                C_error("Out of memory, %s() (%s:%d) tried to allocate %d "
                        "bytes", function, file, line, size );
        if (prev_tag)
                prev_tag->next = tag;
        if (old_tag == mem_root)
                mem_root = tag;
        mem_bytes += size - tag->size;
        if (size > tag->size) {
                mem_calls++;
                if (mem_bytes > mem_bytes_max)
                        mem_bytes_max = mem_bytes;
        }
        tag->size = size;
        tag->alloc_file = file;
        tag->alloc_line = line;
        tag->alloc_func = function;
        tag->data = (char *)tag + sizeof (c_mem_tag_t);
        memset((char *)tag->data + size, NO_MANS_LAND_BYTE, NO_MANS_LAND_SIZE);
        return tag->data;
}

/******************************************************************************\
 Reallocate [ptr] to [size] bytes large. Abort on error. When memory checking
 is enabled, this calls realloc_checked() instead.
\******************************************************************************/
void *C_realloc_full(const char *file, int line, const char *function,
                     void *ptr, size_t size)
{
        static int inited;

        if (!inited) {
                inited = TRUE;
                C_var_unlatch(&c_mem_check);
        }
        if (c_mem_check.value.n)
                return realloc_checked(file, line, function, ptr, size);
        ptr = realloc(ptr, size);
        if (!ptr)
                C_error_full(file, line, function,
                             "Out of memory, tried to allocate %u bytes",
                             size);
        return ptr;
}

/******************************************************************************\
 Checks if a no-mans-land region has been corrupted.
\******************************************************************************/
static int check_no_mans_land(const char *ptr)
{
        int i;

        for (i = 0; i < NO_MANS_LAND_SIZE; i++)
                if (ptr[i] != NO_MANS_LAND_BYTE)
                        return FALSE;
        return TRUE;
}

/******************************************************************************\
 Frees memory. If memory checking is enabled, will check the following:
 - [ptr] was never allocated
 - [ptr] was already freed
 - [ptr] no-mans-land (upper or lower) was corrupted
\******************************************************************************/
void C_free_full(const char *file, int line, const char *function, void *ptr)
{
        c_mem_tag_t *tag, *prev_tag, *old_tag;

        if (!c_mem_check.value.n) {
                free(ptr);
                return;
        }
        if (!ptr)
                return;
        if (!(tag = find_tag(ptr, &prev_tag)))
                C_error_full(file, line, function,
                             "Trying to free unallocated address (0x%x)", ptr);
        if (tag->freed)
                C_error_full(file, line, function,
                             "Address (0x%x), %d bytes allocated by "
                             "%s() in %s:%d, already freed by %s() in %s:%d",
                             ptr, tag->size,
                             tag->alloc_func, tag->alloc_file, tag->alloc_line,
                             tag->free_func, tag->free_file, tag->free_line);
        if (!check_no_mans_land(tag->no_mans_land))
                C_error_full(file, line, function,
                             "Address (0x%x), %d bytes allocated by "
                             "%s() in %s:%d, overran lower boundary",
                             ptr, tag->size,
                             tag->alloc_func, tag->alloc_file, tag->alloc_line);
        if (!check_no_mans_land((char *)ptr + tag->size))
                C_error_full(file, line, function,
                             "Address (0x%x), %d bytes allocated by "
                             "%s() in %s:%d, overran upper boundary",
                             ptr, tag->size,
                             tag->alloc_func, tag->alloc_file, tag->alloc_line);
        tag->freed = TRUE;
        tag->free_file = file;
        tag->free_line = line;
        tag->free_func = function;
        old_tag = tag;
        tag = realloc(tag, sizeof (*tag));
        if (prev_tag)
                prev_tag->next = tag;
        if (old_tag == mem_root)
                mem_root = tag;
        mem_bytes -= tag->size;
}

/******************************************************************************\
 If memory checking is enabled, checks for memory leaks and prints warnings.
\******************************************************************************/
void C_check_leaks(void)
{
        c_mem_tag_t *tag;
        int tags;

        if (!c_mem_check.value.n)
                return;
        tags = 0;
        for (tag = mem_root; tag; tag = tag->next) {
                unsigned int i;

                tags++;
                if (tag->freed)
                        continue;
                C_warning("%s() leaked %d bytes in %s:%d",
                          tag->alloc_func, tag->size, tag->alloc_file,
                          tag->alloc_line);

                /* If we leaked a string, we can print it */
                if (tag->size < 1)
                        continue;
                for (i = 0; C_is_print(((char *)tag->data)[i]); i++) {
                        char buf[128];

                        if (i >= tag->size - 1 || i >= sizeof (buf) - 1 ||
                            !((char *)tag->data)[i + 1]) {
                                C_strncpy_buf(buf, tag->data);
                                C_debug("Looks like a string: '%s'", buf);
                                break;
                        }
                }
        }
        C_debug("%d allocation calls, high mark %.1fmb, %d tags",
                mem_calls, mem_bytes_max / 1048576.f, tags);
}

/******************************************************************************\
 Run some test to see if memory checking actually works. Note that code here
 is intentionally poor, so do not fix "bugs" here they are there for a reason.
\******************************************************************************/
void C_test_mem_check(void)
{
        char *ptr;
        int i;

        switch (c_mem_check.value.n) {
        case 0:
        case 1: return;
        case 2: C_debug("Normal operation, shouldn't fail");
                ptr = C_malloc(1024);
                C_free(ptr);
                ptr = C_calloc(1024);
                C_realloc(ptr, 2048);
                C_realloc(ptr, 512);
                C_free(ptr);
                return;
        case 3: C_debug("Intentionally leaking memory");
                ptr = C_malloc(1024);
                return;
        case 4: C_debug("Freeing unallocated memory");
                C_free((void *)0x12345678);
                break;
        case 5: C_debug("Double freeing memory");
                ptr = C_malloc(1024);
                C_free(ptr);
                C_free(ptr);
                break;
        case 6: C_debug("Simulating memory underrun");
                ptr = C_malloc(1024);
                for (i = 0; i > -NO_MANS_LAND_SIZE / 2; i--)
                        ptr[i] = 42;
                C_free(ptr);
                break;
        case 7: C_debug("Simulating memory overrun");
                ptr = C_malloc(1024);
                for (i = 1024; i < 1024 + NO_MANS_LAND_SIZE / 2; i++)
                        ptr[i] = 42;
                C_free(ptr);
                break;
        case 8: C_debug("Reallocating unallocated memory");
                ptr = C_realloc((void *)0x12345678, 1024);
                break;
        case 9: C_debug("Intentionally leaking string");
                ptr = C_malloc(1024);
                C_strncpy(ptr, "This string was leaked", 1024);
                return;
        default:
                C_error("Unknown memory check test %d",
                        c_mem_check.value.n);
        }
        C_error("Memory check test %d failed", c_mem_check.value.n);
}

/******************************************************************************\
 Allocate zero'd memory.
\******************************************************************************/
void *C_recalloc_full(const char *file, int line, const char *function,
                      void *ptr, size_t size)
{
        ptr = C_realloc_full(file, line, function, ptr, size);
        memset(ptr, 0, size);
        return ptr;
}

/******************************************************************************\
 Will first try to find the resource and reference if it has already been
 allocated. In this case *[found] will be set to TRUE. Otherwise, a new
 resource will be allocated and cleared and *[found] will be set to FALSE.
\******************************************************************************/
void *C_ref_alloc_full(const char *file, int line, const char *function,
                       size_t size, c_ref_t **root, c_ref_cleanup_f cleanup,
                       const char *name, int *found)
{
        c_ref_t *prev, *next, *ref;

        if (size < sizeof (c_ref_t) || !root || !name)
                C_error_full(file, line, function,
                             "Invalid reference structure initialization");

        /* Find a place for the object */
        prev = NULL;
        next = NULL;
        ref = *root;
        while (ref) {
                int cmp;

                cmp = strcmp(name, ref->name);
                if (!cmp) {
                        ref->refs++;
                        if (c_mem_check.value.n)
                                C_trace_full(file, line, function,
                                             "Loading '%s', %d refs",
                                             name, ref->refs);
                        if (found)
                                *found = TRUE;
                        return ref;
                }
                if (cmp < 0) {
                        next = ref;
                        ref = NULL;
                        break;
                }
                prev = ref;
                ref = ref->next;
        }
        if (found)
                *found = FALSE;

        /* Allocate a new object */
        ref = C_recalloc_full(file, line, function, NULL, size);
        if (!*root || *root == next)
                *root = ref;
        ref->prev = prev;
        if (prev)
                ref->prev->next = ref;
        ref->next = next;
        if (next)
                ref->next->prev = ref;
        ref->refs = 1;
        ref->cleanup_func = cleanup;
        ref->root = root;
        C_strncpy_buf(ref->name, name);
        if (c_mem_check.value.n)
                C_trace_full(file, line, function,
                             "Loading '%s', allocated", name);
        return ref;
}

/******************************************************************************\
 Increases the reference count.
\******************************************************************************/
void C_ref_up_full(const char *file, int line, const char *function,
                   c_ref_t *ref)
{
        if (!ref)
                return;
        if (ref->refs < 1)
                C_error_full(file, line, function,
                             "Invalid reference structure");
        ref->refs++;
        if (c_mem_check.value.n)
                C_trace_full(file, line, function,
                             "Referenced '%s' (%d refs)", ref->name, ref->refs);
}

/******************************************************************************\
 Decreases the reference count. If there are no references left, the cleanup
 function is called on the data and the memory is freed.
\******************************************************************************/
void C_ref_down_full(const char *file, int line, const char *function,
                     c_ref_t *ref)
{
        if (!ref)
                return;
        if (ref->refs < 1)
                C_error_full(file, line, function,
                             "Invalid reference structure");
        ref->refs--;
        if (ref->refs > 0) {
                if (c_mem_check.value.n)
                        C_trace_full(file, line, function,
                                     "Dereferenced '%s' (%d refs)",
                                     ref->name, ref->refs);
                return;
        }
        if (ref->root) {
                if (*ref->root == ref)
                        *ref->root = ref->next;
                if (ref->prev)
                        ref->prev->next = ref->next;
                if (ref->next)
                        ref->next->prev = ref->prev;
        }
        if (c_mem_check.value.n)
                C_trace_full(file, line, function, "Freed '%s'",
                             ref->name, ref->refs);
        if (ref->cleanup_func)
                ref->cleanup_func(ref);
        C_free(ref);
}

/******************************************************************************\
 Checks the endian-ness of the current system. I will be shocked if this is
 warning ever gets raised for anyone.
\******************************************************************************/
void C_endian_check(void)
{
        union {
                int n;
                char s[sizeof (int)];
        } u;
        int i;

        u.n = 0x03020100;
        for (i = 0; i < 4 && i < sizeof (int); i++)
                if (u.s[i] != i) {
                        C_warning("Not a little endian system");
                        return;
                }
}

