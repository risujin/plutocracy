/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* This is the largest value that pixels can be stretched. This means that all
   GUI textures need to be created this many times larger than normal. The
   largest supported resoluton is WQXGA 2560x1600 (nVidia Geforce 8800GT)
   which requires a pixel scale of 2 to have a 2D display area of approximately
   1280x800. */
#define R_PIXEL_SCALE_MIN 0.5f
#define R_PIXEL_SCALE_MAX 2.0f

/* Ranges for zooming in and out. Maximum zoom distance is a scale value of
   the globe radius. */
#define R_ZOOM_MIN 8.f
#define R_ZOOM_MAX_SCALE 0.5f

/* OpenGL cannot address enough vertices to render more than 5 subdivisons'
   worth of tiles */
#define R_TILES_MAX 20480

/* The rotation speed of the sun around the globe has to be fixed so that there
   are no synchronization errors between players */
#define R_MINUTES_PER_DAY 5

/* Because of prerendering back buffer usage, we need a minimum resolution */
#define R_WIDTH_MIN 512
#define R_HEIGHT_MIN 384

/* Maximum number of globe 4-subdivision iterations */
#define R_SUBDIV4_MAX 5

/* Rendering field-of-view in degrees */
#define R_FOV 90.f

/* tan(R_FOV / 2) */
#define R_FOV_HALF_TAN 1.f

/* Longest renderable ship path. This is used throughout the program. The
   two biggest constraints on this value are network transmission and stack
   space usage. */
#define R_PATH_MAX 256

/* There is a fixed set of fonts available for the game */
typedef enum {
        R_FONT_CONSOLE,
        R_FONT_GUI,
        R_FONT_TITLE,
        R_FONT_LCD,
        R_FONTS
} r_font_t;

/* Terrain enumeration */
typedef enum {
        R_T_SHALLOW = 0,
        R_T_SAND = 1,
        R_T_GROUND = 2,
        R_T_GROUND_HOT = 3,
        R_T_GROUND_COLD = 4,
        R_T_WATER = 5,
        R_T_BASES = 3,
        R_T_TRANSITION = 6,
} r_terrain_t;

/* Tile selection types */
typedef enum {
        R_ST_ARROW,
        R_ST_DOT,
        R_ST_GOTO,
        R_ST_TILE,
        R_ST_BORDER,
        R_ST_DASHED_BORDER,
        R_SELECT_TYPES,
        R_ST_NONE,
} r_select_type_t;

/* Model selection types */
typedef enum {
        R_MS_NONE,
        R_MS_SELECTED,
        R_MS_HOVER,
} r_model_select_t;

/* Opaque texture object */
typedef struct r_texture r_texture_t;

/* Model instance type */
typedef struct r_model {
        PyObject_HEAD;
        struct r_model_data *data;
        r_model_select_t selected;
        c_vec3_t origin, normal, forward;
        c_color_t additive, modulate;
        GLfloat matrix[16];
        float scale;
        int anim, frame, last_frame, last_frame_time, time_left;
        bool unlit;
} r_model_t;

/* 2D textured quad sprite, can only be rendered in 2D mode */
typedef struct r_sprite {
        r_texture_t *texture;
        c_vec2_t origin, size;
        c_color_t modulate;
        float angle, z;
        bool unscaled;
} r_sprite_t;

/* A point sprite in world space */
typedef struct r_billboard {
        r_sprite_t sprite;
        c_vec3_t world_origin;
        float size;
        bool z_scale;
} r_billboard_t;

/* Sometimes it is convenient to store the source text for a text sprite in a
   generic buffer and only re-render it if it has changed */
typedef struct r_text {
        r_sprite_t sprite;
        r_font_t font;
        float wrap, shadow;
        int frame;
        char buffer[256];
        bool invert;
} r_text_t;

/* A quad strip composed of nine quads that stretch with the size parameter
   of the sprite. Adjust sprite parameters to change how the window is
   displayed. */
typedef struct r_window {
        r_sprite_t sprite;
        c_vec2_t corner;
} r_window_t;

/* Structure that contains configuration parameters for a tile */
typedef struct r_tile {
        r_terrain_t terrain;
        c_vec3_t forward, normal, origin;
        float height;
} r_tile_t;

/* r_assets.c */
void R_free_fonts(void);
int R_font_height(r_font_t);
int R_font_line_skip(r_font_t);
char *R_font_apply(r_font_t, const char *);
int R_font_index_to_x(r_font_t, const char *, int);
c_vec2_t R_font_size(r_font_t, const char *);
int R_font_width(r_font_t);
void R_load_fonts(void);
void R_stock_fonts(void);
#define R_texture_free(t) C_ref_down((c_ref_t *)(t))
r_texture_t *R_texture_load(const char *filename, int mipmaps);
#define R_texture_ref(t) C_ref_up((c_ref_t *)(t))

/* r_camera.c */
void R_grab_cam(void);
void R_move_cam_by(c_vec2_t distances);
c_vec3_t R_project(c_vec3_t);
c_vec3_t R_project_by_cam(c_vec3_t);
void R_release_cam(void);
void R_rotate_cam_by(c_vec3_t angles);
void R_rotate_cam_to(c_vec3_t origin);
c_vec3_t R_rotate_from_cam(c_vec3_t);
c_vec3_t R_rotate_to_cam(c_vec3_t);
void R_zoom_cam_by(float);

/* r_globe.c */
void R_finish_globe(void);
void R_hover_tile(int tile, r_select_type_t);
void R_render_border(int tile, c_color_t, bool);
void R_select_path(int tile, const char *path);
void R_select_tile(int tile, r_select_type_t);
void R_start_globe(void);

extern float r_globe_light, r_globe_radius, r_zoom_max;

/* r_mode.c */
void R_cleanup(void);
void R_clip_left(float);
void R_clip_top(float);
void R_clip_right(float);
void R_clip_bottom(float);
void R_clip_rect(c_vec2_t origin, c_vec2_t size);
void R_clip_disable(void);
void R_finish_frame(void);
void R_init(void);
#define R_pixel_clamp(v) C_vec2_clamp((v), r_scale_2d)
void R_pop_clip(void);
void R_push_clip(void);
const char *R_save_screenshot(void);
void R_start_frame(void);
void R_render_status(void);

extern c_count_t r_count_faces;
extern c_vec3_t r_cam_forward, r_cam_normal, r_cam_origin;
extern float r_cam_zoom, r_scale_2d;
extern int r_width_2d, r_height_2d, r_restart, r_scale_2d_frame;

/* r_model.c */
r_model_t *R_model_init(const char *filename, bool cull);
void R_model_play(r_model_t *, const char *anim_name);
void R_model_render(r_model_t *);

/* r_solar.c */
void R_adjust_light_for(c_vec3_t origin);

/* r_ship.c */
void R_render_ship_boarding(c_vec3_t origin_a, c_vec3_t origin_b, c_color_t);
void R_render_ship_status(const r_model_t *, float left, float left_max,
                          float right, float right_max, c_color_t modulate,
                          bool selected, bool own);

/* r_solar.c */
extern c_color_t r_fog_color;
extern float r_solar_angle;

/* r_sprite.c */
#define R_billboard_cleanup(p) R_sprite_cleanup(&(p)->sprite)
void R_billboard_init(r_billboard_t *, r_texture_t *);
void R_billboard_load(r_billboard_t *, const char *);
void R_billboard_render(r_billboard_t *);
void R_fill_screen(c_color_t);
void R_sprite_cleanup(r_sprite_t *);
void R_sprite_init(r_sprite_t *, r_texture_t *);
void R_sprite_init_text(r_sprite_t *, r_font_t, float wrap, float shadow,
                        int invert, const char *text);
void R_sprite_load(r_sprite_t *, const char *filename);
void R_sprite_render(const r_sprite_t *);
#define R_text_init(t) C_zero(t)
void R_text_configure(r_text_t *, r_font_t, float wrap, float shadow,
                      int invert, const char *text);
#define R_text_cleanup(t) R_sprite_cleanup(&(t)->sprite)
void R_text_render(r_text_t *);
#define R_window_cleanup(w) R_sprite_cleanup(&(w)->sprite)
void R_window_init(r_window_t *, r_texture_t *);
void R_window_load(r_window_t *, const char *filename);
void R_window_render(r_window_t *);

/* r_terrain.c */
void R_configure_globe(void);
void R_generate_globe(int subdiv4);
void R_tile_coords(int index, c_vec3_t verts[3]);
float R_tile_latitude(int tile);
void R_tile_neighbors(int tile, int neighbors[3]);
int R_tile_region(int tile, int neighbors[12]);
int R_land_bridge(int tile_a, int tile_b);
r_terrain_t R_terrain_base(r_terrain_t);
const char *R_terrain_to_string(r_terrain_t);
int R_water_terrain(int terrain);

extern r_tile_t r_tiles[R_TILES_MAX];
extern int r_tiles_max;

/* r_tests.c */
void R_free_test_assets(void);
void R_load_test_assets(void);
void R_render_test_line(c_vec3_t from, c_vec3_t to, c_color_t);

/* r_variables.c */
void R_register_variables(void);

extern c_var_t r_color_bits, r_gamma, r_height, r_multisample, r_pixel_scale,
               r_width, r_windowed;

