/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "r_common.h"

/* Limits */
#define CLIP_STACK 32
#define MODE_STACK 32
#define OPTIONS_MAX 32

/* Keep track of how many faces we render each frame */
c_count_t r_count_faces;

/* Current OpenGL settings */
r_mode_t r_mode;
c_color_t clear_color;
int r_width_2d, r_height_2d, r_mode_hold, r_restart;

/* Structure to wrap OpenGL extensions */
r_ext_t r_ext;

/* Set to TRUE to restart the video system on the next frame */
int r_restart;

/* The frame number of the last video restart */
int r_init_frame;

/* Supported extensions */
int r_extensions[R_EXTENSIONS];

/* The 3D-mode projection matrix */
GLfloat r_proj_matrix[16];

/* Effective pixel scale value. Use this instead of [r_pixel_scale] because
   the latter might be in a special mode. */
float r_scale_2d;
int r_scale_2d_frame;

/* Clipping stack */
static int clip_stack;
static float clip_values[CLIP_STACK * 4];

/* Mode stack */
static int mode_stack;
static r_mode_t mode_values[MODE_STACK];

/* When non-empty will save a PNG screenshot to this filename */
static char screenshot[256];

/* OpenGL options that are temporarily disabled */
static GLenum enabled_options[OPTIONS_MAX], disabled_options[OPTIONS_MAX];

static r_text_t status_text;

/******************************************************************************\
 Equivalent to glEnable, but stores the current value for later restoration.
\******************************************************************************/
void R_gl_enable(GLenum option)
{
        int i;

        if (glIsEnabled(option))
                return;

        /* See if we disabled this temporarily first */
        for (i = 0; i < OPTIONS_MAX; i++)
                if (disabled_options[i] == option) {
                        disabled_options[i] = 0;
                        glEnable(option);
                        return;
                }

        /* Find an open enabled slot */
        for (i = 0; i < OPTIONS_MAX; i++)
                if (!enabled_options[i]) {
                        enabled_options[i] = option;
                        glEnable(option);
                        return;
                }

        C_error("Enabled options buffer overflow");
}

/******************************************************************************\
 Equivalent to glDisable, but stores the current value for later restoration.
\******************************************************************************/
void R_gl_disable(GLenum option)
{
        int i;

        if (!glIsEnabled(option))
                return;

        /* See if we enabled this temporarily first */
        for (i = 0; i < OPTIONS_MAX; i++)
                if (enabled_options[i] == option) {
                        enabled_options[i] = 0;
                        glDisable(option);
                        return;
                }

        /* Find an open disabled slot */
        for (i = 0; i < OPTIONS_MAX; i++)
                if (!disabled_options[i]) {
                        disabled_options[i] = option;
                        glDisable(option);
                        return;
                }

        C_error("Disabled options buffer overflow");
}

/******************************************************************************\
 Restores any settings changed by [R_gl_enable] or [R_gl_disable].
\******************************************************************************/
void R_gl_restore(void)
{
        int i;

        for (i = 0; i < OPTIONS_MAX; i++) {
                if (enabled_options[i]) {
                        glDisable(enabled_options[i]);
                        enabled_options[i] = 0;
                }
                if (disabled_options[i]) {
                        glEnable(disabled_options[i]);
                        disabled_options[i] = 0;
                }
        }
}

/******************************************************************************\
 Updates when [r_pixel_scale] changes.
\******************************************************************************/
static void pixel_scale_update(void)
{
        float new_scale_2d;

        /* Automatic pixel scale */
        if (!r_pixel_scale.value.f) {
                float min;

                min = (float)r_width.value.n;
                if (r_height.value.n < min)
                        min = (float)r_height.value.n;
                new_scale_2d = min < 256.f ? 1.f :
                               (float)min / r_height.stock.n;
        }

        /* Directly set */
        else
                new_scale_2d = r_pixel_scale.value.f;

        /* Allowed ranges */
        if (new_scale_2d < 0.5f)
                new_scale_2d = 0.5f;
        if (new_scale_2d > 2.0f)
                new_scale_2d = 2.0f;

        /* Pixel scale changed */
        if (new_scale_2d != r_scale_2d) {
                r_scale_2d = new_scale_2d;
                r_scale_2d_frame = c_frame;
                R_free_fonts();
                R_load_fonts();
        }

        r_width_2d = (int)(r_width.value.n / r_scale_2d + 0.5f);
        r_height_2d = (int)(r_height.value.n / r_scale_2d + 0.5f);
        C_debug("2D area %dx%d", r_width_2d, r_height_2d);
}

/******************************************************************************\
 Sets the video mode. SDL creates a window in windowed mode.
\******************************************************************************/
static int set_video_mode(void)
{
        int flags;

        C_var_unlatch(&r_vsync);
        C_var_unlatch(&r_depth_bits);
        C_var_unlatch(&r_color_bits);
        C_var_unlatch(&r_windowed);
        C_var_unlatch(&r_width);
        C_var_unlatch(&r_height);
        C_var_unlatch(&r_multisample);

        /* Ensure a minimum render size or pre-rendering will crash */
        if (r_width.value.n < R_WIDTH_MIN)
                r_width.value.n = R_WIDTH_MIN;
        if (r_height.value.n < R_HEIGHT_MIN)
                r_height.value.n = R_HEIGHT_MIN;

#ifdef WINDOWS
        /* On Windows, before we change the video mode we need to flush out
           any existing textures in case they actually don't get lost or
           deallocated properly on their own */
        R_dealloc_textures();
#endif

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, r_depth_bits.value.n);
        SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_vsync.value.n);
        if (r_multisample.value.n <= 0) {
                r_multisample.value.n = 0;
                SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        } else
                SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, r_multisample.value.n);
        if (r_color_bits.value.n <= 16) {
                SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
                SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
                SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
                SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
        } else {
                SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
                SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
                SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
                SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
        }
        flags = SDL_OPENGL | SDL_DOUBLEBUF | SDL_ANYFORMAT;
        if (!r_windowed.value.n)
                flags |= SDL_FULLSCREEN;
        if (!SDL_SetVideoMode(r_width.value.n, r_height.value.n,
                              r_color_bits.value.n, flags)) {
                C_warning("Failed to set video mode: %s", SDL_GetError());
                return FALSE;
        }

        /* Set screen view */
        glViewport(0, 0, r_width.value.n, r_height.value.n);

        /* We only need to compute a perspective matrix once as we never change
           the field-of-view in-game */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(R_FOV, (float)r_width.value.n / r_height.value.n,
                       1.f, 512.f);
        glGetFloatv(GL_PROJECTION_MATRIX, r_proj_matrix);
        glMatrixMode(GL_MODELVIEW);

        R_check_errors();

        /* Update pixel scale */
        pixel_scale_update();

#ifdef WINDOWS
        /* Under Windows we just lost all of our textures, so we need to
           reload the entire texture linked list */
        R_realloc_textures();
#endif

        return TRUE;
}

/******************************************************************************\
 Checks the extension string to see if an extension is listed.
\******************************************************************************/
static bool check_extension(const char *ext)
{
        static const char *ext_str;
        static int ext_str_len;
        const char *str;
        int len;

        if (!ext_str) {
                ext_str = (const char *)glGetString(GL_EXTENSIONS);
                if (!ext_str)
                        return FALSE;
                ext_str_len = C_strlen(ext_str);
        }
        len = C_strlen(ext);
        for (str = ext_str; ; ) {
                str = strstr(str, ext);
                if (!str || !*str)
                        break;
                if (str + len > ext_str + ext_str_len)
                        return FALSE;
                if (str[len] > ' ')
                        continue;
                return TRUE;
        }
        return FALSE;
}

/******************************************************************************\
 Outputs debug strings and checks supported extensions. Fills out [r_ext].
\******************************************************************************/
static void check_gl_extensions(void)
{
        C_zero(&r_ext);

        /* Multitexture */
        glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &r_ext.multitexture);
        C_debug("%d texture units supported", r_ext.multitexture);
        if (r_ext.multitexture > 1) {
                r_ext.glActiveTexture =
                        SDL_GL_GetProcAddress("glActiveTexture");
                if (!r_ext.glActiveTexture) {
                        C_warning("Failed to get glActiveTexture address");
                        r_ext.multitexture = 1;
                }
        }

        /* Point sprites */
        C_var_unlatch(&r_ext_point_sprites);
        if (r_ext_point_sprites.value.n &&
            check_extension("GL_ARB_point_sprite")) {
                r_ext.point_sprites = TRUE;
                C_debug("Hardware point sprites supported");
        } else {
                r_ext.point_sprites = FALSE;
                C_warning("Using software point sprites");
        }

        /* Check for anisotropic filtering */
        if (check_extension("GL_EXT_texture_filter_anisotropic")) {
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,
                            &r_ext.anisotropy);
                C_debug("%g anisotropy levels supported", r_ext.anisotropy);
        } else
                C_warning("Anisotropic filtering not supported");

        /* Check for vertex buffer objects */
        if (check_extension("GL_ARB_vertex_buffer_object")) {
                r_ext.glGenBuffers = SDL_GL_GetProcAddress("glGenBuffers");
                r_ext.glDeleteBuffers =
                        SDL_GL_GetProcAddress("glDeleteBuffers");
                r_ext.glBindBuffer = SDL_GL_GetProcAddress("glBindBuffer");
                r_ext.glBufferData = SDL_GL_GetProcAddress("glBufferData");
                if (!r_ext.glGenBuffers || !r_ext.glDeleteBuffers ||
                    !r_ext.glBindBuffer || !r_ext.glBufferData) {
                        C_warning("Vertex buffer extension supported, but "
                                  "failed to get function addresses");
                } else {
                        r_ext.vertex_buffers = TRUE;
                        C_debug("Vertex buffer objects supported");
                }
        } else
                C_warning("Vertex buffer objects not supported");

        /* Full support for non-power-of-two textures */
        if (check_extension("GL_ARB_texture_non_power_of_two")) {
                r_ext.npot_textures = TRUE;
                C_debug("Non-power-of-two textures supported");
        } else {
                r_ext.npot_textures = FALSE;
                C_warning("Non-power-of-two textures not supported");
        }
}

/******************************************************************************\
 Sets the initial OpenGL state.
\******************************************************************************/
static void set_gl_state(void)
{
        c_color_t color;

        glEnable(GL_TEXTURE_2D);
        glAlphaFunc(GL_GREATER, 1 / 255.f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_LEQUAL);
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

        /* We use lines to do 2D edge anti-aliasing although there is probably
           a better way so we need to always smooth lines (requires alpha
           blending to be on to work) */
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

        /* Only rasterize polygons that are facing you. Blender seems to export
           its polygons in counter-clockwise order. */
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        /* GL_SMOOTH here means faces are shaded taking each vertex into
           account rather than one solid color per face */
        glShadeModel(GL_SMOOTH);

        /* Not configured to a render mode intially. The stack is initialized
           with no modes on it (index of -1). */
        r_mode = R_MODE_NONE;
        mode_stack = -1;
        mode_values[0] = R_MODE_NONE;

        /* Setup the lighting model */
        color = C_color(0.f, 0.f, 0.f, 1.f);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, C_ARRAYF(color));

        /* Point sprites */
        if (r_ext.point_sprites) {
                glEnable(GL_POINT_SPRITE);
                glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
        }

        /* Multisampling starts out enabled, we don't need it for everything */
        glDisable(GL_MULTISAMPLE);

        R_check_errors();
}

/******************************************************************************\
 Updates the gamma ramp when [r_gamma] changes.
\******************************************************************************/
static int gamma_update(c_var_t *var, c_var_value_t value)
{
        return SDL_SetGamma(value.f, value.f, value.f) >= 0;
}

/******************************************************************************\
 Updates the clear color when [r_clear] changes.
\******************************************************************************/
static int clear_update(c_var_t *var, c_var_value_t value)
{
        clear_color = C_color_string(value.s);
        glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.f);
        return TRUE;
}

/******************************************************************************\
 Creates the client window. Initializes OpenGL settings such view matrices,
 culling, and depth testing. Returns TRUE on success.
\******************************************************************************/
void R_init(void)
{
        char buffer[64];

        C_status("Opening window");
        C_var_unlatch(&r_pixel_scale);
        C_count_reset(&r_count_faces);

        /* Print the video driver name */
        SDL_VideoDriverName(buffer, sizeof (buffer));
        C_debug("SDL video driver '%s'", buffer);

        /* If we fail to set the video mode, our only hope is to reset the
           "unsafe" variables that the user might have messed up and try
           setting the video mode again */
        if (!set_video_mode()) {
                C_reset_unsafe_vars();
                if (!set_video_mode())
                        C_error("Failed to set video mode");
                C_warning("Video mode set after resetting unsafe variables");
        }

        check_gl_extensions();
        set_gl_state();
        r_cam_zoom = 1000.f;
        R_clip_disable();
        R_load_assets();
        R_init_camera();
        R_init_solar();
        R_init_globe();
        R_init_ships();

        /* Set updatable variables */
        C_var_update(&r_clear, clear_update);
        C_var_update(&r_gamma, gamma_update);
        /* Initilize status text */
        R_text_init(&status_text);
}

/******************************************************************************\
 Cleanup the namespace.
\******************************************************************************/
void R_cleanup(void)
{
        R_text_cleanup(&status_text);
        R_cleanup_globe();
        R_cleanup_solar();
        R_cleanup_ships();
        R_free_assets();
}

/******************************************************************************\
 Renders the status text on the screen. This function uses the throttled
 counter for interval timing. Counters are reset periodically.
\******************************************************************************/
void R_render_status(void)
{
        if (c_show_fps.value.n <= 0 && c_show_bps.value.n <= 0) {
                n_bytes_received = n_bytes_sent = 0;
                return;
        }
        if(C_count_poll(&c_throttled, 1000)) {
                char display[150] = {PACKAGE_STRING ":"};
                int l = sizeof(PACKAGE_STRING ":") - 1;
                if(c_show_fps.value.n > 0) {
                        if (c_throttle_msec > 0)
                                l += snprintf(&display[l], sizeof(display) - l,
                                      " %.0f fps (%.0f%% throttled), "
                                      "%.0f faces/frame",
                                      C_count_fps(&c_throttled),
                                      100.f * C_count_per_frame(&c_throttled) /
                                      c_throttle_msec,
                                      C_count_per_frame(&r_count_faces));
                        else
                                l += snprintf(&display[l], sizeof(display) - l,
                                      " %.0f fps, %.0f faces/frame",
                                      C_count_fps(&c_throttled),
                                      C_count_per_frame(&r_count_faces));
                }
                if(c_show_bps.value.n > 0 && l < sizeof(display)) {
                        snprintf(&display[l], sizeof(display) - l, "%s"
                                 "Bps received: %d Bps sent: %d",
                                 (c_show_fps.value.n > 0) ? " | " : "",
                                  n_bytes_received, n_bytes_sent);
                        n_bytes_received = n_bytes_sent = 0;
                }
                R_text_configure(&status_text, R_FONT_CONSOLE,
                                 0, 1.f, FALSE, display);
                status_text.sprite.origin = C_vec2(4.f, 4.f);
                C_count_reset(&c_throttled);
                C_count_reset(&r_count_faces);
        }
        R_text_render(&status_text);
}

/******************************************************************************\
 See if there were any OpenGL errors.
\******************************************************************************/
void R_check_errors_full(const char *file, int line, const char *func)
{
        int gl_error;

        if (r_gl_errors.value.n < 1)
                return;
        gl_error = glGetError();
        if (gl_error == GL_NO_ERROR)
                return;
        if (r_gl_errors.value.n > 1)
                C_error_full(file, line, func, "OpenGL error %d: %s",
                             gl_error, gluErrorString(gl_error));
        C_warning(file, line, func, "OpenGL error %d: %s",
                  gl_error, gluErrorString(gl_error));
}

/******************************************************************************\
 Sets the OpenGL clipping planes to the strictest clipping in each direction.
 This only works in 2D mode. OpenGL takes plane equations as arguments. Points
 that are visible satisfy the following inequality:
 a * x + b * y + c * z + d >= 0
\******************************************************************************/
static void set_clipping(void)
{
        GLdouble eqn[4];
        float left, top, right, bottom;
        int i;

        if (r_mode != R_MODE_2D)
                return;

        /* Find the most restrictive clipping values in each direction */
        left = clip_values[0];
        top = clip_values[1];
        right = clip_values[2];
        bottom = clip_values[3];
        for (i = 1; i <= clip_stack; i++) {
                if (clip_values[4 * i] > left)
                        left = clip_values[4 * i];
                if (clip_values[4 * i + 1] > top)
                        top = clip_values[4 * i + 1];
                if (clip_values[4 * i + 2] < right)
                        right = clip_values[4 * i + 2];
                if (clip_values[4 * i + 3] < bottom)
                        bottom = clip_values[4 * i + 3];
        }

        /* Clip left */
        eqn[2] = 0.f;
        eqn[3] = -1.f;
        if (left > 0.f) {
                eqn[0] = 1.f / left;
                eqn[1] = 0.f;
                glEnable(GL_CLIP_PLANE0);
                glClipPlane(GL_CLIP_PLANE0, eqn);
        } else
                glDisable(GL_CLIP_PLANE0);

        /* Clip top */
        if (top > 0.f) {
                eqn[0] = 0.f;
                eqn[1] = 1.f / top;
                glEnable(GL_CLIP_PLANE1);
                glClipPlane(GL_CLIP_PLANE1, eqn);
        } else
                glDisable(GL_CLIP_PLANE1);

        /* Clip right */
        eqn[3] = 1.f;
        if (right < r_width_2d - 1) {
                eqn[0] = -1.f / right;
                eqn[1] = 0.f;
                glEnable(GL_CLIP_PLANE2);
                glClipPlane(GL_CLIP_PLANE2, eqn);
        } else
                glDisable(GL_CLIP_PLANE2);

        /* Clip bottom */
        if (bottom < r_height_2d - 1) {
                eqn[0] = 0.f;
                eqn[1] = -1.f / bottom;
                glEnable(GL_CLIP_PLANE3);
                glClipPlane(GL_CLIP_PLANE3, eqn);
        } else
                glDisable(GL_CLIP_PLANE3);
}

/******************************************************************************\
 Disables the clipping at the current stack level.
\******************************************************************************/
void R_clip_disable(void)
{
        clip_values[4 * clip_stack] = 0.f;
        clip_values[4 * clip_stack + 1] = 0.f;
        clip_values[4 * clip_stack + 2] = 100000.f;
        clip_values[4 * clip_stack + 3] = 100000.f;
        set_clipping();
}

/******************************************************************************\
 Adjust the clip stack.
\******************************************************************************/
void R_push_clip(void)
{
        if (++clip_stack >= CLIP_STACK)
                C_error("Clip stack overflow");
        R_clip_disable();
}

void R_pop_clip(void)
{
        if (--clip_stack < 0)
                C_error("Clip stack underflow");
        set_clipping();
}

/******************************************************************************\
 Clip in a specific direction.
\******************************************************************************/
void R_clip_left(float dist)
{
        clip_values[4 * clip_stack] = dist;
        set_clipping();
}

void R_clip_top(float dist)
{
        clip_values[4 * clip_stack + 1] = dist;
        set_clipping();
}

void R_clip_right(float dist)
{
        clip_values[4 * clip_stack + 2] = dist;
        set_clipping();
}

void R_clip_bottom(float dist)
{
        clip_values[4 * clip_stack + 3] = dist;
        set_clipping();
}

/******************************************************************************\
 Setup a clipping rectangle.
\******************************************************************************/
void R_clip_rect(c_vec2_t origin, c_vec2_t size)
{
        clip_values[4 * clip_stack] = origin.x;
        clip_values[4 * clip_stack + 1] = origin.y;
        clip_values[4 * clip_stack + 2] = origin.x + size.x;
        clip_values[4 * clip_stack + 3] = origin.y + size.y;
        set_clipping();
}

/******************************************************************************\
 Sets up OpenGL to render 3D polygons in world space.
\******************************************************************************/
void R_set_mode(r_mode_t mode)
{
        if (r_mode_hold)
                return;

        /* Make sure the texture coordinate matrix is identity */
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();

        /* Reset model-view matrices even if the mode didn't change */
        glMatrixMode(GL_MODELVIEW);
        if (mode == R_MODE_3D)
                glLoadMatrixf(r_cam_matrix);
        else if (mode == R_MODE_2D)
                glLoadIdentity();

        if (mode == r_mode)
                return;
        r_mode = mode;
        glMatrixMode(GL_PROJECTION);

        /* 2D mode sets up an orthogonal projection to render sprites */
        if (mode == R_MODE_2D) {
                glLoadIdentity();
                glOrtho(0.f, r_width_2d, r_height_2d, 0.f, 0.f, 1.f);
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                set_clipping();
        } else {
                glDisable(GL_CLIP_PLANE0);
                glDisable(GL_CLIP_PLANE1);
                glDisable(GL_CLIP_PLANE2);
                glDisable(GL_CLIP_PLANE3);
        }

        /* 3D mode sets up perspective projection and camera view for models */
        if (mode == R_MODE_3D) {
                glLoadMatrixf(r_proj_matrix);
                glEnable(GL_CULL_FACE);
                glEnable(GL_DEPTH_TEST);
                glMatrixMode(GL_MODELVIEW);
        } else {
                glDisable(GL_CULL_FACE);
                glDisable(GL_DEPTH_TEST);
        }

        R_check_errors();
}

/******************************************************************************\
 Save the previous mode and set the mode. Will also push an OpenGL model view
 matrix on the stack.
\******************************************************************************/
void R_push_mode(r_mode_t mode)
{
        if (++mode_stack >= 32)
                C_error("Mode stack overflow");
        mode_values[mode_stack] = mode;
        glPushMatrix();
        R_set_mode(mode);
}

/******************************************************************************\
 Restore saved mode. Will also pop an OpenGL model view matrix off the stack.
\******************************************************************************/
void R_pop_mode(void)
{
        if (--mode_stack < -1)
                C_error("Mode stack underflow");
        glPopMatrix();
        if (mode_stack >= 0)
                R_set_mode(mode_values[mode_stack]);
}

/******************************************************************************\
 Clears the buffer and starts rendering the scene.
\******************************************************************************/
void R_start_frame(void)
{
        int clear_flags;

        /* Pixel scale changes affect the entire frame so wait until it is
           done before applying the new value */
        if (C_var_unlatch(&r_pixel_scale) && !r_restart)
                pixel_scale_update();

        /* Video can only be restarted at the start of the frame */
        if (r_restart) {
                set_video_mode();
                set_gl_state();
                if (r_color_bits.changed > r_init_frame)
                        R_realloc_textures();
                r_init_frame = c_frame;
                r_restart = FALSE;
        }

        /* Only clear the screen if r_clear is set */
        clear_flags = GL_DEPTH_BUFFER_BIT;
        if (clear_color.a > 0.f)
                clear_flags |= GL_COLOR_BUFFER_BIT;
        glClear(clear_flags);

        R_update_camera();
        R_render_solar();
}

/******************************************************************************\
 Mark this frame for saving a screenshot when the buffer flips. Returns the
 full path and filename of the image saved or NULL if there was a problem.
\******************************************************************************/
const char *R_save_screenshot(void)
{
        time_t msec;
        struct tm *local;
        const char *filename;
        int i;

        if (!C_mkdir(r_screenshots_dir.value.s))
                return NULL;

        /* Can't take two screenshots per frame */
        if (screenshot[0]) {
                C_warning("Can't save screenshot '%s' queued", screenshot);
                return NULL;
        }

        time(&msec);
        local = localtime(&msec);

        /* Start off with a path based on the current date and time */
        filename = C_va("%s/%d-%02d-%02d--%02d%02d.png",
                        r_screenshots_dir.value.s, local->tm_year + 1900,
                        local->tm_mon + 1, local->tm_mday, local->tm_hour,
                        local->tm_min);

        /* If this is taken, start adding letters to the end of it */
        for (i = 0; C_file_exists(filename) && i < 26; i++)
                filename = C_va("%s/%d-%02d-%02d--%02d%02d%c.png",
                                r_screenshots_dir.value.s,
                                local->tm_year + 1900,
                                local->tm_mon + 1, local->tm_mday,
                                local->tm_hour, local->tm_min, 'a' + i);

        C_strncpy_buf(screenshot, filename);
        return filename;
}

/******************************************************************************\
 Finishes rendering the scene and flips the buffer.
\******************************************************************************/
void R_finish_frame(void)
{
        R_render_tests();

        /* Before flipping the buffer, save any pending screenshots */
        if (screenshot[0]) {
                r_texture_t *tex;

                C_debug("Saving screenshot '%s'", screenshot);
                tex = R_texture_alloc(r_width.value.n, r_height.value.n, FALSE);
                R_texture_screenshot(tex, 0, 0);
                R_surface_save(tex->surface, screenshot);
                R_texture_free(tex);
                screenshot[0] = NUL;
        }

        SDL_GL_SwapBuffers();
        R_check_errors();
}

