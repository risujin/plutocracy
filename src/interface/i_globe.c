/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Interfaces for interacting with the globe */

#include "i_common.h"

/* Time in milliseconds since the last click to qualify as a double-click */
#define DOUBLE_CLICK_MSEC 1000
#define DOUBLE_CLICK_DIST 8.f

static c_vec3_t grab_normal;
static c_vec2_t globe_motion, grab_coords[2];
static float grab_angle;
static int grabbing, grab_rolling, grab_times[2], grab_button;

/******************************************************************************\
 Performs ray-sphere intersection between an arbitrary ray and the globe. The
 ray is specified using origin [o] and normalized direction [d]. If the ray
 intersects the globe, returns TRUE and sets [point] to the location of the
 intersection.

 The original optimized code taken from:
 http://www.devmaster.net/wiki/Ray-sphere_intersection

 The basic idea is to setup the problem algerbraically and solve using the
 quadratic formula. The resulting calculations are greatly simplified by the
 fact that [d] is normalized and that the sphere is centered on the origin.
\******************************************************************************/
static int intersect_ray_globe(c_vec3_t o, c_vec3_t d, c_vec3_t *point)
{
        float B, C, D;

        /* The naming convention here corresponds to the quadratic formula,
           with [D] as the discriminant */
        B = C_vec3_dot(o, d);
        C = C_vec3_dot(o, o) - r_globe_radius * r_globe_radius;
        D = B * B - C;
        if (D < 0.f)
                return FALSE;
        *point = C_vec3_add(o, C_vec3_scalef(d, -B - sqrtf(D)));
        return TRUE;
}

/******************************************************************************\
 Returns an eye-space ray direction from the camera into the screen pixel.
\******************************************************************************/
static c_vec3_t screen_ray(c_vec2_t v)
{
        return C_vec3_norm(C_vec3(v.x - r_width_2d / 2.f,
                                  r_height_2d / 2.f - v.y,
                                  -0.5f * r_height_2d / R_FOV_HALF_TAN));
}

/******************************************************************************\
 Converts a screen coordinate to a globe [normal]. Returns TRUE if the globe
 was clicked on and [normal] was set. Otherwise, returns FALSE and sets
 [angle] to the roll angle.
\******************************************************************************/
static int screen_to_normal(c_vec2_t coords, c_vec3_t *normal, float *angle)
{
        c_vec3_t direction, forward, point;

        /* We need to create a vector that points out of the camera into the
           ray that is cast into the direction of the clicked pixel */
        direction = screen_ray(coords);
        forward = R_rotate_to_cam(direction);

        /* Test the direction ray */
        if (i_test_globe.value.n) {
                c_vec3_t a, b;

                a = C_vec3_add(r_cam_origin, r_cam_forward);
                b = C_vec3_add(r_cam_origin, C_vec3_scalef(forward, 10.f));
                R_render_test_line(a, b, C_color(1.f, 0.75f, 0.f, 1.f));
        }

        /* Find the intersection point and normalize it to get a normal pointing
           from the globe center out to the clicked on point */
        if (intersect_ray_globe(r_cam_origin, forward, &point)) {
                *normal = C_vec3_norm(point);
                return TRUE;
        }

        /* If there was no intersection, find the roll angle of the mouse */
        if (angle)
                *angle = atan2f(direction.y, direction.x);
        return FALSE;
}

/******************************************************************************\
 Grab screen coordinates [coords] and begin rotating the globe.
\******************************************************************************/
static void grab_globe(c_vec2_t coords)
{
        if (grabbing)
                return;
        grabbing = TRUE;
        grab_button = i_mouse_button;

        /* Preserve grab timestamp and position for double-click detection */
        grab_times[0] = grab_times[1];
        grab_times[1] = c_time_msec;
        grab_coords[0] = grab_coords[1];
        grab_coords[1] = coords;

        if (screen_to_normal(coords, &grab_normal, &grab_angle)) {
                grab_rolling = FALSE;
                grab_normal = R_rotate_from_cam(grab_normal);
        } else
                grab_rolling = TRUE;
        I_close_ring();
        G_mouse_ray_miss();
        R_grab_cam();
}

/******************************************************************************\
 Release the globe from a rotation grab.
\******************************************************************************/
static void release_globe(void)
{
        c_vec3_t normal;

        if (!grabbing)
                return;
        grabbing = FALSE;
        R_release_cam();

        /* Check if this is a double right click */
        if (i_mouse_focus != &i_root ||
            c_time_msec - grab_times[0] > DOUBLE_CLICK_MSEC ||
            C_vec2_len(C_vec2_sub(grab_coords[0], i_mouse)) >
            DOUBLE_CLICK_DIST ||
            !screen_to_normal(i_mouse, &normal, NULL))
                return;
        grab_times[0] = 0;
        grab_times[1] = 0;
        R_rotate_cam_to(normal);
}

/******************************************************************************\
 Rotate the globe during a grab or do nothing. [coordsy] are in screen
 coordinates.
\******************************************************************************/
static void rotate_globe(c_vec2_t coords)
{
        c_vec3_t normal, angles;
        float angle;

        /* Rotation without roll */
        if (screen_to_normal(coords, &normal, &angle)) {
                normal = R_rotate_from_cam(normal);

                /* A transition loses the rotation for the frame */
                if (grab_rolling) {
                        grab_normal = normal;
                        grab_rolling = FALSE;
                        return;
                }

                angles.y = acosf(grab_normal.x) - acosf(normal.x);
                angles.x = asinf(grab_normal.y) - asinf(normal.y);
                angles.z = 0.f;
                grab_normal = normal;
        }

        /* Roll-only rotation */
        else {

                /* A transition loses the rotation for the frame */
                if (!grab_rolling) {
                        grab_angle = angle;
                        grab_rolling = TRUE;
                        return;
                }

                angles.x = 0.f;
                angles.y = 0.f;
                angles.z = angle - grab_angle;
                grab_angle = angle;
        }

        R_rotate_cam_by(angles);
}

/******************************************************************************\
 Runs mouse click detection functions for testing.
\******************************************************************************/
static void test_globe(void)
{
        c_vec3_t normal;
        float angle;

        if (!i_test_globe.value.n ||
            !screen_to_normal(i_mouse, &normal, &angle))
                return;
        R_render_test_line(C_vec3_scalef(normal, r_globe_radius),
                           C_vec3_scalef(normal, r_globe_radius + 1.f),
                           C_color(0.f, 1.f, 1.f, 1.f));
}

/******************************************************************************\
 Scroll the globe by moving the mouse to the edge of the screen.
\******************************************************************************/
static void mouse_scroll(void)
{
        c_vec2_t scroll;
        float best, amount;

        if (i_edge_scroll.value.f < 0.f)
                return;
        best = 0.f;

        /* Scroll right */
        amount = 1.f - i_mouse.x / i_edge_scroll.value.f;
        if (amount > best)
                best = amount;

        /* Scroll left */
        amount = 1.f - (r_width_2d - i_mouse.x) /
                       i_edge_scroll.value.f;
        if (amount > best)
                best = amount;

        /* Scroll up */
        amount = 1.f - i_mouse.y / i_edge_scroll.value.f;
        if (amount > best)
                best = amount;

        /* Scroll down */
        amount = 1.f - (r_height_2d - i_mouse.y) /
                       i_edge_scroll.value.f;
        if (amount > best)
                best = amount;

        /* Mouse must be off-screen */
        if (best > 1.f)
                return;

        /* Determine direction */
        scroll = C_vec2_norm(C_vec2(i_mouse.x - r_width_2d / 2.f,
                                    i_mouse.y - r_height_2d / 2.f));

        /* Apply scroll for this frame */
        best *= -c_frame_sec * i_scroll_speed.value.f;
        R_move_cam_by(C_vec2_scalef(scroll, best));
}

/******************************************************************************\
 Processes events from the root window that affect the globe.
\******************************************************************************/
void I_globe_event(i_event_t event)
{
        /* No processing here during limbo */
        if (i_limbo)
                return;

        switch (event) {
        case I_EV_KEY_DOWN:
                if (i_key_focus != &i_root)
                        return;
                I_close_ring();
                if (i_key == SDLK_RIGHT && globe_motion.x > -1.f)
                        globe_motion.x = -i_scroll_speed.value.f;
                if (i_key == SDLK_LEFT && globe_motion.x < 1.f)
                        globe_motion.x = i_scroll_speed.value.f;
                if (i_key == SDLK_DOWN && globe_motion.y > -1.f)
                        globe_motion.y = -i_scroll_speed.value.f;
                if (i_key == SDLK_UP && globe_motion.y < 1.f)
                        globe_motion.y = i_scroll_speed.value.f;
                if (i_key == '-')
                        R_zoom_cam_by(i_zoom_speed.value.f);
                if (i_key == '=')
                        R_zoom_cam_by(-i_zoom_speed.value.f);
                G_process_key(i_key, i_key_shift, i_key_ctrl, i_key_alt);
                break;
        case I_EV_KEY_UP:
                if ((i_key == SDLK_RIGHT && globe_motion.x < 0.f) ||
                    (i_key == SDLK_LEFT && globe_motion.x > 0.f))
                        globe_motion.x = 0.f;
                if ((i_key == SDLK_DOWN && globe_motion.y < 0.f) ||
                    (i_key == SDLK_UP && globe_motion.y > 0.f))
                        globe_motion.y = 0.f;
                break;
        case I_EV_MOUSE_DOWN:
                if (i_mouse_focus != &i_root)
                        break;
                if (i_mouse_button == SDL_BUTTON_WHEELDOWN)
                        R_zoom_cam_by(i_zoom_speed.value.f);
                else if (i_mouse_button == SDL_BUTTON_WHEELUP)
                        R_zoom_cam_by(-i_zoom_speed.value.f);
                else if (i_mouse_button == SDL_BUTTON_MIDDLE)
                        grab_globe(i_mouse);
                else
                        G_process_click(i_mouse_button);
                break;
        case I_EV_MOUSE_UP:
                if (i_mouse_button == grab_button)
                        release_globe();
                break;
        case I_EV_MOUSE_MOVE:
                if (I_ring_shown())
                        break;
                if (i_mouse_focus != &i_root)
                        G_mouse_ray_miss();
                else if (grabbing)
                        rotate_globe(i_mouse);
                break;
        case I_EV_RENDER:
                test_globe();
                R_move_cam_by(C_vec2_scalef(globe_motion, c_frame_sec));
                mouse_scroll();

                /* Because the globe can move by itself for various reasons,
                   do the mouse trace every frame */
                if (!I_ring_shown() && i_mouse_focus == &i_root && !grabbing) {
                        c_vec3_t direction;

                        direction = screen_ray(i_mouse);
                        direction = R_rotate_to_cam(direction);
                        G_mouse_ray(r_cam_origin, direction);
                }
                break;
        default:
                break;
        }
}

