/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* This file forms the starting point for the client program. The client may
   also function as a server. */

#include "common/c_shared.h"
#include "network/n_shared.h"
#include "render/r_shared.h"
#include "interface/i_shared.h"
#include "game/g_shared.h"

#define CORRUPT_CHECK_VALUE 1776

static r_text_t status_text;

/******************************************************************************\
 Renders the status text on the screen. This function uses the throttled
 counter for interval timing. Counters are reset periodically.
\******************************************************************************/
static void render_status(void)
{
        if (c_show_fps.value.n <= 0)
                return;
        if (C_count_poll(&c_throttled, 2000)) {
                char *str;

                if (c_throttle_msec > 0)
                        str = C_va(PACKAGE_STRING
                                   ": %.0f fps (%.0f%% throttled), "
                                   "%.0f faces/frame",
                                   C_count_fps(&c_throttled),
                                   100.f * C_count_per_frame(&c_throttled) /
                                           c_throttle_msec,
                                   C_count_per_frame(&r_count_faces));
                else
                        str = C_va(PACKAGE_STRING
                                   ": %.0f fps, %.0f faces/frame",
                                   C_count_fps(&c_throttled),
                                   C_count_per_frame(&r_count_faces));
                R_text_configure(&status_text, R_FONT_CONSOLE,
                                 0, 1.f, FALSE, str);
                status_text.sprite.origin = C_vec2(4.f, 4.f);
                C_count_reset(&c_throttled);
                C_count_reset(&r_count_faces);
        }
        R_text_render(&status_text);
}
/******************************************************************************\
 Run one frame of the client's graphical main loop
\******************************************************************************/
int plutocracy_update( void )
{
        SDL_Event ev;
        
        R_start_frame();
        while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT)
                {
                        c_exit = TRUE;
                }
                I_dispatch(&ev);
        }
        G_render_globe();
        I_render();
        render_status();
        R_finish_frame();
        C_time_update();
        C_throttle_fps();

        /* Update the game after rendering everything */
        G_update_host();
        G_update_client();
        return 0;
}

/******************************************************************************\
 This is the client's graphical main loop.
\******************************************************************************/
void main_loop(void)
{
        static int corrupt_check = CORRUPT_CHECK_VALUE;

        C_status("Main loop");
        C_time_init();
        C_rand_seed((unsigned int)time(NULL));
        R_text_init(&status_text);
        while (!c_exit) {
                if (plutocracy_update()) break;
                /* This check is a long-shot, but if there was rampant memory
                   corruption this variable's value may have been changed */
                if (corrupt_check != CORRUPT_CHECK_VALUE)
                        C_error("Static memory corruption detected");
        }

        /* Do NOT do cleanup here! This code is not guaranteed to execute. */
}

/******************************************************************************\
 Concatenates an argument array and runs it through the config parser.
\******************************************************************************/
#if 0 /* not currently used, but evilchampion requested it not be deleted yet */
static void parse_config_args(int argc, char *argv[])
{
        int i, len;
        char buffer[4096], *pos;

        if (argc < 2)
                return;
        C_status("Parsing command line");
        buffer[0] = NUL;
        pos = buffer;
        for (i = 1; i < argc; i++) {
                len = C_strlen(argv[i]);
                if (pos + len >= buffer + sizeof (buffer)) {
                        C_warning("Command-line config overflowed");
                        return;
                }
                memcpy(pos, argv[i], len);
                pos += len;
                *(pos++) = ' ';
        }
        *pos = NUL;
        C_parse_config_string(buffer);
}
#endif

/******************************************************************************\
 Called when the program quits normally or is killed by a signal and should
 perform an orderly cleanup.
\******************************************************************************/
void Plutocracy_Cleanup(void)
{
        static int ran_once;

        /* Disable the log event handler */
        c_log_mode = C_LM_CLEANUP;

        /* It is possible that this function will get called multiple times
           for certain kinds of exits, do not clean-up twice! */
        if (ran_once) {
                C_warning("Cleanup already called");
                return;
        }
        ran_once = TRUE;

        C_status("Cleaning up");
        G_cleanup();
        I_cleanup();
        R_text_cleanup(&status_text);
        R_free_test_assets();
        R_cleanup();
        N_cleanup();
        SDL_Quit();
        C_cleanup_lang();
        C_check_leaks();
        C_debug("Done");
}

/******************************************************************************\
 Caught a signal.
\******************************************************************************/
static void signal_handler(int sig)
{
        C_warning("Caught signal %d", sig);
        exit(1);
}

/******************************************************************************\
 Initialize SDL.
\******************************************************************************/
static void init_sdl(void)
{
        SDL_version compiled;
        const SDL_version *linked;

        SDL_VERSION(&compiled);
        C_debug("Compiled with SDL %d.%d.%d",
                compiled.major, compiled.minor, compiled.patch);
        linked = SDL_Linked_Version();
        if (!linked)
                C_error("Failed to get SDL linked version");
        C_debug("Linked with SDL %d.%d.%d",
                linked->major, linked->minor, linked->patch);
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
                C_error("Failed to initialize SDL: %s", SDL_GetError());
        SDL_WM_SetCaption(PACKAGE_STRING, PACKAGE);
}

/******************************************************************************\
 Start up the client program from here.
\******************************************************************************/
int init_plutocracy(void)
{
        /* Use the cleanup function instead of lots of atexit() calls to
           control the order of cleanup */
        atexit(Plutocracy_Cleanup);

        /* Set signal handler */
        C_signal_handler(signal_handler);

        /* Each namespace must register its configurable variables */
        C_register_variables();
        N_register_variables();
        R_register_variables();
        I_register_variables();
        G_register_variables();

        /* Parse configuration scripts and open the log file */
        C_parse_config_file("autogen.cfg");
        C_parse_config_file("autoexec.cfg");
        I_parse_config();
        C_open_log_file();

        /* Run tests if they are enabled */
        C_endian_check();
        C_test_mem_check();

        /* Seed the system random number generator */
        srand((unsigned int)time(NULL));

        /* Initialize */
        C_status("Initializing " PACKAGE_STRING " client");
        C_init_lang();
        C_translate_vars();
        init_sdl();
        N_init();
        R_init();
        G_init();
        I_init();
        R_load_test_assets();
        G_refresh_servers();

        return 0;
}

