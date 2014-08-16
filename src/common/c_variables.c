/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* This file contains the common configurable variables and the framework
   for handling configurable variables system-wide. Variables are not set
   or referred to by their names often, so efficiency here is not important. */

#include "c_shared.h"

/* Message logging */
c_var_t c_log_level, c_log_file, c_log_throttle;

/* Never burn the user's CPU, if we are running through frames too quickly
   (faster than this rate), we need to take a nap */
c_var_t c_max_fps, c_show_fps;

/* We can do some detailed allocated memory tracking and detect double-free,
   memory under/overrun, and leaks on the fly. This variable cannot be changed
   after initilization! */
c_var_t c_mem_check;

/* Language that should be used for translations, must restart game to apply
   changes */
c_var_t c_lang;

/* Variable that can be used to input information from the game interface to
   a debugging function */
c_var_t c_test_int;

/* Variable to control whether or not to show the bytes-per-second meter*/
c_var_t c_show_bps;

/* This should be set to TRUE when the main loop needs to exit properly */
int c_exit;

static c_var_t *root;

/******************************************************************************\
 Registers the common configurable variables.
\******************************************************************************/
void C_register_variables(void)
{
        /* Message logging */
        C_register_integer(&c_log_level, "c_log_level", C_LOG_WARNING,
                           "log detail level: 0 = disable, to 4 = traces");
        c_log_level.edit = C_VE_ANYTIME;
        C_register_string(&c_log_file, "c_log_file", "",
                          "filename to redirect log output to");
        C_register_integer(&c_log_throttle, "c_log_throttle", 25,
                           "maximum logs per second, 0 to disable");
        c_log_throttle.edit = C_VE_ANYTIME;

        /* FPS cap */
        C_register_integer(&c_max_fps, "c_max_fps", 120,
                           "software frames-per-second limit");
        c_max_fps.edit = C_VE_ANYTIME;
        C_register_integer(&c_show_fps, "c_show_fps", FALSE,
                           "enable to display current frames-per-second");
        c_show_fps.edit = C_VE_ANYTIME;

        /* Memory checking */
        C_register_integer(&c_mem_check, "c_mem_check", FALSE,
                           "enable to debug memory allocations");

        /* Language selection */
        C_register_string(&c_lang, "c_lang", "",
                          "language used for all text in the game");

        /* Testing variables */
        C_register_integer(&c_test_int, "c_test_int", 0,
                           "integer value used for testing");
        c_test_int.archive = FALSE;
        c_test_int.edit = C_VE_ANYTIME;
        /* Bytes-per-second meter */
        C_register_integer(&c_show_bps, "c_show_bps", FALSE,
                           "enable to display current bytes-per-second");
        c_show_bps.edit = C_VE_ANYTIME;
}

/******************************************************************************\
 This function will register a static configurable variable. The data is stored
 in the c_var_t [var], which is guaranteed to always have the specified
 [type] and can be addressed using [name] from within a configuration string.
 Note that [name] must be static and cannot be deallocated! The variable is
 assigned [value].

 These variables cannot be deallocated or have any member other than the value
 modified. The variables are tracked as a linked list.
\******************************************************************************/
static void var_register(c_var_t *var, const char *name, c_var_type_t type,
                         c_var_value_t value, const char *comment)
{
        c_var_t *pos, *prev;

        if (var->type)
                C_error("Attempted to re-register '%s' with '%s'",
                        var->name, name);
        var->type = type;
        var->name = name;
        var->comment = comment;
        var->stock = var->latched = var->value = value;
        var->edit = C_VE_LATCHED;
        var->archive = TRUE;
        var->changed = -1;

        /* Attach the var to the linked list, sorted alphabetically */
        prev = NULL;
        pos = root;
        while (pos && strcasecmp(var->name, pos->name) > 0) {
                prev = pos;
                pos = pos->next;
        }
        var->next = pos;
        if (prev)
                prev->next = var;
        if (pos == root)
                root = var;
}

void C_register_float(c_var_t *var, const char *name, float value_f,
                      const char *comment)
{
        c_var_value_t value;

        value.f = value_f;
        var_register(var, name, C_VT_FLOAT, value, comment);
}

void C_register_integer(c_var_t *var, const char *name, int value_n,
                        const char *comment)
{
        c_var_value_t value;

        value.n = value_n;
        var_register(var, name, C_VT_INTEGER, value, comment);
}

void C_register_string(c_var_t *var, const char *name, const char *value_s,
                       const char *comment)
{
        c_var_value_t value;

        C_strncpy_buf(value.s, value_s);
        var_register(var, name, C_VT_STRING, value, comment);
}

/******************************************************************************\
 Tries to find variable [name] (case insensitive) in the variable linked list.
 Returns NULL if it fails.
\******************************************************************************/
c_var_t *C_resolve_var(const char *name)
{
        c_var_t *var;

        var = root;
        while (var)
        {
                if (!strcasecmp(var->name, name))
                        return var;
                var = var->next;
        }
        return NULL;
}

/******************************************************************************\
 Parses and sets a variable's value according to its type. After this function
 returns [value] can be safely deallocated.
\******************************************************************************/
void C_var_set(c_var_t *var, const char *string)
{
        c_var_value_t new_value;
        const char *set_string;

        if (var->edit == C_VE_LOCKED) {
                C_warning("Cannot set '%s' to '%s', variable is locked",
                          var->name, string);
                return;
        }
        if (var->edit != C_VE_LATCHED)
                var->has_latched = FALSE;

        /* Get the new value and see if it changed */
        switch (var->type) {
        case C_VT_INTEGER:
                new_value.n = atoi(string);
                if (!C_is_digit(string[0]) &&
                    (!strcasecmp(string, "yes") || !strcasecmp(string, "true")))
                        new_value.n = 1;
                if (var->has_latched && new_value.n == var->latched.n)
                        return;
                if (new_value.n == var->value.n) {
                        if (var->has_latched) {
                                C_debug("Variable '%s' unlatched %d",
                                        var->name, var->latched.n);
                                var->has_latched = FALSE;
                        }
                        return;
                }
                break;
        case C_VT_FLOAT:
                new_value.f = (float)atof(string);
                if (var->has_latched && new_value.f == var->latched.f)
                        return;
                if (new_value.f == var->value.f) {
                        if (var->has_latched) {
                                C_debug("Variable '%s' unlatched %g",
                                        var->name, var->latched.f);
                                var->has_latched = FALSE;
                        }
                        return;
                }
                break;
        case C_VT_STRING:
                C_strncpy_buf(new_value.s, string);
                if (var->has_latched && !strcmp(var->latched.s, string))
                        return;
                if (!strcmp(var->value.s, string)) {
                        if (var->has_latched) {
                                C_debug("Variable '%s' unlatched '%s'",
                                        var->name, var->latched.s);
                                var->has_latched = FALSE;
                        }
                        return;
                }
                break;
        default:
                C_error("Variable '%s' is uninitialized", var->name);
        }

        /* If this variable is controlled by an update function, call it now */
        if (var->edit == C_VE_FUNCTION) {
                if (!var->update)
                        C_error("Update function not set for '%s'", var->name);
                if (!var->update(var, new_value))
                        return;
        }

        /* Set the variable's value */
        set_string = "set to";
        if (var->edit == C_VE_LATCHED) {
                var->has_latched = TRUE;
                var->latched = new_value;
                set_string = "latched";
        } else {
                var->value = new_value;
                var->changed = c_frame;
        }
        switch (var->type) {
        case C_VT_INTEGER:
                C_debug("Integer '%s' %s %d", var->name,
                        set_string, new_value.n);
                break;
        case C_VT_FLOAT:
                C_debug("Float '%s' %s %g", var->name, set_string, new_value.f);
                break;
        case C_VT_STRING:
                C_debug("String '%s' %s '%s'", var->name, set_string, string);
        default:
                break;
        }
}

/******************************************************************************\
 If the variable has a latched value, propagate it to the actual value.
\******************************************************************************/
bool C_var_unlatch(c_var_t *var)
{
        if (var->type == C_VT_UNREGISTERED)
                C_error("Tried to unlatch an unregistered variable: %s",
                        var->name);
        if (!var->has_latched || var->edit != C_VE_LATCHED)
                return FALSE;
        var->value = var->latched;
        var->has_latched = FALSE;
        var->changed = c_frame;
        return TRUE;
}

/******************************************************************************\
 Prints out the current and latched values of a variable.
\******************************************************************************/
static void print_var(const c_var_t *var)
{
        const char *value, *latched;

        switch (var->type) {
        case C_VT_INTEGER:
                value = C_va("Integer '%s' is %d (%s)",
                             var->name, var->value.n, var->comment);
                break;
        case C_VT_FLOAT:
                value = C_va("Float '%s' is %g (%s)",
                             var->name, var->value.f, var->comment);
                break;
        case C_VT_STRING:
                value = C_va("String '%s' is '%s' (%s)",
                             var->name, var->value.s, var->comment);
                break;
        default:
                C_error("Tried to print out unregistered variable");
        }
        latched = "";
        if (var->edit == C_VE_LATCHED && var->has_latched) {
                switch (var->type) {
                case C_VT_INTEGER:
                        latched = C_va(" (%d latched)", var->latched.n);
                        break;
                case C_VT_FLOAT:
                        latched = C_va(" (%g latched)", var->latched.f);
                        break;
                case C_VT_STRING:
                        latched = C_va(" ('%s' latched)", var->latched.s);
                default:
                        break;
                }
        }
        C_print(C_va("%s%s", value, latched));
}

/******************************************************************************\
 Callback for parsing a configuration file key-value pair.
\******************************************************************************/
static int config_key_value(const char *key, const char *value)
{
        gchar *cleaned_value;
        c_var_t *var;

        var = C_resolve_var(key);
        if (!var) {
                C_print(C_va("No variable named '%s'", key));
                return TRUE;
        }

        if (value) {
                cleaned_value = NULL;
                pango_parse_markup(value, strlen(value), 0, NULL,
                                   &cleaned_value, NULL, NULL);
                if(cleaned_value)
                        C_var_set(var, cleaned_value);
                g_free(cleaned_value);
        }
        else
                print_var(var);
        return TRUE;
}

/******************************************************************************\
 Parses a configuration file.
\******************************************************************************/
int C_parse_config_file(const char *filename)
{
        c_token_file_t tf;

        if (!C_token_file_init(&tf, filename))
                return FALSE;
        C_token_file_parse_pairs(&tf, config_key_value);
        C_token_file_cleanup(&tf);
        return TRUE;
}

/******************************************************************************\
 Parses a configuration file from a string.
\******************************************************************************/
void C_parse_config_string(const char *string)
{
        c_token_file_t tf;

        C_token_file_init_string(&tf, string);
        C_token_file_parse_pairs(&tf, config_key_value);
        C_token_file_cleanup(&tf);
}

/******************************************************************************\
 Writes a configuration file that contains the values of all modified,
 archivable variables.
\******************************************************************************/
void C_write_autogen(void)
{
        c_file_t file;
        c_var_t *var;
        const char *value;

        if (!C_file_init_write(&file, C_va("%s/autogen.cfg", C_user_dir()))) {
                C_warning("Failed to save variable config");
                return;
        }
        C_file_printf(&file, "/***************************************"
                             "***************************************\\\n"
                             " %s - Automatically generated config\n"
                             "\\**************************************"
                             "****************************************/\n",
                             PACKAGE_STRING);
        for (var = root; var; var = var->next) {
                if (!var->archive)
                        continue;

                /* Get the variable's value */
                C_var_unlatch(var);
                value = NULL;
                switch (var->type) {
                case C_VT_INTEGER:
                        if (var->value.n == var->stock.n)
                                continue;
                        value = C_va("%d", var->value.n);
                        break;
                case C_VT_FLOAT:
                        if (var->value.f == var->stock.f)
                                continue;
                        value = C_va("%g", var->value.f);
                        break;
                case C_VT_STRING:
                        if (!strcmp(var->value.s, var->stock.s))
                                continue;
                        value = C_escape_string(var->value.s);
                        break;
                default:
                        C_error("Unregistered variable in list");
                }

                /* Print the comment and key-value pair */
                C_file_printf(&file, "\n/* %s */\n%s %s\n",
                              var->comment ? var->comment : "",
                              var->name, value);
        }
        C_file_printf(&file, "\n");
        C_file_cleanup(&file);
        C_debug("Saved autogen config");
}

/******************************************************************************\
 Find all the cvars that start with [str] a string containing any additional
 characters that are shared by all candidates. If there is more than one
 match, the possible matches are printed to the console.
\******************************************************************************/
const char *C_auto_complete_vars(const char *str)
{
        static char buf[128];
        c_var_t *var, *matches[100];
        int i, j, str_len, matches_len, common;

        /* Find all cvars that match [str] */
        str_len = C_strlen(str);
        matches_len = 0;
        for (var = root; var; var = var->next)
                if (!strncasecmp(var->name, str, str_len)) {
                        matches[matches_len++] = var;
                        if (matches_len >= sizeof (matches) / sizeof (*matches))
                                break;
                }
        if (matches_len < 1)
                return "";
        if (matches_len < 2) {
                C_strncpy_buf(buf, matches[0]->name + str_len);
                str_len = C_strlen(buf);
                if (str_len > sizeof (buf) - 2)
                        str_len = sizeof (buf) - 2;
                buf[str_len] = ' ';
                buf[str_len + 1] = NUL;
                return buf;
        }

        /* Check for a longer common root */
        common = C_strlen(matches[0]->name);
        for (i = 1; i < matches_len; i++) {
                for (j = str_len; matches[i]->name[j] == matches[0]->name[j];
                     j++);
                if (j < common)
                        common = j;
        }
        memcpy(buf, matches[0]->name + str_len, common - str_len);
        buf[common - str_len] = NUL;

        /* Output all of the matched vars to the console */
        C_print(C_va("\n%d matches:", matches_len));
        for (i = 0; i < matches_len; i++)
                C_print(C_va("    %s  (%s)", matches[i]->name,
                             matches[i]->comment));

        return buf;
}

/******************************************************************************\
 A convenience function that changes [var]'s edit mode to C_VE_FUNCTION, sets
 its update function, and finally calls the update function on the current
 value of the variable.
\******************************************************************************/
void C_var_update_data(c_var_t *var, c_var_update_f update, void *data)
{
        C_var_unlatch(var);
        var->edit = C_VE_FUNCTION;
        var->update = update;
        var->update_data = data;
        update(var, var->value);
}

/******************************************************************************\
 A generic variable update function that updates a color from a string.
\******************************************************************************/
int C_color_update(c_var_t *var, c_var_value_t value)
{
        *(c_color_t *)var->update_data = C_color_string(value.s);
        return TRUE;
}

/******************************************************************************\
 Translate all the comments for the variables. This was not done during
 registration because we could not read the [c_lang] var yet.
\******************************************************************************/
void C_translate_vars(void)
{
        c_var_t *var;
        int total;

        for (var = root, total = 0; var; var = var->next)
                if (var->comment && var->comment[0]) {
                        var->comment = C_str(C_va("%s-comment", var->name),
                                                  var->comment);
                        total++;
                }
        C_debug("%d registered variables", total);
}

/******************************************************************************\
 Unsafe variable settings may be preventing the program from starting. Reset
 them to the defaults that are most likely to work. Variable edit rules are
 ignored and update functions are not called.
\******************************************************************************/
void C_reset_unsafe_vars(void)
{
        c_var_t *var;

        for (var = root; var; var = var->next)
                if (var->unsafe) {
                        var->value = var->latched = var->stock;
                        var->has_latched = FALSE;
                        C_debug("Reset unsafe variable '%s'", var->name);
                }
}

