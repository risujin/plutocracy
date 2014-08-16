/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Contains code that uses SDL timers for various purposes */

#include "c_shared.h"

/* Number of milliseconds spent sleeping this frame */
c_count_t c_throttled;

/* The minimum duration of frame in milliseconds calculated from [c_max_fps] by
   the [C_throttle_fps()] function */
int c_throttle_msec;

/* Duration of time since the program started */
int c_time_msec;

/* The current frame number */
int c_frame;

/* Duration of the last frame */
int c_frame_msec;
float c_frame_sec;

/******************************************************************************\
 Initializes counters and timers.
\******************************************************************************/
void C_time_init(void)
{
        c_frame = 1;
        c_time_msec = SDL_GetTicks();
        C_count_reset(&c_throttled);
}

/******************************************************************************\
 Updates the current time. This needs to be called exactly once per frame.
\******************************************************************************/
void C_time_update(void)
{
        static unsigned int last_msec;

        c_time_msec = SDL_GetTicks();
        c_frame_msec = c_time_msec - last_msec;
        c_frame_sec = c_frame_msec / 1000.f;
        last_msec = c_time_msec;
        c_frame++;

        /* Report when a frame takes an unusually long time */
        if (c_frame_msec > 500)
                C_debug("Frame %d lagged, %d msec", c_frame, c_frame_msec);
}

/******************************************************************************\
 Returns the time since the last call to C_timer(). Useful for measuring the
 efficiency of sections of code.
\******************************************************************************/
unsigned int C_timer(void)
{
        static unsigned int last_msec;
        unsigned int elapsed;

        elapsed = SDL_GetTicks() - last_msec;
        last_msec += elapsed;
        return elapsed;
}

/******************************************************************************\
 Initializes a counter structure.
\******************************************************************************/
void C_count_reset(c_count_t *counter)
{
        counter->last_time = c_time_msec;
        counter->start_frame = c_frame;
        counter->start_time = c_time_msec;
        counter->value = 0.f;
}

/******************************************************************************\
 Polls a counter to see if [interval] msec have elapsed since the last poll.
 Returns TRUE if the counter is ready to be polled and sets the poll time.
\******************************************************************************/
int C_count_poll(c_count_t *counter, int interval)
{
        if (c_time_msec - counter->last_time < interval)
                return FALSE;
        counter->last_time = c_time_msec;
        return TRUE;
}

/******************************************************************************\
 Returns the per-frame count of a counter.
\******************************************************************************/
float C_count_per_frame(const c_count_t *counter)
{
        int frames;

        frames = c_frame - counter->start_frame;
        if (frames < 1)
                return 0.f;
        return counter->value / frames;
}

/******************************************************************************\
 Returns the per-second count of a counter.
\******************************************************************************/
float C_count_per_sec(const c_count_t *counter)
{
        float seconds;

        seconds = (c_time_msec - counter->start_time) / 1000.f;
        if (seconds <= 0.f)
                return 0.f;
        return counter->value / seconds;
}

/******************************************************************************\
 Returns the average frames-per-second while counter was running.
\******************************************************************************/
float C_count_fps(const c_count_t *counter)
{
        float seconds;

        seconds = (c_time_msec - counter->start_time) / 1000.f;
        if (seconds <= 0.f)
                return 0.f;
        return (c_frame - counter->start_frame) / seconds;
}

/******************************************************************************\
 Throttle framerate if vsync is off or broken so we don't burn the CPU for no
 reason. SDL_Delay() will only work in 10ms increments on some systems so it
 is necessary to break down our delays into bite-sized chunks ([wait_msec]).
\******************************************************************************/
void C_throttle_fps(void)
{
        static int wait_msec;
        int msec;

        if (c_max_fps.value.n < 1)
                return;
        c_throttle_msec = 1000 / c_max_fps.value.n;
        if (c_frame_msec > c_throttle_msec)
                return;
        wait_msec += c_throttle_msec - c_frame_msec;
        msec = (wait_msec / 10) * 10;
        if (msec > 0) {
                SDL_Delay(msec);
                wait_msec -= msec;
                C_count_add(&c_throttled, msec);
        }
}

