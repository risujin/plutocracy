/******************************************************************************\
 Plutocracy GenDoc - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "gendoc.h"

#define END_BRACE_MARK -2
#define TOKENS 128

int d_num_tokens;

static FILE *file;
static int last_nl, token_space[TOKENS];
static char last_ch, cur_ch, def[32000], comment[4000], tokens[TOKENS][64];

/******************************************************************************\
 A string copy function that isn't retarded.
\******************************************************************************/
void D_strncpy(char *dest, const char *src, size_t size)
{
        size_t len;

        len = strlen(src) + 1;
        if (len > size - 1)
                len = size - 1;
        memmove(dest, src, len);
        dest[size - 1] = NUL;
}

/******************************************************************************\
 Just returns the definition.
\******************************************************************************/
const char *D_def(void)
{
        return def;
}

/******************************************************************************\
 Just returns the comment.
\******************************************************************************/
const char *D_comment(void)
{
        return comment;
}

/******************************************************************************\
 Returns TRUE if the character is an operator.
\******************************************************************************/
int D_is_operator(char ch)
{
        return (ch >= '!' && ch <= '/') || (ch >= ':' && ch <= '@') ||
               (ch >= '{' && ch <= '~') || (ch >= '[' && ch <= '^');
}

/******************************************************************************\
 Returns TRUE if the first token of [buf] is an operator.
\******************************************************************************/
int D_is_keyword(const char *buf, entry_t **entry, int *type)
{
        struct {
                const char *string;
                int type;
        } keywords[] = {
                { "#define",    2 },
                { "bool",       0 },
                { "break",      1 },
                { "char",       0 },
                { "case",       1 },
                { "const",      0 },
                { "do",         1 },
                { "double",     0 },
                { "for",        1 },
                { "else",       1 },
                { "enum",       1 },
                { "extern",     0 },
                { "float",      0 },
                { "goto",       1 },
                { "if",         1 },
                { "inline",     0 },
                { "int",        0 },
                { "long",       0 },
                { "return",     1 },
                { "short",      0 },
                { "signed",     0 },
                { "single",     0 },
                { "sizeof",     1 },
                { "size_t",     0 },
                { "static",     0 },
                { "struct",     1 },
                { "switch",     1 },
                { "typedef",    1 },
                { "unsigned",   0 },
                { "void",       0 },
                { "while",      1 },
        };
        int i, j;
        char keyword[64];

        *type = -1;
        i = 0;
        if (buf[i] == '#')
                i++;
        for (; buf[i] && buf[i] > ' ' && !D_is_operator(buf[i]); i++);
        if (i <= 0 || i > sizeof (keyword))
                return 0;
        memcpy(keyword, buf, i);
        keyword[i] = NUL;

        /* Common C keywords */
        *entry = NULL;
        for (j = 0; j < sizeof (keywords) / sizeof (keywords[0]); j++)
                if (!strcmp(keyword, keywords[j].string)) {
                        *type = keywords[j].type;
                        return i;
                }

        /* Types we already know */
        *entry = D_entry_find(d_types, keyword);
        if (*entry)
                return i;

        return 0;
}

/******************************************************************************\
 Parse a token. Returns the buffer position of the first character after the
 token. Fixes any markers in the definition.
\******************************************************************************/
static int tokenize_def_from(int i)
{
        int j;

        if (!def[i])
                return i;

        /* Skip space */
        for (; def[i] <= ' '; i++)
                token_space[d_num_tokens]++;

        /* C comments */
        if (def[i] == '/' && def[i + 1] == '*') {
                for (i++; def[i] && (def[i] != '/' || def[i - 1] != '*'); i++);
                return i;
        }

        /* C++ comments */
        if (def[i] == '/' && def[i + 1] == '/') {
                for (i++; def[i] && (def[i] != '\n' || def[i - 1] != '*'); i++);
                return i;
        }

        /* Blocks count as a single token */
        if (def[i] == '{') {
                for (j = 0; def[i] && def[i] != END_BRACE_MARK; i++) {
                        if (j < sizeof (tokens[0]) - 1)
                                tokens[d_num_tokens][j++] = def[i];
                }
                def[i] = '}';
                tokens[d_num_tokens++][j] = NUL;
                return i + 1;
        }

        /* String or character constant */
        if (def[i] == '"' || def[i] == '\'') {
                char start;

                tokens[d_num_tokens][0] = start = def[i];
                for (j = 1, ++i; def[i] && j < sizeof (tokens[0]) - 1; j++) {
                        tokens[d_num_tokens][j] = def[i++];
                        if (def[i] == start && def[i - 1] != '\\') {
                                i++;
                                break;
                        }
                }
                d_num_tokens++;
                return i;
        }

        /* Operator */
        if (D_is_operator(def[i])) {
                tokens[d_num_tokens][0] = def[i];
                tokens[d_num_tokens][1] = NUL;
                d_num_tokens++;
                return i + 1;
        }

        /* Identifier or constant */
        for (j = 0; j < sizeof (tokens[0]) - 1; j++) {
                if (D_is_operator(def[i]) || def[i] <= ' ')
                        break;
                tokens[d_num_tokens][j] = def[i++];
        }
        d_num_tokens++;
        return i;
}

/******************************************************************************\
 Tokenize the definition string.
\******************************************************************************/
static void tokenize_def(void)
{
        int pos, new_pos;

        memset(token_space, 0, sizeof (token_space));
        memset(tokens, 0, sizeof (tokens));
        for (d_num_tokens = 0, pos = 0; d_num_tokens < TOKENS; ) {
                new_pos = tokenize_def_from(pos);
                if (new_pos == pos)
                        break;
                pos = new_pos;
        }
}

/******************************************************************************\
 Returns the nth token in the definition.
\******************************************************************************/
const char *D_token(int n)
{
        if (n < 0 || n > TOKENS)
                return "";
        return tokens[n];
}

/******************************************************************************\
 Returns the amount of empty space before the nth token.
\******************************************************************************/
int D_token_space(int n)
{
        if (n < 0 || n > TOKENS)
                return 0;
        return token_space[n];
}

/******************************************************************************\
 Read in one character.
\******************************************************************************/
static void read_ch(void)
{
        if (cur_ch == EOF)
                return;
        last_ch = cur_ch;
        cur_ch = fgetc(file);
        if (cur_ch == '\n')
                last_nl = 0;
        else
                last_nl++;
}

/******************************************************************************\
 "Unread" one character.
\******************************************************************************/
static void unread_ch(void)
{
        cur_ch = last_ch;
        last_ch = NUL;
}

/******************************************************************************\
 Save comments out of the file.
\******************************************************************************/
static int parse_comment(void)
{
        if (cur_ch != '/')
                return FALSE;
        read_ch();
        if (cur_ch == '*') {
                int i, end, long_comment;

                while (cur_ch == '*')
                        read_ch();

                /* Style-guide long comments always end in backslash */
                if ((long_comment = cur_ch == '\\'))
                        read_ch();

                for (end = i = 0; i < sizeof (comment) && cur_ch != EOF; i++) {
                        if (cur_ch == '/' && last_ch == '*')
                                break;

                        /* Short comments need two spaces trimmed from the start
                           of every line to align correctly */
                        if (cur_ch == '\n' && !long_comment) {
                                comment[i] = '\n';
                                read_ch();
                                if (cur_ch == ' ') {
                                        read_ch();
                                        if (cur_ch == ' ')
                                                read_ch();
                                }
                                continue;
                        }

                        comment[i] = cur_ch;
                        if (cur_ch != '*' && cur_ch != '\\')
                                end = i;
                        read_ch();
                }
                comment[end] = NUL;
                read_ch();
                return TRUE;
        } else if (cur_ch == '/') {
                while (cur_ch != EOF && (cur_ch != '\n' || last_ch == '\\'))
                        read_ch();
                return TRUE;
        }
        unread_ch();
        return FALSE;
}

/******************************************************************************\
 Read in a special section of the definition. [match_last] can be NUL to
 disable.
\******************************************************************************/
static int parse_def_section(int i, char match_cur, char match_last,
                             int slashable)
{
        do {
                def[i++] = cur_ch;
                read_ch();
                if (last_ch == '\\' && slashable && cur_ch) {
                        def[i++] = cur_ch;
                        read_ch();
                        continue;
                }
                if (cur_ch == EOF)
                        return i;
        } while (i < sizeof (def) - 1 &&
                 (cur_ch != match_cur ||
                  (match_last && last_ch != match_last)));
        def[i] = cur_ch;
        read_ch();
        return i;
}

/******************************************************************************\
 Read in the next definition.
\******************************************************************************/
int D_parse_def(void)
{
        int i, braces, stop_define, stop_typedef;

        /* Skip spaces and pickup the last comment */
        for (read_ch(); ; ) {
                if (cur_ch == EOF)
                        return FALSE;
                if (cur_ch <= ' ')
                        read_ch();
                else if (!parse_comment())
                        break;
        }

        /* The definition may not be the first on the line, pad it */
        if (last_nl < sizeof (def) - 1)
                for (i = 0; i < last_nl - 1; i++)
                        def[i] = ' ';

        /* Read the entire definition */
        stop_define = cur_ch == '#';
        stop_typedef = FALSE;
        for (braces = 0; i < sizeof (def) - 1 && cur_ch != EOF; i++) {

                /* Skip strings */
                if (cur_ch == '"') {
                        i = parse_def_section(i, '"', NUL, TRUE);
                        continue;
                }

                /* Skip character constants */
                if (cur_ch == '\'') {
                        i = parse_def_section(i, '\'', NUL, TRUE);
                        continue;
                }

                /* Skip C comments */
                if (cur_ch == '*' && last_ch == '/') {
                        i = parse_def_section(i, '/', '*', NUL);
                        continue;
                }

                /* Skip C++ comments */
                if (cur_ch == '/' && last_ch == '/') {
                        i = parse_def_section(i, '\n', NUL, TRUE);
                        continue;
                }

                /* Track braces. If there is a newline before there are any
                   characters after the closing brace, assume this is a
                   function body and stop reading there.*/
                if (cur_ch == '{')
                        braces++;
                else if (cur_ch == '}') {
                        braces--;
                        if (braces <= 0) {
                                stop_typedef = TRUE;
                                def[i] = END_BRACE_MARK;
                                read_ch();
                                continue;
                        }
                }
                if ((stop_define || stop_typedef) &&
                    cur_ch == '\n' && last_ch != '\\')
                        break;
                if (cur_ch > ' ' && stop_typedef)
                        stop_typedef = FALSE;

                def[i] = cur_ch;
                if (braces <= 0 && cur_ch == ';') {
                        i++;
                        break;
                }
                read_ch();
        }
        def[i] = NUL;

        tokenize_def();
        return TRUE;
}

/******************************************************************************\
 Opens a file for parsing. Returns FALSE if the file failed to open.
\******************************************************************************/
int D_parse_open(const char *filename)
{
        file = fopen(filename, "r");
        if (!file)
                return FALSE;
        last_nl = 0;
        last_ch = NUL;
        cur_ch = NUL;
        return TRUE;
}

/******************************************************************************\
 Closes the current file.
\******************************************************************************/
void D_parse_close(void)
{
        fclose(file);
}

