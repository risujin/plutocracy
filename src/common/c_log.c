/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* This file implements the logging system */

#include "c_shared.h"

/* Size of the print buffer */
#define BUFFER_SIZE 640

extern c_var_t c_log_level, c_log_file, c_log_throttle;

c_log_event_f c_log_func;
c_log_mode_t c_log_mode;

static c_file_t log_file;

/******************************************************************************\
 Close the log file to conserve file handles.
\******************************************************************************/
void C_close_log_file(void)
{
        if (!log_file.type)
                return;
        C_debug("Closing log-file");
        C_file_cleanup(&log_file);
}

/******************************************************************************\
 Opens the log file or falls back to standard error output.
\******************************************************************************/
void C_open_log_file(void)
{
        C_var_unlatch(&c_log_file);
        if (!c_log_file.value.s[0])
                return;
        if (C_file_init_write(&log_file, c_log_file.value.s))
                C_debug("Opened log file '%s'", c_log_file.value.s);
        else
                C_warning("Failed to open log file '%s'",
                          c_log_file.value.s);
}

/******************************************************************************\
 Prints text to the GUI handler.
\******************************************************************************/
void C_print(const char *string)
{
        if (!c_log_func || c_log_mode != C_LM_NORMAL)
                return;
        c_log_mode = C_LM_NO_FUNC;
        c_log_func(C_LOG_PRINT, 0, string);
        if (c_log_mode == C_LM_NO_FUNC)
                c_log_mode = C_LM_NORMAL;
}

/******************************************************************************\
 Wraps log-formatted text. [margin] amount of padding is added to each line
 following the first. If [plen] is not NULL, it is set to the length of the
 returned string.
\******************************************************************************/
char *C_wrap_log(const char *src, int margin, int wrap, int *plen)
{
        static char dest[320];
        int i, j, k, last_break, last_line, cols, char_len;

        /* Make sure the margin and wrap settings are sane */
        if (wrap < 20)
                wrap = 20;
        if (margin > wrap / 2)
                margin = 4;

        /* Take care of leading newlines here to prevent padding them */
        for (j = i = 0; src[i] == '\n'; i++)
                dest[j++] = '\n';

        for (last_break = last_line = cols = 0; ; i += char_len, cols++) {
                if (!(char_len = C_utf8_append(dest, &j, sizeof (dest) - 2,
                                               src + i)))
                        break;

                /* Track the last breakable spot */
                if (src[i] == ' ' || src[i] == '\t' || src[i] == '-' ||
                    src[i] == '/' || src[i] == '\\')
                        last_break = i;
                if (src[i] == '\n') {
                        last_break = i;
                        j--;
                }

                /* Wrap text and pad up until the margin length */
                if (cols >= wrap || src[i] == '\n') {
                        if (last_break == last_line)
                                last_break = i;
                        j -= i - last_break;
                        last_line = i = last_break;
                        if (j >= (int)sizeof (dest) - margin - 3)
                                break;
                        dest[j++] = '\n';
                        dest[j++] = ':';
                        for (k = 0; k < margin - 1; k++)
                                dest[j++] = ' ';
                        cols = margin;
                }
        }
        dest[j++] = '\n';
        dest[j] = NUL;
        if (plen)
                *plen = j;
        return dest;
}

/******************************************************************************\
 Outputs a log entry.
\******************************************************************************/
static void log_output(c_log_level_t level, int margin, const char *buffer)
{
        const char *wrapped;
        int len;

        /* Wrap the text and print it to console or log file */
        wrapped = C_wrap_log(buffer, margin, C_LOG_WRAP_COLS, &len);
        if (log_file.type)
                C_file_write(&log_file, wrapped, len);
        else
                fputs(wrapped, stdout);

        /* Errors */
        if (level == C_LOG_ERROR) {
#ifdef WIN32
                /* Display a message box in Windows */
                MessageBox(NULL, buffer, PACKAGE_STRING, MB_OK | MB_ICONERROR);
#endif
                abort();
        }

        /* Send this to the GUI handler */
        if (c_log_mode == C_LM_NORMAL)
                return;
        c_log_mode = C_LM_NO_FUNC;
        if (c_log_func)
                c_log_func(level, margin, buffer);
        if (c_log_mode == C_LM_NO_FUNC)
                c_log_mode = C_LM_NORMAL;
}

/******************************************************************************\
 Prints a string to the log file or to standard output. The output detail
 can be controlled using [c_log_level]. Debug calls without any text are
 considered traces.
\******************************************************************************/
void C_log(c_log_level_t level, const char *file, int line,
           const char *function, const char *fmt, ...)
{
        static int log_time, log_count, repeat_count;
        static char last_log[BUFFER_SIZE];
        int margin;
        char fmt2[128], buffer[BUFFER_SIZE];
        va_list va;

        if (level >= C_LOG_DEBUG && (!fmt || !fmt[0]))
                level = C_LOG_TRACE;
        if (level > C_LOG_ERROR && level > c_log_level.value.n)
                return;

        /* Throttle log spam */
        if (c_time_msec - log_time > 1000) {
                log_count = 1;
                log_time = c_time_msec;
        } else if (c_frame > 0 && c_log_throttle.value.n > 0 &&
                   ++log_count > c_log_throttle.value.n)
                return;

        va_start(va, fmt);
        if (c_log_level.value.n <= C_LOG_STATUS) {
                if (level >= C_LOG_STATUS) {
                        snprintf(fmt2, sizeof(fmt2), "%s", fmt);
                        margin = 0;
                } else if (level == C_LOG_WARNING) {
                        snprintf(fmt2, sizeof(fmt2), "* %s", fmt);
                        margin = 2;
                } else {
                        snprintf(fmt2, sizeof(fmt2), "*** %s", fmt);
                        margin = 4;
                }
        } else if (c_log_level.value.n == C_LOG_DEBUG) {
                if (level >= C_LOG_DEBUG) {
                        snprintf(fmt2, sizeof(fmt2), "| %s(): %s",
                                 function, fmt);
                        margin = 6 + C_strlen(function);
                } else if (level == C_LOG_STATUS) {
                        snprintf(fmt2, sizeof(fmt2), "\n%s(): %s --",
                                 function, fmt);
                        margin = 4 + C_strlen(function);
                } else if (level == C_LOG_WARNING) {
                        snprintf(fmt2, sizeof(fmt2), "* %s(): %s",
                                 function, fmt);
                        margin = 6 + C_strlen(function);
                } else {
                        snprintf(fmt2, sizeof(fmt2), "*** %s(): %s",
                                 function, fmt);
                        margin = 8 + C_strlen(function);
                }
        } else if (c_log_level.value.n >= C_LOG_TRACE) {
                margin = 8;
                if (level >= C_LOG_TRACE)
                        snprintf(fmt2, sizeof(fmt2), "! %s:%d, %s():\n%s",
                                 file, line, function, fmt);
                else if (level == C_LOG_DEBUG)
                        snprintf(fmt2, sizeof(fmt2), "| %s:%d, %s():\n%s",
                                 file, line, function, fmt);
                else if (level == C_LOG_STATUS)
                        snprintf(fmt2, sizeof(fmt2), "\n%s:%d, %s():\n%s --",
                                 file, line, function, fmt);
                else if (level == C_LOG_WARNING)
                        snprintf(fmt2, sizeof(fmt2), "* %s:%d, %s():\n%s",
                                 file, line, function, fmt);
                else
                        snprintf(fmt2, sizeof(fmt2), "*** %s:%d, %s():\n%s",
                                 file, line, function, fmt);
        }
        vsnprintf(buffer, sizeof (buffer), fmt2, va);
        va_end(va);

        /* If this is just repeat text, don't bother reprinting it */
        if (!strcmp(buffer, last_log)) {
                repeat_count++;
                return;
        } else if (repeat_count)
                log_output(C_LOG_DEBUG, margin,
                           C_va("(repeated %dx)", repeat_count));
        C_strncpy_buf(last_log, buffer);
        repeat_count = 0;

        log_output(level, margin, buffer);
}

/******************************************************************************\
 If [boolean] is FALSE then generates an error. The error message contains the
 actual assertion statement and source location information.
\******************************************************************************/
void C_assert_full(const char *file, int line, const char *function,
                   int boolean, const char *message)
{
        if (!boolean)
                return;
        C_error_full(file, line, function, "Assertion failed: %s", message);
}

