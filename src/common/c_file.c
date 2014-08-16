/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin, Amanieu d'Antras

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Classes and functions that manipulate files. */

#include "c_shared.h"

/* This file legitimately uses standard library file I/O functions */
#ifdef PLUTOCRACY_LIBC_ERRORS
#undef fopen
#undef fclose
#undef FILE
#endif

/******************************************************************************\
 Close a file that was opened with C_file_open_read or C_file_open_write.
\******************************************************************************/
void C_file_cleanup(c_file_t *file)
{
        if (!file || !file->stream || !file->type)
                return;
        if (file->type == C_FT_LIBC)
                fclose(file->stream);
        else if (file->type == C_FT_ZLIB)
                gzclose(file->stream);
        else
                C_error("Invalid file I/O type %d", file->type);
        file->type = C_FT_NONE;
        file->stream = NULL;
}

/******************************************************************************\
 Try opening the regular and gz-suffixed filename.
\******************************************************************************/
static void file_open(c_file_t *file, const char *path, const char *name)
{
        if (file->stream)
                return;
        if ((file->stream = gzopen(C_va("%s/%s", path, name), "rb")))
                return;
        file->stream = gzopen(C_va("%s/%s.gz", path, name), "rb");
}

/******************************************************************************\
 Open a file for reading. If the file is not found, the function will try again
 with a .gz extension. Returns TRUE on success.

 The following search order is used:

   1) User's directory (~/.plutocracy/file)
   2) Current directory (./file)
   3) Installation directory (/usr/local/share/plutocracy/file)

\******************************************************************************/
int C_file_init_read(c_file_t *file, const char *name)
{
        if (!file || !name || !name[0])
                return FALSE;
        file->stream = NULL;
        if (!C_absolute_path(name))
                file_open(file, C_user_dir(), name);
        file_open(file, ".", name);
        file_open(file, C_app_dir(), name);
        if (!file->stream) {
                file->type = C_FT_NONE;
                return FALSE;
        }
        file->type = C_FT_ZLIB;
        return TRUE;
}

/******************************************************************************\
 Open a file for writing.
\******************************************************************************/
int C_file_init_write(c_file_t *file, const char *name)
{
        file->stream = fopen(name, "wb");
        if (!file->stream) {
                file->type = C_FT_NONE;
                return FALSE;
        }
        file->type = C_FT_LIBC;
        return TRUE;
}

/******************************************************************************\
 Read [len] bytes from file [file] into [buf]. Returns the number of bytes
 read.
\******************************************************************************/
int C_file_read(c_file_t *file, char *buf, int len)
{
        if (!file || !file->stream || !file->type)
                return 0;
        if (file->type == C_FT_LIBC)
                return (int)fread(buf, 1, len, (FILE *)file->stream);
        else if (file->type == C_FT_ZLIB)
                return gzread((gzFile *)file->stream, buf, len);
        C_error("Invalid file I/O type %d", file->type);
        return 0;
}

/******************************************************************************\
 Write [len] bytes from [buf] into [file]. Returns the number of bytes actually
 written.
\******************************************************************************/
int C_file_write(c_file_t *file, const char *buf, int len)
{
        if (!file || !file->stream || !file->type)
                return 0;
        if (file->type != C_FT_LIBC)
                C_error("Invalid file I/O type %d", file->type);
        return (int)fwrite(buf, 1, len, (FILE *)file->stream);
}

/******************************************************************************\
 Flush all buffered write operations into file.
\******************************************************************************/
void C_file_flush(c_file_t *file)
{
        if (!file || !file->stream || !file->type)
                return;
        if (file->type != C_FT_LIBC)
                C_error("Invalid file I/O type %d", file->type);
        fflush((FILE *)file->stream);
}

/******************************************************************************\
 Format a string according to [fmt] and write it to file [file]. Returns the
 number of bytes actually written. This version accepts a va_list parameter
 instead of an actual variable argument list.
\******************************************************************************/
int C_file_vprintf(c_file_t *file, const char *fmt, va_list va)
{
        if (!file || !file->stream || !file->type)
                return 0;
        if (file->type != C_FT_LIBC)
                C_error("Invalid file I/O type %d", file->type);
        return vfprintf((FILE *)file->stream, fmt, va);
}

/******************************************************************************\
 Format a string according to [fmt] and write it to file [file]. Returns the
 number of bytes actually written.
\******************************************************************************/
int C_file_printf(c_file_t *file, const char *fmt, ...)
{
        va_list ap;
        int ret;

        va_start(ap, fmt);
        ret = C_file_vprintf(file, fmt, ap);
        va_end(ap);
        return ret;
}

/******************************************************************************\
 Returns TRUE if the file exists.
\******************************************************************************/
int C_file_exists(const char *name)
{
        FILE *file;

        file = fopen(name, "r");
        if (file) {
                fclose(file);
                return TRUE;
        }
        return FALSE;
}

/******************************************************************************\
 Initializes a token file structure. Returns FALSE on failure.
\******************************************************************************/
int C_token_file_init(c_token_file_t *tf, const char *filename)
{
        C_strncpy_buf(tf->filename, filename);
        tf->token = tf->pos = tf->buffer + sizeof (tf->buffer) - 3;
        tf->pos[1] = tf->swap = ' ';
        tf->pos[2] = NUL;
        tf->eof = FALSE;
        if (!C_file_init_read(&tf->file, filename))
                return FALSE;
        C_debug("Opened token file '%s'", filename);
        return TRUE;
}

/******************************************************************************\
 Initializes a token file structure without actually opening a file. The
 buffer is filled with [string].
\******************************************************************************/
void C_token_file_init_string(c_token_file_t *tf, const char *string)
{
        C_strncpy_buf(tf->buffer, string);
        tf->token = tf->pos = tf->buffer;
        tf->swap = tf->buffer[0];
        tf->filename[0] = NUL;
        tf->file.stream = NULL;
        tf->file.type = C_FT_NONE;
        tf->eof = TRUE;
}

/******************************************************************************\
 Cleanup resources associated with a token file.
\******************************************************************************/
void C_token_file_cleanup(c_token_file_t *tf)
{
        if (!tf || !tf->file.type || !tf->file.stream)
                return;
        C_file_cleanup(&tf->file);
}

/******************************************************************************\
 Fills the buffer with new data when necessary. Should be run before any time
 the token file buffer position advances.

 Because we need to check ahead one character in order to properly detect
 C-style block comments, this function will read in the next chunk while there
 is still one unread byte left in the buffer.
\******************************************************************************/
static void token_file_check_chunk(c_token_file_t *tf)
{
        int token_len, bytes_read, to_read;

        if ((tf->pos[1] && tf->pos[2]) || tf->eof)
                return;
        token_len = (int)(tf->pos - tf->token) + 1;
        if (token_len >= sizeof (tf->buffer) - 2) {
                C_warning("Oversize token in '%s'", tf->filename);
                token_len = 0;
        }
        memmove(tf->buffer, tf->token, token_len);
        tf->token = tf->buffer;
        tf->buffer[token_len] = tf->pos[1];
        tf->pos = tf->buffer + token_len - 1;
        to_read = sizeof (tf->buffer) - token_len - 2;
        bytes_read = C_file_read(&tf->file, tf->pos + 2, to_read);
        tf->buffer[token_len + bytes_read + 1] = NUL;
        tf->eof = bytes_read < to_read;
}

/******************************************************************************\
 Returns positive if the string starts with a comment. Returns negative if the
 string is a block comment. Returns zero otherwise.
\******************************************************************************/
static int is_comment(const char *str)
{
        if (str[0] == '/' && str[1] == '*')
                return -1;
        if (str[0] == '#' || (str[0] == '/' && str[1] == '/'))
                return 1;
        return 0;
}

/******************************************************************************\
 Returns TRUE if the string ends the current comment type.
\******************************************************************************/
static int is_comment_end(const char *str, int type)
{
        return (type > 0 && str[0] == '\n') ||
               (type < 0 && str[0] == '/' && str[-1] == '*');
}

/******************************************************************************\
 Read a token out of a token file. A token is either a series of characters
 unbroken by spaces or comments or a single or double-quote enclosed string.
 The kind of enclosed string (or zero) is returned via [quoted]. Enclosed
 strings are parsed for backslash symbols. Token files support Bash, C, and
 C++ style comments.

 Always returns a non-NULL string. A zero-length string indicates end of file.

 FIXME: Symbols in identifiers should be read as individual tokens.
\******************************************************************************/
const char *C_token_file_read_full(c_token_file_t *tf, int *quoted)
{
        int parsing_comment, parsing_string;

        if (!tf->pos || tf->pos < tf->buffer ||
            tf->pos >= tf->buffer + sizeof (tf->buffer))
                C_error("Invalid token file");
        *tf->pos = tf->swap;

        /* Skip space */
        parsing_comment = FALSE;
        while (C_is_space(*tf->pos) || parsing_comment || is_comment(tf->pos)) {
                if (!parsing_comment)
                        parsing_comment = is_comment(tf->pos);
                else if (is_comment_end(tf->pos, parsing_comment))
                        parsing_comment = 0;
                token_file_check_chunk(tf);
                tf->token = ++tf->pos;
        }
        if (!*tf->pos) {
                if (quoted)
                        *quoted = FALSE;
                return "";
        }

        /* Read token */
        parsing_string = FALSE;
        if (*tf->pos == '"' || *tf->pos == '\'') {
                parsing_string = *tf->pos;
                token_file_check_chunk(tf);
                tf->token = ++tf->pos;
        }
        while (*tf->pos) {
                if (parsing_string) {
                        if (*tf->pos == parsing_string) {
                                token_file_check_chunk(tf);
                                *(tf->pos++) = NUL;
                                break;
                        } else if (parsing_string == '"' && *tf->pos == '\\') {
                                token_file_check_chunk(tf);
                                memmove(tf->pos, tf->pos + 1, tf->buffer +
                                        sizeof (tf->buffer) - tf->pos - 1);
                                if (*tf->pos == 'n')
                                        *tf->pos = '\n';
                                else if (*tf->pos == 'r')
                                        *tf->pos = '\r';
                                else if (*tf->pos == 't')
                                        *tf->pos = '\t';
                        }
                } else if (C_is_space(*tf->pos) || is_comment(tf->pos))
                        break;
                token_file_check_chunk(tf);
                tf->pos++;
        }

        tf->swap = *tf->pos;
        *tf->pos = NUL;
        if (quoted)
                *quoted = parsing_string;
        return tf->token;
}

/******************************************************************************\
 Reads a string in Quake-like key/value pair format:

   // C and C++ comments allowed
   c_my_var_has_no_spaces "values always in quotes"
                          "can be concatenated"
   c_numbers_not_in_quotes -123.5

 Ignores newlines and spaces. Variable names entered without values will have
 their current values printed out.

 The [callback] function will be called for every single key and key-value
 pair. Single keys will have a NULL [value] argument. If [callback] returns
 FALSE, file parsing will stop.

 The caller is responsible for initializing and cleaning up the token file!
\******************************************************************************/
void C_token_file_parse_pairs(c_token_file_t *tf, c_key_value_f callback)
{
        const char *token;
        int quoted, have_value;
        char key[C_TOKEN_SIZE], value[C_TOKEN_SIZE];

        key[0] = NUL;
        have_value = FALSE;
        for (;;) {
                token = C_token_file_read_full(tf, &quoted);
                if (!token[0] && !quoted)
                        break;
                if (!C_is_digit(token[0]) && !quoted) {
                        if (key[0] && !callback(key, have_value ? value : NULL))
                                return;
                        C_strncpy_buf(key, token);
                        value[0] = NUL;
                        have_value = FALSE;
                        continue;
                }
                strncat(value, token, sizeof (value) - strlen(value));
                have_value = TRUE;
        }
        callback(key, have_value ? value : NULL);
}

