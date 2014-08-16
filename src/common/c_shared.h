/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* We need to include Python.h first because of the feature test macros it
 * defines. However, it includes setjmp.h which png.h doesn't like, so png.h
 * has to be included firster.
 *
 * Why can't these headers just get along :(
 */
#include <png.h>
#undef _POSIX_C_SOURCE

/* Python */
#include <Python.h>
#include "structmember.h"

/* Standard library */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <time.h>
#include <errno.h>

/* OpenGL */
#ifdef DARWIN
#include <gl.h>
#include <glu.h>
#include <glext.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include "GL/glext.h"
#endif

/* Pango */
#include <pango/pango.h>

/* SDL */
#include "SDL.h"
#include "SDL_main.h"

/* zlib */
#include <zlib.h>

/* Ensure common definitions */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL ((void*)0)
#endif
#ifndef NUL
#define NUL '\0'
#endif
#ifndef INFINITY
#define INFINITY 2147483647
#endif

/* Value ranges */
#define C_SHORT_MIN ((short)-32768)
#define C_SHORT_MAX ((short)32767)
#define C_USHORT_MAX ((unsigned short)65535)
#define C_INT_MIN -2147483648
#define C_INT_MAX 2147483647
#define C_UINT_MAX 4294967295
#define C_FLOAT_MIN 3.4E-38f
#define C_FLOAT_MAX 3.4E+38f
#define C_DOUBLE_MIN 1.7E-308
#define C_DOUBLE_MAX 1.7E+308

/* Floating-point PI */
#define C_PI 3.14159265358979323846f

/* E */
#define C_E 2.7182818284590452f

/* Golden ratio */
#define C_TAU 1.61803398874989f

/* Sides of a unit isoceles triangle */
#define C_SIN_60 0.86602540378443865f
#define C_SIN_30 0.5f

/* Square root of two divided by two */
#define C_COS_45 0.70710678118654752f

/* If you are going to use the C_va* functions, keep in mind that after calling
   any of those functions [C_VA_BUFFERS] times, you will begin overwriting
   the buffers starting from the first. Each buffer also has a fixed size.
   Note that [C_VA_BUFFERS] * [C_VA_BUFFER_SIZE] has to be less than 32k or
   the function will not compile on some systems. */
#define C_VA_BUFFERS 16
#define C_VA_BUFFER_SIZE 2000

/* This is the size of the largest token a token file can contain */
#define C_TOKEN_SIZE 4000

/* All angles should be in radians but there are some cases (OpenGL) where
   conversions are necessary */
#define C_rad_to_deg(a) ((a) * 180.f / C_PI)
#define C_deg_to_rad(a) ((a) * C_PI / 180.f)

/* Certain functions should not be used. Files that legitimately use these
   should undefine these replacements. This is a bad thing to do because some
   standard library implementations will have strange definitions for these
   functions which may call each other. For instance, strcmp() which calls
   strlen(). */
#ifdef PLUTOCRACY_LIBC_ERRORS
#undef calloc
#define calloc(s) ERROR_use_C_calloc
#undef fopen
#define fopen(n, m) ERROR_use_C_file_open
#undef fclose
#define fclose(f) ERROR_use_C_file_close
#undef free
#define free(s) ERROR_use_C_free
#undef malloc
#define malloc(s) ERROR_use_C_malloc
#undef realloc
#define realloc(p, s) ERROR_use_C_realloc
#undef strdup
#define strdup(s) ERROR_use_C_strdup
#undef strlen
#define strlen(s) ERROR_use_C_strlen
#undef strncpy
#define strncpy(d, s, n) ERROR_use_C_strncpy
#undef FILE
#define FILE ERROR_use_c_file_t
#endif

/* Vectors */
#include "c_vectors.h"

/* Log files are wrapped to this many columns */
#define C_LOG_WRAP_COLS 79

/* Debug log levels, errors are fatal and will always abort */
typedef enum {
        C_LOG_PRINT = -1,
        C_LOG_ERROR = 0,
        C_LOG_WARNING,
        C_LOG_STATUS,
        C_LOG_DEBUG,
        C_LOG_TRACE,
} c_log_level_t;

/* Log modes */
typedef enum {
        C_LM_NORMAL,
        C_LM_NO_FUNC,
        C_LM_CLEANUP,
} c_log_mode_t;

/* Variable value types. Note that there is a difference between statically and
   dynamically allocated strings. */
typedef enum {
        C_VT_UNREGISTERED,
        C_VT_INTEGER,
        C_VT_FLOAT,
        C_VT_STRING,
} c_var_type_t;

/* Variables can be set at different times */
typedef enum {
        C_VE_ANYTIME,
        C_VE_LOCKED,
        C_VE_LATCHED,
        C_VE_FUNCTION,
} c_var_edit_t;

/* File I/O types */
typedef enum {
        C_FT_NONE,
        C_FT_LIBC,
        C_FT_ZLIB,
} c_file_type_t;

/* Wrap multiple file I/O libraries */
typedef struct {
        c_file_type_t type;
        void *stream;
} c_file_t;

/* Holds all possible variable types */
typedef union {
        int n;
        float f;
        char s[256];
} c_var_value_t;

/* Define a compact boolean type. This actually does not make much of an impact
   as far as memory usage is concerned but is helpful to denote usage. */
typedef unsigned char bool;

/* Callback for GUI log handler */
typedef void (*c_log_event_f)(c_log_level_t, int margin, const char *);

/* Callback for cleaning up referenced memory */
typedef void (*c_ref_cleanup_f)(void *data);

/* Callback for key-value file parsing */
typedef int (*c_key_value_f)(const char *key, const char *value);

/* Callback for signal catchers */
typedef void (*c_signal_f)(int signal);

/* Callback for modified variables. Return TRUE to set the value. */
typedef struct c_var c_var_t;
typedef int (*c_var_update_f)(c_var_t *, c_var_value_t);

/* Type for configurable variables */
struct c_var {
        const char *name, *comment;
        struct c_var *next;
        void *update_data;
        c_var_value_t value, latched, stock;
        c_var_type_t type;
        c_var_edit_t edit;
        c_var_update_f update;
        int changed;
        bool has_latched, archive, unsafe;
};

/* Resizing arrays */
typedef struct c_array {
        int capacity, len, item_size;
        void *data;
} c_array_t;

/* A structure to hold the data for a file that is being read in tokens */
typedef struct c_token_file {
        char filename[256], buffer[C_TOKEN_SIZE], swap, *pos, *token;
        c_file_t file;
        bool eof;
} c_token_file_t;

/* Reference-counted linked-list. Memory allocated using the referenced
   memory allocator must have this structure as the first member! */
typedef struct c_ref {
        char name[256];
        struct c_ref *prev, *next, **root;
        c_ref_cleanup_f cleanup_func;
        int refs;
} c_ref_t;

/* A counter for counting how often something happens per frame */
typedef struct c_count {
        int start_frame, start_time, last_time;
        float value;
} c_count_t;

/* c_file.c */
void C_file_cleanup(c_file_t *);
int C_file_exists(const char *name);
void C_file_flush(c_file_t *);
int C_file_init_read(c_file_t *, const char *name);
int C_file_init_write(c_file_t *, const char *name);
int C_file_printf(c_file_t *, const char *fmt, ...);
int C_file_read(c_file_t *, char *buf, int len);
int C_file_vprintf(c_file_t *, const char *fmt, va_list va);
int C_file_write(c_file_t *, const char *buf, int len);
void C_token_file_cleanup(c_token_file_t *);
int C_token_file_init(c_token_file_t *, const char *filename);
void C_token_file_init_string(c_token_file_t *, const char *string);
const char *C_token_file_read_full(c_token_file_t *, int *out_quoted);
void C_token_file_parse_pairs(c_token_file_t *, c_key_value_f callback);
#define C_token_file_read(f) C_token_file_read_full(f, NULL)

/* c_log.c */
#define C_assert(s) C_assert_full(__FILE__, __LINE__, __func__, !(s), #s)
void C_assert_full(const char *file, int line, const char *function,
                   int boolean, const char *message);
void C_close_log_file(void);
#define C_debug(fmt, ...) C_log(C_LOG_DEBUG, __FILE__, __LINE__, \
                                __func__, fmt, ## __VA_ARGS__)
#define C_debug_full(f, l, fn, fmt, ...) C_log(C_LOG_DEBUG, f, l, fn, \
                                               fmt, ## __VA_ARGS__)
#define C_error(fmt, ...) C_log(C_LOG_ERROR, __FILE__, __LINE__, \
                                __func__, fmt, ## __VA_ARGS__)
#define C_error_full(f, l, fn, fmt, ...) C_log(C_LOG_ERROR, f, l, \
                                               fn, fmt, ## __VA_ARGS__)
void C_log(c_log_level_t, const char *file, int line,
           const char *function, const char *fmt, ...);
void C_open_log_file(void);
void C_print(const char *);
#define C_status(fmt, ...) C_log(C_LOG_STATUS, __FILE__, __LINE__, \
                                 __func__, fmt, ## __VA_ARGS__)
#define C_status_full(f, l, fn, fmt, ...) C_log(C_LOG_STATUS, f, l, \
                                                fn, fmt, ## __VA_ARGS__)
#define C_trace(fmt, ...) C_log(C_LOG_TRACE, __FILE__, __LINE__, \
                                __func__, fmt, ## __VA_ARGS__)
#define C_trace_full(f, l, fn, fmt, ...) C_log(C_LOG_TRACE, f, l, \
                                               fn, fmt, ## __VA_ARGS__)
#define C_warning(fmt, ...) C_log(C_LOG_WARNING, __FILE__, __LINE__, \
                                  __func__, fmt, ## __VA_ARGS__)
#define C_warning_full(f, l, fn, fmt, ...) C_log(C_LOG_WARNING, f, l, fn, \
                                                 fmt, ## __VA_ARGS__)
char *C_wrap_log(const char *, int margin, int wrap, int *plen);

extern c_log_event_f c_log_func;
extern c_log_mode_t c_log_mode;

/* c_math.c */
#define C_is_pow2(n) !(n & (n - 1))
void C_limit_float(float *value, float min, float max);
void C_limit_int(int *value, int min, int max);
int C_next_pow2(int);
int C_rand(void);
#define C_rand_real() ((float)(C_rand() & 0xffff) / 0xffff)
void C_rand_seed(unsigned int);
int C_roll_dice(int num, int sides);
c_vec3_t C_vec3_rotate_to(c_vec3_t from, c_vec3_t normal,
                          float proportion, c_vec3_t to);
#define C_wave(amp, freq) \
        (1.f - 0.5f * (amp) * (1.f + sinf((freq) * c_time_msec)))

/* c_memory.c */
#define C_alloc(p) ((p) = C_calloc(sizeof (*(p))))
int C_array_append(c_array_t *, void *item);
void C_array_cleanup(c_array_t *);
#define C_array_get(ary, type, i) (((type*)(ary)->data) + i)
#define C_array_init(ary, type, cap) \
        C_array_init_full(ary, (int)sizeof (type), cap)
void C_array_init_full(c_array_t *, int item_size, int cap);
void C_array_reserve(c_array_t *, int n);
void *C_array_steal(c_array_t *);
#define C_calloc(s) C_recalloc_full(__FILE__, __LINE__, __func__, NULL, s)
void C_check_leaks(void);
void C_endian_check(void);
#define C_free(p) C_free_full(__FILE__, __LINE__, __func__, p)
void C_free_full(const char *file, int line, const char *function, void *ptr);
#define C_malloc(s) C_realloc(NULL, s)
#define C_one(s) memset(s, -1, sizeof (*(s)))
#define C_one_buf(s) memset(s, -1, sizeof (s))
#define C_realloc(p, s) C_realloc_full(__FILE__, __LINE__, __func__, p, s)
void *C_realloc_full(const char *file, int line, const char *function,
                     void *ptr, size_t size);
void *C_recalloc_full(const char *file, int line, const char *function,
                      void *ptr, size_t size);
#define C_ref_alloc(s, r, c, n, f) C_ref_alloc_full(__FILE__, __LINE__, \
                                                    __func__, s, r, c, n, f)
void *C_ref_alloc_full(const char *file, int line, const char *function,
                       size_t, c_ref_t **root, c_ref_cleanup_f,
                       const char *name, int *found);
#define C_ref_down(r) C_ref_down_full(__FILE__, __LINE__, __func__, r);
void C_ref_down_full(const char *file, int line, const char *function,
                     c_ref_t *ref);
#define C_ref_up(r) C_ref_up_full(__FILE__, __LINE__, __func__, r);
void C_ref_up_full(const char *file, int line, const char *function,
                   c_ref_t *ref);
void C_test_mem_check(void);
#define C_zero(s) memset(s, 0, sizeof (*(s)))
#define C_zero_buf(s) memset(s, 0, sizeof (s))

/* c_os_posix, c_os_windows.c */
bool C_absolute_path(const char *path);
const char *C_app_dir(void);
int C_mkdir(const char *path);
int C_modified_time(const char *filename);
const char *C_user_dir(void);
void C_signal_handler(c_signal_f);

/* c_string.c */
#define C_bool_string(b) ((b) ? "TRUE" : "FALSE")
void C_cleanup_lang(void);
c_color_t C_color_string(const char *);
char *C_escape_string(const char *);
unsigned int C_hash_djb2(const char *);
void C_init_lang(void);
#define C_is_digit(c) (((c) >= '0' && (c) <= '9') || (c) == '.' || (c) == '-')
int C_is_path(const char *);
#define C_is_print(c) ((c) > 0 && (c) < 0x7f)
#define C_is_space(c) ((c) > 0 && (c) <= ' ')
char *C_line(char **pstr, bool *end);
void C_sanitize(char *);
char *C_skip_spaces(const char *str);
#define C_strdup(s) C_strdup_full(__FILE__, __LINE__, __func__, s)
char *C_strdup_full(const char *file, int line, const char *func, const char *);
int C_strlen(const char *);
#define C_strncpy(d, s, n) C_strncpy_full(__FILE__, __LINE__, __func__, d, s, n)
int C_strncpy_full(const char *file, int line, const char *func,
                   char *dest, const char *src, int len);
#define C_strncpy_buf(d, s) C_strncpy(d, s, sizeof (d))
int C_suffix(char *string, const char *suffix, int size);
#define C_suffix_buf(str, suf) C_suffix(str, suf, sizeof (str))
char *C_token(char **pstr, bool *end_of_string);
int C_utf8_append(char *dest, int *dest_i, int dest_sz, const char *src);
char *C_utf8_encode(unsigned int unicode, int *len);
int C_utf8_index(char *str, int n);
int C_utf8_size(unsigned char first_byte);
int C_utf8_strlen(const char *, int *utf8_chars);
char *C_va(const char *fmt, ...);
char *C_van(int *output_len, const char *fmt, ...);
char *C_vanv(int *output_len, const char *fmt, va_list);
const char *C_str(const char *token, const char *fallback);

extern c_var_t c_lang;

/* c_time.c */
#define C_count_add(c, v) ((c)->value += (v))
float C_count_fps(const c_count_t *);
float C_count_per_frame(const c_count_t *);
float C_count_per_sec(const c_count_t *);
int C_count_poll(c_count_t *, int interval);
void C_count_reset(c_count_t *);
void C_throttle_fps(void);
void C_time_init(void);
void C_time_update(void);
unsigned int C_timer(void);

extern c_count_t c_throttled;
extern int c_time_msec, c_frame_msec, c_frame, c_throttle_msec;
extern float c_frame_sec;

/* c_variables.c */
const char *C_auto_complete_vars(const char *);
int C_color_update(c_var_t *, c_var_value_t);
int C_parse_config_file(const char *filename);
void C_parse_config_string(const char *string);
void C_register_float(c_var_t *, const char *name, float value,
                      const char *comment);
void C_register_integer(c_var_t *, const char *name, int value,
                        const char *comment);
void C_register_string(c_var_t *, const char *name, const char *value,
                       const char *comment);
void C_register_variables(void);
void C_reset_unsafe_vars(void);
c_var_t *C_resolve_var(const char *name);
void C_translate_vars(void);
#define C_var_reset(v) ((v)->value = (v)->stock)
void C_var_set(c_var_t *, const char *value);
bool C_var_unlatch(c_var_t *);
#define C_var_update(v, u) C_var_update_data(v, u, NULL)
void C_var_update_data(c_var_t *, c_var_update_f, void *);
void C_write_autogen(void);

extern c_var_t c_max_fps, c_mem_check, c_show_fps, c_test_int, c_show_bps;
extern int c_exit;
