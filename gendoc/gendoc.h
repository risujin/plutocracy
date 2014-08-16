/******************************************************************************\
 Plutocracy GenDoc - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Prevents deprecation warnings on Windows */
#define _CRT_SECURE_NO_DEPRECATE 1

/* Standard library headers */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* Common defines */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NUL
#define NUL '\0'
#endif

/* Copies string to a fixed-size buffer */
#define D_strncpy_buf(b, s) D_strncpy(b, s, sizeof (b))

/* List entry */
typedef struct entry {
        char name[80], def[4000], comment[4000], file[64];
        struct entry *next;
} entry_t;

/* gendoc.c */
extern entry_t *d_types;

/* gendoc_entry.c */
void D_entry_add(const entry_t *, entry_t **root);
entry_t *D_entry_find(entry_t *root, const char *name);
void D_output_entries(entry_t *root);

/* gendoc_parse.c */
const char *D_comment(void);
const char *D_def(void);
int D_is_keyword(const char *, entry_t **out, int *type);
int D_is_operator(char ch);
void D_parse_close(void);
int D_parse_def(void);
int D_parse_open(const char *filename);
void D_strncpy(char *dest, const char *src, size_t);
const char *D_token(int n);
int D_token_space(int n);

extern FILE *d_file;
extern int d_num_tokens;

