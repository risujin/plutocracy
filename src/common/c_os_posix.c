/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* This file should only be included on POSIX-compliant systems */

#include "c_shared.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

/* Signals we catch and die on */
static int catch_signals[] = {SIGSEGV, SIGHUP, SIGINT, SIGTERM,
                              SIGQUIT, SIGALRM, -1};

/******************************************************************************\
 Creates directory if it does not already exist. Returns TRUE if the directory
 exists after the call.
\******************************************************************************/
int C_mkdir(const char *path)
{
        if (!mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
                C_debug("Created directory '%s'", path);
        else if (errno != EEXIST) {
                C_warning("Failed to create: %s", strerror(errno));
                return FALSE;
        }
        return TRUE;
}

/******************************************************************************\
 Returns the path to the user's writeable Plutocracy directory without
 trailing slash.
\******************************************************************************/
const char *C_user_dir(void)
{
        static char user_dir[256];

        if (user_dir[0])
                return user_dir;
        snprintf(user_dir, sizeof (user_dir), "%s/." PACKAGE, getenv("HOME"));
        C_debug("Home directory is '%s'", user_dir);
        C_mkdir(user_dir);
        return user_dir;
}

/******************************************************************************\
 Returns the path to the program installation.
\******************************************************************************/
const char *C_app_dir(void)
{
        return PKGDATADIR;
}

/******************************************************************************\
 Returns the last time that [filename] was modified. Returns negative if an
 error occurred.
\******************************************************************************/
int C_modified_time(const char *filename)
{
        struct stat s;

        if (stat(filename, &s))
                return -1;
        return (int)s.st_mtime;
}

/******************************************************************************\
 Attach a cleanup signal handler and ignore certain signals.
\******************************************************************************/
void C_signal_handler(c_signal_f func)
{
        sigset_t sigset;
        int *ps;

        /* Ignore certain signals */
        signal(SIGPIPE, SIG_IGN);
        signal(SIGFPE, SIG_IGN);

        /* Hook handler */
        ps = catch_signals;
        sigemptyset(&sigset);
        while (*ps != -1) {
                signal(*ps, func);
                sigaddset(&sigset, *(ps++));
        }
        if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) == -1)
                C_warning("Failed to set signal blocking mask");
}

/******************************************************************************\
 Returns TRUE for an absolute path.
\******************************************************************************/
bool C_absolute_path(const char *path)
{
        return path[0] == '/';
}

