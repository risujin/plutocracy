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

entry_t *d_types;
FILE *d_file;

static entry_t *functions, *defines, *variables;

/******************************************************************************\
 Returns TRUE if [token] is a type specifier.
\******************************************************************************/
static int is_type(const char *token)
{
        const char **type, *types[] = {"signed", "unsigned", "char", "int",
                                       "short", "long", "float", "single",
                                       "double", "size_t", "const", NULL};

        for (type = types; *type; type++)
                if (!strcmp(token, *type))
                        return TRUE;
        if (D_entry_find(d_types, token))
                return TRUE;
        return FALSE;
}

/******************************************************************************\
 Parse a C header file for declarations.
\******************************************************************************/
static void parse_header(const char *filename)
{
        entry_t current;
        int i;

        while (D_parse_def()) {
                memset(&current, 0, sizeof (current));

                /* extern variables */
                if (!strcmp(D_token(0), "extern") &&
                    D_token(d_num_tokens - 2)[0] != ')') {
                        D_strncpy_buf(current.file, filename);
                        D_strncpy_buf(current.def, D_def());
                        D_strncpy_buf(current.comment, D_comment());
                        for (i = 2; i < d_num_tokens - 1; i++) {
                                if (D_token(i)[0] == '*' ||
                                    D_token(i)[0] == ',' ||
                                    is_type(D_token(i)))
                                        continue;
                                if (D_token(i)[0] == '[') {
                                        for (; i < d_num_tokens - 1; i++)
                                                if (D_token(i)[0] == ']')
                                                        break;
                                        continue;
                                }
                                current.file[0] = -1;
                                D_strncpy_buf(current.name, D_token(i));
                                D_entry_add(&current, &variables);
                        }
                        continue;
                }

                /* #define and macro functions */
                if (D_token(0)[0] == '#' && !strcmp(D_token(1), "define")) {
                        entry_t **root;

                        D_strncpy_buf(current.file, filename);
                        D_strncpy_buf(current.name, D_token(2));
                        D_strncpy_buf(current.def, D_def());
                        D_strncpy_buf(current.comment, D_comment());
                        root = &defines;
                        if (D_token(3)[0] == '(' && !D_token_space(3))
                                root = &functions;
                        D_entry_add(&current, root);
                        continue;
                }

                /* struct or enum */
                if (!strcmp(D_token(0), "struct") ||
                    !strcmp(D_token(0), "enum")) {
                        D_strncpy_buf(current.file, filename);
                        D_strncpy_buf(current.name, D_token(1));
                        D_strncpy_buf(current.def, D_def());
                        D_strncpy_buf(current.comment, D_comment());
                        D_entry_add(&current, &d_types);
                        continue;
                }

                /* typedef */
                if (!strcmp(D_token(0), "typedef")) {
                        D_strncpy_buf(current.file, filename);
                        D_strncpy_buf(current.name, D_token(d_num_tokens - 2));

                        /* Function pointer */
                        if (D_token(d_num_tokens - 2)[0] == ')')
                                for (i = 1; i < d_num_tokens - 2; i++)
                                        if (D_token(i)[0] == ')')
                                                D_strncpy_buf(current.name,
                                                              D_token(i - 1));

                        D_strncpy_buf(current.def, D_def());
                        D_strncpy_buf(current.comment, D_comment());
                        D_entry_add(&current, &d_types);
                        continue;
                }

                /* Function */
                if (D_token(d_num_tokens - 2)[0] == ')') {
                        for (i = 2; i < d_num_tokens; i++)
                                if (D_token(i)[0] == '(')
                                        break;
                        if (i >= d_num_tokens)
                                continue;

                        /* Unless this is an inline function, we need to find it
                           in a source file for the full comment */
                        if (strcmp(D_token(0), "static") &&
                            strcmp(D_token(0), "inline"))
                                current.file[0] = -1;
                        else
                                D_strncpy_buf(current.file, filename);

                        D_strncpy_buf(current.name, D_token(i - 1));
                        D_strncpy_buf(current.def, D_def());
                        D_strncpy_buf(current.comment, D_comment());
                        D_entry_add(&current, &functions);
                        continue;
                }
        }
}

/******************************************************************************\
 Parse a C source file for comments. This is only done to pick up source file
 comments which are assumed to be more informative.
\******************************************************************************/
static void parse_source(const char *filename)
{
        entry_t *entry;
        int i;

        while (D_parse_def()) {

                /* Must be a non-static function body or variable list */
                if (!strcmp(D_token(0), "static") ||
                    !strcmp(D_token(0), "enum") ||
                    !strcmp(D_token(0), "typedef") ||
                    !strcmp(D_token(0), "struct") ||
                    !strcmp(D_token(0), "extern") ||
                    D_token(0)[0] == '#')
                        continue;

                /* Function */
                if (D_token(d_num_tokens - 1)[0] == '{') {

                        /* Find the function name */
                        for (i = 2; i < d_num_tokens; i++)
                                if (D_token(i)[0] == '(')
                                        break;
                        if (i >= d_num_tokens)
                                continue;
                        entry = D_entry_find(functions, D_token(i - 1));
                        if (!entry)
                                continue;

                        /* Update the comment */
                        D_strncpy_buf(entry->comment, D_comment());
                        D_strncpy_buf(entry->file, filename);
                        continue;
                }

                /* Variable list */
                for (i = 1; i < d_num_tokens - 1; i++) {
                        if (D_token(i)[0] == '=') {
                                i++;
                                continue;
                        }
                        if (D_token(i)[0] == '*' || D_token(i)[0] == ',' ||
                            D_token(i)[0] == '{' || is_type(D_token(i)))
                                continue;
                        if (D_token(i)[0] == '[') {
                                for (; i < d_num_tokens - 1; i++)
                                        if (D_token(i)[0] == ']')
                                                break;
                                continue;
                        }
                        entry = D_entry_find(variables, D_token(i));
                        if (!entry)
                                continue;

                        /* Update the comment */
                        D_strncpy_buf(entry->comment, D_comment());
                        D_strncpy_buf(entry->file, filename);
                }
        }
}

/******************************************************************************\
 Parse an input file.
\******************************************************************************/
static int parse_file(const char *filename)
{
        size_t len;

        if (!D_parse_open(filename)) {
                fprintf(stderr, "gendoc: Failed to open '%s' for reading!\n",
                        filename);
                return 1;
        }
        len = strlen(filename);
        if (len > 2 && filename[len - 2] == '.' && filename[len - 1] == 'h')
                parse_header(filename);
        else if (len > 2 && filename[len - 2] == '.' &&
                 filename[len - 1] == 'c')
                parse_source(filename);
        else {
                fprintf(stderr, "gendoc: Unrecognized extension in '%s'.\n",
                        filename);
                return 1;
        }
        D_parse_close();
        return 0;
}

/******************************************************************************\
 Reads in and outputs the header file.
\******************************************************************************/
static void output_header(const char *filename)
{
        FILE *file;
        char buf[4096];

        if (!filename || !filename[0])
                return;
        file = fopen(filename, "r");
        if (!file) {
                fprintf(stderr, "gendoc: Failed to open header '%s'\n",
                        filename);
                return;
        }
        fread(buf, 1, sizeof (buf), file);
        fclose(file);
        fputs(buf, d_file);
}

/******************************************************************************\
 Outputs HTML document.
\******************************************************************************/
static void output_html(const char *title, const char *header)
{
        /* Header */
        fprintf(d_file,
                "<html>\n"
                "<head>\n"
                "<link rel=StyleSheet href=\"gendoc.css\" type=\"text/css\">\n"
                "<title>%s</title>\n"
                "</head>\n"
                "<script>\n"
                "         function toggle(n) {\n"
                "                 body = document.getElementById(n + "
                                                                "'_body');\n"
                "                 if (body.style.display == 'block')\n"
                "                         body.style.display = 'none';\n"
                "                 else\n"
                "                         body.style.display = 'block';\n"
                "         }\n"
                "</script>\n"
                "<body>\n", title);
        output_header(header);
        fprintf(d_file,
               "<div class=\"menu\">\n"
               "<a href=\"#Definitions\">Definitions</a> |\n"
               "<a href=\"#Types\">Types</a> |\n"
               "<a href=\"#Functions\">Functions</a> |\n"
               "<a href=\"#Variables\">Variables</a>\n"
               "</div>\n"
               "<h1>%s</h1>\n", title);

        /* Write definitions */
        fprintf(d_file, "<a name=\"Definitions\"></a>\n"
                        "<h2>Definitions</h2>\n");
        D_output_entries(defines);

        /* Write d_types */
        fprintf(d_file, "<a name=\"Types\"></a>\n"
                        "<h2>Types</h2>\n");
        D_output_entries(d_types);

        /* Write functions */
        fprintf(d_file, "<a name=\"Functions\"></a>\n"
                        "<h2>Functions</h2>\n");
        D_output_entries(functions);

        /* Write variables */
        fprintf(d_file, "<a name=\"Variables\"></a>\n"
                        "<h2>Variables</h2>\n");
        D_output_entries(variables);

        /* Footer */
        fprintf(d_file, "</body>\n</html>\n");
}

/******************************************************************************\
 Entry point. Prints out help directions.

 TODO: Add a --output=... argument to output to a file instead of stdout.
\******************************************************************************/
int main(int argc, char *argv[])
{
        int i;
        char title[64] = "Plutocracy GenDoc", header[256] = "";

        /* Somebody didn't read directions */
        if (argc < 2) {
                fprintf(stderr,
                        "Usage: gendoc [--title TITLE] [--file OUTPUT] "
                                      "[--header FILE] [input headers] "
                                      "[input sources]\n"
                        "You shouldn't need to run this program directly, "
                        "running 'scons gendoc' from the root directory "
                        "should automatically run GenDoc with the correct "
                        "arguments.\n");
                return 1;
        }

        /* Process input files */
        d_file = stdout;
        for (i = 1; i < argc; i++) {
                if (argv[i][0] != '-' || argv[i][1] != '-') {
                        if (parse_file(argv[i]))
                                return 1;
                        continue;
                }

                /* Flags */
                if (!strcmp(argv[i], "--title") && i < argc - 1)
                        D_strncpy_buf(title, argv[++i]);
                else if (!strcmp(argv[i], "--file") && i < argc - 1) {
                        d_file = fopen(argv[++i], "w");
                        if (!d_file) {
                                fprintf(stderr,
                                        "Failed to open output file %s",
                                        argv[i]);
                                return 1;
                        }
                } else if (!strcmp(argv[i], "--header") && i < argc - 1)
                        D_strncpy_buf(header, argv[++i]);
                else {
                        fprintf(stderr, "Invalid argument '%s'\n",
                                argv[i]);
                        return 1;
                }
        }

        /* Output HTML document */
        output_html(title, header);

        return 0;
}

