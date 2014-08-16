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

/******************************************************************************\
 Searches for an entry.
\******************************************************************************/
entry_t *D_entry_find(entry_t *entry, const char *name)
{
        while (entry) {
                int cmp;

                cmp = strcmp(name, entry->name);
                if (cmp < 0)
                        break;
                if (!cmp)
                        return entry;
                entry = entry->next;
        }
        return NULL;
}

/******************************************************************************\
 Returns TRUE if the character is valid in a conventional macro name.
\******************************************************************************/
static int is_macro(char ch, int start)
{
        return ch == '_' || (ch >= 'A' && ch <= 'Z') ||
               (!start && ch >= '0' && ch <= '9');
}

/******************************************************************************\
 Updates an entry if it exists or creates a new one.
\******************************************************************************/
void D_entry_add(const entry_t *current, entry_t **root)
{
        entry_t *entry, *prev, *cur;
        int cmp;

        prev = NULL;
        cur = *root;
        while (cur) {
                cmp = strcmp(current->name, cur->name);
                if (cmp < 0)
                        break;
                else if (!cmp) {
                        entry_t *next;

                        next = cur->next;
                        *cur = *current;
                        cur->next = next;
                        return;
                }
                prev = cur;
                cur = cur->next;
        }
        if (!prev) {
                entry = calloc(sizeof (*entry), 1);
                *entry = *current;
                entry->next = *root;
                *root = entry;
                return;
        }
        entry = calloc(sizeof (*entry), 1);
        *entry = *current;
        entry->next = prev->next;
        prev->next = entry;
}

/******************************************************************************\
 Prints an HTML-formatted comment.
\******************************************************************************/
static void output_comment(const char *buf)
{
        int i, j, last_nl, blank_nl, spaces, in_p;
        enum {
                CODE_NONE,
                CODE_BRACKETS,
                CODE_MACRO,
                CODE_DIAGRAM,
        } code;

        if (!buf[0])
                return;
        in_p = FALSE;
        blank_nl = TRUE;
        code = CODE_NONE;
        for (last_nl = -100, spaces = i = 0; buf[i]; i++) {

                /* Convert newlines to tags */
                if (buf[i] == '\n') {
                        blank_nl = spaces >= 0;
                        if (blank_nl && in_p) {
                                fprintf(d_file, "</p>");
                                in_p = FALSE;
                        }
                        if (code == CODE_DIAGRAM) {
                                fprintf(d_file, "</code><br>");
                                code = CODE_NONE;
                                blank_nl = TRUE;
                        } else if (i - last_nl < 64 && in_p)
                                fprintf(d_file, "<br>");
                        last_nl = i;
                        spaces = 0;
                } else if (buf[i] == ' ' || buf[i] == '\t') {
                        if (spaces >= 0)
                                spaces++;
                } else {
                        if (!in_p) {
                                fprintf(d_file, "<p class=\"comment\">");
                                in_p = TRUE;
                        }

                        /* Codify diagrams */
                        if (spaces >= 2 && blank_nl) {
                                fprintf(d_file, "<code>  ");
                                code = CODE_DIAGRAM;
                        }

                        spaces = -1;
                }

                /* Links need to be processed separately */
                if (!strncmp(buf + i, "http://", 7)) {
                        char link[256];

                        for (j = 0; j < sizeof (link) - 1 &&
                             buf[i + j] && buf[i + j] > ' '; j++)
                                link[j] = buf[i + j];
                        link[j] = NUL;
                        fprintf(d_file, "<a class=\"out\" href=\"%s\">%s</a>",
                                link, link);
                        i += j - 1;
                        continue;
                }

                /* Codify anything specifically tagged */
                if (buf[i] == '[' && !code) {
                        fprintf(d_file, "<code>");
                        code = CODE_BRACKETS;
                        continue;
                }
                if (buf[i] == ']' && code == CODE_BRACKETS) {
                        fprintf(d_file, "</code>");
                        code = CODE_NONE;
                        continue;
                }

                /* Codify any potential macro references */
                if (!code && is_macro(buf[i], TRUE)) {
                        for (j = i + 1; buf[j] && is_macro(buf[j], FALSE); j++);
                        if (j - i > 2) {
                                fprintf(d_file, "<code>");
                                code = CODE_MACRO;
                        }
                } else if (code == CODE_MACRO && !is_macro(buf[i], FALSE)) {
                        fprintf(d_file, "</code>");
                        code = CODE_NONE;
                }

                fputc(buf[i], d_file);
        }
        if (code)
                fprintf(d_file, "</code>");
        if (in_p)
                fprintf(d_file, "</p>\n");
}

/******************************************************************************\
 Prints an HTML-formatted definition.
\******************************************************************************/
static void output_def(const char *name, const char *buf)
{
        enum {
                NONE = 0,
                OPERATOR,
                C_COMMENT,
                CPP_COMMENT,
                NUMERIC,
                STRING,
                CHARACTER,
        } mode;
        int i, start_token, escaped, mode_i;

        if (!buf[0])
                return;
        fprintf(d_file, "<pre>");
        start_token = TRUE;
        escaped = FALSE;
        mode = NONE;
        for (i = 0; buf[i]; i++) {
                int end;

                /* Keywords and known types */
                if (start_token) {
                        entry_t *entry;
                        int next_i, type;

                        next_i = D_is_keyword(buf + i, &entry, &type);
                        if (next_i && (!entry || strcmp(entry->name, name))) {
                                next_i += i;
                                if (entry)
                                        fprintf(d_file, "<a href=\"#%s\">",
                                                entry->name);
                                else
                                        fprintf(d_file,
                                                "<b class=\"keyword%d\">",
                                                type);
                                while (i < next_i)
                                        fputc(buf[i++], d_file);
                                fprintf(d_file, entry ? "</a>" : "</b>");
                                start_token = FALSE;
                                i--;
                                continue;
                        }
                }

                /* End current mode before starting a new one */
                switch (mode) {
                case C_COMMENT:
                        end = buf[i - 1] == '/' && buf[i - 2] == '*';
                        break;
                case CPP_COMMENT:
                        end = buf[i] == '\n';
                        break;
                case NUMERIC:
                        end = buf[i] <= ' ' ||
                              (buf[i] != '.' && D_is_operator(buf[i]));
                        break;
                case STRING:
                        end = !escaped && buf[i - 1] == '"' && i - mode_i > 1;
                        break;
                case CHARACTER:
                        end = !escaped && buf[i - 1] == '\'' && i - mode_i > 1;
                        break;
                case OPERATOR:
                        end = !D_is_operator(buf[i]);
                        break;
                default:
                        end = FALSE;
                        break;
                }
                if (end) {
                        fprintf(d_file, "</span>");
                        mode = NONE;
                }

                /* C comments */
                if (!mode && buf[i] == '/' && buf[i + 1] == '*') {
                        fprintf(d_file, "<span class=\"def_comment\">");
                        mode = C_COMMENT;
                }

                /* C++ comments */
                if (!mode && buf[i] == '/' && buf[i + 1] == '/') {
                        fprintf(d_file, "<span class=\"def_comment\">");
                        mode = CPP_COMMENT;
                }

                /* Numeric constants */
                if (!mode && start_token && buf[i] >= '0' && buf[i] <= '9') {
                        fprintf(d_file, "<span class=\"def_const\">");
                        mode = NUMERIC;
                }

                /* Strings */
                if (!mode && buf[i] == '"') {
                        fprintf(d_file, "<span class=\"def_const\">");
                        mode = STRING;
                        mode_i = i;
                }

                /* Characters */
                if (!mode && buf[i] == '\'') {
                        fprintf(d_file, "<span class=\"def_const\">");
                        mode = CHARACTER;
                        mode_i = i;
                }

                /* Operators */
                if (!mode && D_is_operator(buf[i])) {
                        fprintf(d_file, "<span class=\"def_op\">");
                        mode = OPERATOR;
                }

                fputc(buf[i], d_file);
                start_token = mode == OPERATOR || buf[i] <= ' ';
                escaped = !escaped && buf[i] == '\\';
        }
        if (mode)
                fprintf(d_file, "</span>");
        fprintf(d_file, "</pre>\n");
}

/******************************************************************************\
 Outputs the HTML-formatted path.
\******************************************************************************/
void output_path(const char *path)
{
        int i, len;

        if (!path[0])
                return;
        if (path[0] < 0) {
                fprintf(d_file, "<span class=\"no_file\">(not found)</span>");
                return;
        }
        len = (int)strlen(path);
        for (i = len - 1; i > 0 && path[i] != '/'; i--);
        if (i > 0)
                i++;
        fprintf(d_file, "<span class=\"file\">(%s)</span>", path + i);
}

/******************************************************************************\
 Prints an entry linked list.
\******************************************************************************/
void D_output_entries(entry_t *entry)
{
        static int entries;

        while (entry) {
                fprintf(d_file,
                        "<a name=\"%s\" href=\"javascript:toggle(%d)\">\n"
                        "<div class=\"entry\" id=\"%d\">\n"
                        "%s ", entry->name, entries, entries, entry->name);
                output_path(entry->file);
                fprintf(d_file,
                        "</div>\n"
                        "</a>\n"
                        "<div class=\"entry_body\" id=\"%d_body\" "
                             "style=\"display:none\">\n", entries);
                output_def(entry->name, entry->def);
                output_comment(entry->comment);
                fprintf(d_file, "</div>\n");
                entries++;
                entry = entry->next;
        }
}

