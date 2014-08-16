/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Implements texture loading, manipulation, and management. Textures are
   stored in a linked list and should only be accessed during loading time so
   efficiency here is not important. */

#include "r_common.h"

/* Have to include this here because it defines stuff on archs except debian */
#include "SDL_Pango.h"

/* The wide range of 2D scaling can shrink fonts too small to be viewable,
   so we need to set a minimum (in points). */
#define FONT_SIZE_MIN 7

/* Font configuration variables */
extern c_var_t r_font_descriptions[R_FONTS];

/* Terrain tile sheet */
r_texture_t *r_terrain_tex;

/* White generated texture */
r_texture_t *r_white_tex;

/* SDL format used for textures */
SDL_PixelFormat r_sdl_format;

/* SDL_Pango context */
static SDLPango_Context *pango_context;

/* Specifies white back and transparent letter */
static const SDLPango_Matrix _MATRIX_WHITE_BACK_TRANSPARENT_LETTER =
{
 {
  {255, 255, 0, 0},
  {255, 255, 0, 0},
  {255, 255, 0, 0},
  {255, 0, 0, 0}
 }
};
static const SDLPango_Matrix *MATRIX_WHITE_BACK_TRANSPARENT_LETTER =
                                        &_MATRIX_WHITE_BACK_TRANSPARENT_LETTER;

/* Estimated video memory usage */
int r_video_mem, r_video_mem_high;

/* Font asset array */
static struct {
        int line_skip, width, height;
} fonts[R_FONTS];

static c_ref_t *root, *root_alloc;
static bool pango_inited;

/******************************************************************************\
 Frees memory associated with a texture.
\******************************************************************************/
static void texture_cleanup(r_texture_t *pt)
{
        R_surface_free(pt->surface);
        glDeleteTextures(1, &pt->gl_name);
        R_check_errors();
}

/******************************************************************************\
 Checks if a texture is non-power-of-two. Prints warnings if it is and NPOT
 textures are not supported.
\******************************************************************************/
static void texture_check_npot(r_texture_t *pt)
{
        if (!pt || !pt->surface ||
            (C_is_pow2(pt->surface->w) && C_is_pow2(pt->surface->h))) {
                pt->not_pow2 = FALSE;
                return;
        }
        pt->not_pow2 = TRUE;
        pt->pow2_w = C_next_pow2(pt->surface->w);
        pt->pow2_h = C_next_pow2(pt->surface->h);
        pt->uv_scale = C_vec2((float)pt->surface->w / pt->pow2_w,
                              (float)pt->surface->h / pt->pow2_h);
        if (r_ext.npot_textures)
                return;
        C_trace("Texture '%s' not power-of-two: %dx%d",
                 pt->ref.name, pt->surface->w, pt->surface->h);
}

/*****************************************************************************\
 Just allocate a texture object and cleared [width] by [height] SDL surface.
 Does not load anything or add the texture to the texture linked list. The
 texture needs to be uploaded after calling this function.
\******************************************************************************/
r_texture_t *R_texture_alloc_full(const char *file, int line, const char *func,
                                  int width, int height, int alpha)
{
        static int count;
        SDL_Rect rect;
        r_texture_t *pt;

        if (width < 1 || height < 1)
                C_error_full(file, line, func,
                             "Invalid texture dimensions: %dx%d",
                             width, height);
        pt = C_recalloc_full(file, line, func, NULL, sizeof (*pt));
        pt->ref.refs = 1;
        pt->ref.cleanup_func = (c_ref_cleanup_f)texture_cleanup;

        /* Add texture to the allocated textures linked list */
        if (root_alloc) {
                pt->ref.next = root_alloc;
                root_alloc->prev = &pt->ref;
        }
        pt->ref.root = &root_alloc;
        root_alloc = &pt->ref;
        if (c_mem_check.value.n)
                C_strncpy_buf(pt->ref.name,
                              C_va("Texture #%d allocated by %s()",
                                   ++count, func));

        /* Allocate the texture memory and fill it with black */
        pt->alpha = alpha;
        pt->surface = R_surface_alloc(width, height, alpha);
        rect.x = 0;
        rect.y = 0;
        rect.w = width;
        rect.h = height;
        SDL_FillRect(pt->surface, &rect, 0);

        /* Check if texture is power-of-two */
        texture_check_npot(pt);

        /* Give the texture a unique OpenGL name */
        glGenTextures(1, &pt->gl_name);

        R_check_errors();
        if (c_mem_check.value.n)
                C_trace_full(file, line, func, "Allocated texture #%d", count);
        return pt;
}

/******************************************************************************\
 Reads in an area of the screen (current buffer) from ([x], [y]) the size of
 the texture. You must manually upload the texture after calling this function.
\******************************************************************************/
void R_texture_screenshot(r_texture_t *texture, int x, int y)
{
        SDL_Surface *video;

        video = SDL_GetVideoSurface();
        if (SDL_LockSurface(texture->surface) < 0) {
                C_warning("Failed to lock texture: %s", SDL_GetError());
                return;
        }
        glReadPixels(x, r_height.value.n - texture->surface->h - y,
                     texture->surface->w, texture->surface->h,
                     GL_RGBA, GL_UNSIGNED_BYTE, texture->surface->pixels);
        R_surface_flip_v(texture->surface);
        R_check_errors();
        SDL_UnlockSurface(texture->surface);
}

/******************************************************************************\
 Allocates a new texture and copies the contents of another texture to it.
 The texture must be uploaded after calling this function.
\******************************************************************************/
r_texture_t *R_texture_clone_full(const char *file, int line, const char *func,
                                  const r_texture_t *src)
{
        r_texture_t *dest;

        if (!src)
                return NULL;
        dest = R_texture_alloc_full(file, line, func, src->surface->w,
                                    src->surface->h, src->alpha);
        if (!dest)
                return NULL;
        if (SDL_BlitSurface(src->surface, NULL, dest->surface, NULL) < 0)
                C_warning("Failed to clone texture '%s': %s",
                          src->ref.name, SDL_GetError());
        dest->mipmaps = src->mipmaps;
        return dest;
}

/******************************************************************************\
 If the texture's SDL surface has changed, the image must be reloaded into
 OpenGL. This function will do this. It is assumed that the texture surface
 format has not changed since the texture was created. Note that mipmaps will
 make UI textures look blurry so do not use them for anything that will be
 rendered in 2D mode.
\******************************************************************************/
void R_texture_upload(const r_texture_t *pt)
{
        SDL_Surface *surface, *pow2_surface;
        int gl_internal;

        /* If this is a non-power-of-two texture, paste it onto a larger
           power-of-two surface first */
        surface = pt->surface;
        pow2_surface = NULL;
        if (pt->not_pow2) {
                SDL_Rect rect;

                pow2_surface = R_surface_alloc(pt->pow2_w, pt->pow2_h,
                                               pt->alpha);
                rect.x = 0;
                rect.y = 0;
                rect.w = pt->surface->w;
                rect.h = pt->surface->h;
                SDL_BlitSurface(pt->surface, NULL, pow2_surface, &rect);
                surface = pow2_surface;
        }

        /* Select the OpenGL internal texture format */
        if (pt->alpha) {
                gl_internal = GL_RGBA;
                if (r_color_bits.value.n == 16)
                        gl_internal = GL_RGBA4;
                else if (r_color_bits.value.n == 32)
                        gl_internal = GL_RGBA8;
        } else {
                gl_internal = GL_RGB;
                if (r_color_bits.value.n == 16)
                        gl_internal = GL_RGB5;
                else if (r_color_bits.value.n == 32)
                        gl_internal = GL_RGB8;
        }

        /* Upload the texture to OpenGL and build mipmaps */
        glBindTexture(GL_TEXTURE_2D, pt->gl_name);
        if (pt->mipmaps)
                gluBuild2DMipmaps(GL_TEXTURE_2D, gl_internal,
                                  surface->w, surface->h,
                                  GL_RGBA, GL_UNSIGNED_BYTE,
                                  surface->pixels);
        else
                glTexImage2D(GL_TEXTURE_2D, 0, gl_internal,
                             surface->w, surface->h, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);

        /* Free the temporary surface if we used it */
        R_surface_free(pow2_surface);

        R_check_errors();
}

/******************************************************************************\
 Unload all textures from OpenGL.
\******************************************************************************/
void R_dealloc_textures(void)
{
        r_texture_t *tex;

        C_debug("Deallocting loaded textures");
        tex = (r_texture_t *)root;
        while (tex) {
                glDeleteTextures(1, &tex->gl_name);
                tex = (r_texture_t *)tex->ref.next;
        }

        C_debug("Deallocating allocated textures");
        tex = (r_texture_t *)root_alloc;
        while (tex) {
                glDeleteTextures(1, &tex->gl_name);
                tex = (r_texture_t *)tex->ref.next;
        }
}

/******************************************************************************\
 Reuploads all textures in the linked list to OpenGL.
\******************************************************************************/
void R_realloc_textures(void)
{
        r_texture_t *tex;

        C_debug("Uploading loaded textures");
        tex = (r_texture_t *)root;
        while (tex) {
                glGenTextures(1, &tex->gl_name);
                R_texture_upload(tex);
                tex = (r_texture_t *)tex->ref.next;
        }

        C_debug("Uploading allocated textures");
        tex = (r_texture_t *)root_alloc;
        while (tex) {
                glGenTextures(1, &tex->gl_name);
                R_texture_upload(tex);
                tex = (r_texture_t *)tex->ref.next;
        }
}

/******************************************************************************\
 Loads a texture from file. If the texture has already been loaded, just ups
 the reference count and returns the loaded surface. If the texture failed to
 load, returns NULL.
\******************************************************************************/
r_texture_t *R_texture_load(const char *filename, int mipmaps)
{
        r_texture_t *pt;
        int found;

        if (!filename || !filename[0])
                return NULL;

        /* Find a place for the texture */
        pt = C_ref_alloc(sizeof (*pt), &root, (c_ref_cleanup_f)texture_cleanup,
                         filename, &found);
        if (found)
                return pt;

        /* Load the image file */
        pt->mipmaps = mipmaps;
        pt->surface = R_surface_load_png(filename, &pt->alpha);
        if (!pt->surface) {
                C_ref_down(&pt->ref);
                return NULL;
        }

        /* Check if texture is power-of-two */
        texture_check_npot(pt);

        /* Load the texture into OpenGL */
        glGenTextures(1, &pt->gl_name);
        R_texture_upload(pt);
        R_check_errors();

        return pt;
}

/******************************************************************************\
 Selects (binds) a texture for rendering in OpenGL. Also sets whatever options
 are necessary to get the texture to show up properly.
\******************************************************************************/
void R_texture_select(const r_texture_t *texture)
{
        if (!texture || !r_textures.value.n ||
            (r_textures.value.n == 2 && texture->not_pow2)) {
                glDisable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, 0);
                glDisable(GL_BLEND);
                glDisable(GL_ALPHA_TEST);
                return;
        }

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture->gl_name);

        /* Repeat wrapping (not supported for NPOT textures) */
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        /* Mipmaps (not supported for NPOT textures) */
        if (texture->mipmaps) {
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_LINEAR);
                if (texture->mipmaps > 1)
                        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD,
                                        (GLfloat)texture->mipmaps);
        } else
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        /* Anisotropic filtering */
        if (r_ext.anisotropy > 1.f) {
                GLfloat aniso;

                aniso = texture->anisotropy;
                if (aniso > r_ext.anisotropy)
                        aniso = r_ext.anisotropy;
                if (aniso < 1.f)
                        aniso = 1.f;
                glTexParameterf(GL_TEXTURE_2D,
                                GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
        }

        /* Additive blending */
        if (texture->additive) {
                glEnable(GL_BLEND);
                glDisable(GL_ALPHA_TEST);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        } else {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                /* Alpha blending */
                if (texture->alpha) {
                        glEnable(GL_BLEND);
                        glEnable(GL_ALPHA_TEST);
                } else {
                        glDisable(GL_BLEND);
                        glDisable(GL_ALPHA_TEST);
                }
        }

        /* Non-power-of-two textures are pasted onto larger textures that
           require a texture coordinate transformation */
        if (texture->not_pow2) {
                glMatrixMode(GL_TEXTURE);
                glLoadIdentity();
                glScalef(texture->uv_scale.x, texture->uv_scale.y, 1.f);
        } else {
                glMatrixMode(GL_TEXTURE);
                glLoadIdentity();
        }
        glMatrixMode(GL_MODELVIEW);

        R_check_errors();
}

/******************************************************************************\
 Renders a plain quad without any sprite effects. Coordinates refer to the
 upper left-hand corner.
\******************************************************************************/
void R_texture_render(r_texture_t *tex, int x, int y)
{
        r_vertex2_t verts[4];
        unsigned short indices[] = {0, 1, 2, 3};

        verts[0].co = C_vec3(0.f, 0.f, 0.f);
        verts[0].uv = C_vec2(0.f, 0.f);
        verts[1].co = C_vec3(0.f, (float)tex->surface->h, 0.f);
        verts[1].uv = C_vec2(0.f, 1.f);
        verts[2].co = C_vec3((float)tex->surface->w,
                             (float)tex->surface->h, 0.f);
        verts[2].uv = C_vec2(1.f, 1.f);
        verts[3].co = C_vec3((float)tex->surface->w, 0.f, 0.f);
        verts[3].uv = C_vec2(1.f, 0.f);
        R_push_mode(R_MODE_2D);
        R_texture_select(tex);
        glTranslatef((GLfloat)x, (GLfloat)y, 0.f);
        glInterleavedArrays(R_VERTEX2_FORMAT, 0, verts);
        glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices);
        R_check_errors();
        R_pop_mode();
}

/******************************************************************************\
 Returns the height of the tallest glyph in the font.
\******************************************************************************/
int R_font_height(r_font_t font)
{
        return fonts[font].height;
}

/******************************************************************************\
 Returns the width of the widest glyph in the font.
\******************************************************************************/
int R_font_width(r_font_t font)
{
        return fonts[font].width;
}

/******************************************************************************\
 Returns distance from start of one line to the start of the next.
\******************************************************************************/
int R_font_line_skip(r_font_t font)
{
        return fonts[font].line_skip;
}

/******************************************************************************\
 Returns text with the appropriate pango markup applied
\******************************************************************************/
char *R_font_apply(r_font_t font, const char *text)
{
        return C_va("<span font=\"%s\">%s</span>",
                    r_font_descriptions[font].value.s, text);
}

/******************************************************************************\
 Returns the X position is measured from the left edge of the line to the
 trailing edge of the grapheme at [index]
\******************************************************************************/
int R_font_index_to_x(r_font_t font, const char *text, int index)
{
        PangoLayout* layout;
        char *s;
        int line, x_pos;
        s = R_font_apply(font, text);
        SDLPango_SetMarkup(pango_context, s, -1);
        SDLPango_SetMinimumSize(pango_context, -1, 0);
        layout = SDLPango_GetPangoLayout(pango_context);
        pango_layout_index_to_line_x(layout, index, 0, &line, &x_pos);
        return PANGO_PIXELS(x_pos/r_scale_2d);
}

/******************************************************************************\
 Returns the bounding box of the rendered text.
\******************************************************************************/
c_vec2_t R_font_size(r_font_t font, const char *text)
{
        char *s;
        int w, h;

        s = R_font_apply(font, text);
        SDLPango_SetMinimumSize(pango_context, -1, 0);
        SDLPango_SetMarkup(pango_context, s, -1);
        SDLPango_SetMinimumSize(pango_context, -1, 0);
        w = SDLPango_GetLayoutWidth(pango_context);
        h = SDLPango_GetLayoutHeight(pango_context);

        return C_vec2((float)w, (float)h);
}

/******************************************************************************\
 Renders a line of text onto a newly-allocated SDL surface. This surface must
 be freed by the caller.
\******************************************************************************/
r_texture_t *R_font_render(r_font_t font, float wrap, int invert,
                           const char *text, int *width, int *height)
{
        const SDLPango_Matrix *colour_matrix;
        r_texture_t *tex;
        char *s;


        s = R_font_apply(font, text);
        if(invert)
                colour_matrix = MATRIX_WHITE_BACK_TRANSPARENT_LETTER;
        else
                colour_matrix = MATRIX_TRANSPARENT_BACK_WHITE_LETTER;

        SDLPango_SetDefaultColor(pango_context, colour_matrix);
        SDLPango_SetMinimumSize(pango_context,
                                (wrap > 0) ? (int)wrap : -1, 0);
        SDLPango_SetMarkup(pango_context, s, -1);
        *width = SDLPango_GetLayoutWidth(pango_context);
        *height = SDLPango_GetLayoutHeight(pango_context);

        if (++*width < 2 || *height < 2)
                return NULL;
        tex = R_texture_alloc(*width, *height, TRUE);
        SDLPango_Draw(pango_context, tex->surface, 0, 0);
        return tex;
}

/******************************************************************************\
 Setups the font heights, line_skip and width
\******************************************************************************/
static void setup_font(r_font_t font, c_var_t *var)
{
        C_zero(fonts + font);
        fonts[font].height = (int)R_font_size(font, "A").y;
        fonts[font].line_skip = (int)R_font_size(font, "A\nA").y - fonts[font].height;
        fonts[font].width = (int)R_font_size(font, "W").x;
}

/******************************************************************************\
 Reset font variables to stock values.
\******************************************************************************/
void R_stock_fonts(void)
{
        int i;

        for (i = 0; i < R_FONTS; i++) {
                C_var_reset(r_font_descriptions + i);
        }
}

/******************************************************************************\
 Initialize the SDL Pango library and generate font structures.
\******************************************************************************/
void R_load_fonts(void)
{
        int i;

        if (!pango_inited)
                return;

        /* We can't print to the graphic console if our fonts aren't loaded */
        c_log_mode = C_LM_NO_FUNC;

        pango_context = SDLPango_CreateContext();
        SDLPango_SetDpi(pango_context, R_TEXT_DPI*r_scale_2d,
                        R_TEXT_DPI*r_scale_2d);
        SDLPango_SetDefaultColor(pango_context,
                                 MATRIX_TRANSPARENT_BACK_WHITE_LETTER);
        /* Load fonts */
        for (i = 0; i < R_FONTS; i++) {
                C_var_unlatch(r_font_descriptions + i);
                setup_font(i, r_font_descriptions + i);
        }


        /* We can print to the graphic console again */
        c_log_mode = C_LM_NORMAL;
}

/******************************************************************************\
 Loads render assets.
\******************************************************************************/
void R_load_assets(void)
{

        C_status("Loading render assets");
        C_var_unlatch(&r_model_lod);

        /* Setup the texture pixel format, RGBA in 32 bits */
        r_sdl_format.BitsPerPixel = 32;
        r_sdl_format.BytesPerPixel = 4;
        r_sdl_format.Amask = 0xff000000;
        r_sdl_format.Bmask = 0x00ff0000;
        r_sdl_format.Gmask = 0x0000ff00;
        r_sdl_format.Rmask = 0x000000ff;
        r_sdl_format.Ashift = 24;
        r_sdl_format.Bshift = 16;
        r_sdl_format.Gshift = 8;
        r_sdl_format.alpha = 255;

        /* Initialize SDL_Pango library */
        SDLPango_Init();
        pango_inited = TRUE;
        R_load_fonts();

        /* Generate procedural content */
        r_terrain_tex = R_texture_load("models/globe/terrain.png", TRUE);
        R_prerender();
        if (!r_terrain_tex)
                C_error("Failed to load terrain texture");
        r_terrain_tex->anisotropy = 2.f;

        /* Create a fake white texture */
        r_white_tex = R_texture_alloc(1, 1, FALSE);
        if (SDL_LockSurface(r_white_tex->surface) >= 0) {
                R_surface_put(r_white_tex->surface, 0, 0,
                              C_color(1.f, 1.f, 1.f, 1.f));
                SDL_UnlockSurface(r_white_tex->surface);
        } else
                C_warning("Failed to lock white texture");
        R_texture_upload(r_white_tex);
}

/******************************************************************************\
 Cleans up fonts.
\******************************************************************************/
void R_free_fonts(void)
{
        if (!pango_inited)
                return;

        SDLPango_FreeContext(pango_context);
}

/******************************************************************************\
 Cleans up the asset resources. Shuts down the SDL_Pango library.
\******************************************************************************/
void R_free_assets(void)
{
        /* Print out estimated memory usage */
        if (c_mem_check.value.n)
                C_debug("Estimated video memory high mark %.1fmb",
                        r_video_mem_high / (1024.f * 1024.f));

        R_texture_free(r_terrain_tex);
        R_texture_free(r_white_tex);
        R_free_fonts();
}

/******************************************************************************\
 Update vertex buffer data.
\******************************************************************************/
void R_vbo_update(r_vbo_t *vbo)
{
        r_ext.glBindBuffer(GL_ARRAY_BUFFER, vbo->vertices_name);
        r_ext.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo->indices_name);
        if (vbo->vertices)
                r_ext.glBufferData(GL_ARRAY_BUFFER,
                                   vbo->vertices_len * vbo->vertex_size,
                                   vbo->vertices, GL_STATIC_DRAW);
        if (vbo->indices)
                r_ext.glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                   vbo->indices_len * sizeof (unsigned short),
                                   vbo->indices, GL_STATIC_DRAW);
        r_ext.glBindBuffer(GL_ARRAY_BUFFER, 0);
        r_ext.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        R_check_errors();
}

/******************************************************************************\
 Upload vertex buffer object data.
\******************************************************************************/
static void vbo_upload(r_vbo_t *vbo)
{
        int size;

        vbo->init_frame = c_frame;
        if (!r_ext.vertex_buffers)
                return;

        /* First upload the vertex data */
        vbo->vertices_name = 0;
        if (vbo->vertices) {
                r_video_mem += size = vbo->vertex_size * vbo->vertices_len;
                r_ext.glGenBuffers(1, &vbo->vertices_name);
        }

        /* Now upload the indices */
        vbo->indices_name = 0;
        if (vbo->indices) {
                r_video_mem += size = vbo->indices_len * sizeof (short);
                r_ext.glGenBuffers(1, &vbo->indices_name);
        }

        /* Keep track of video memory */
        if (r_video_mem > r_video_mem_high)
                r_video_mem_high = r_video_mem;

        R_check_errors();
        R_vbo_update(vbo);
}

/******************************************************************************\
 Bind and render a vertex buffer object.
\******************************************************************************/
void R_vbo_render(r_vbo_t *vbo)
{
#ifdef WINDOWS
        /* Windows will lose everything in video memory if the resolution is
           changed, so we need to check for this and reupload */
        if (r_init_frame > vbo->init_frame)
                vbo_upload(vbo);
#endif

        /* Use vertex buffer objects if supported */
        if (r_ext.vertex_buffers) {
                r_ext.glBindBuffer(GL_ARRAY_BUFFER, vbo->vertices_name);
                r_ext.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo->indices_name);
                glInterleavedArrays(vbo->vertex_format, vbo->vertex_size, NULL);
                if (vbo->indices)
                        glDrawElements(GL_TRIANGLES, vbo->indices_len,
                                       GL_UNSIGNED_SHORT, NULL);
                else
                        glDrawArrays(GL_TRIANGLES, 0, vbo->vertices_len);
                r_ext.glBindBuffer(GL_ARRAY_BUFFER, 0);
                r_ext.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

        /* Otherwise just make the rendering calls */
        else {
                glInterleavedArrays(vbo->vertex_format, vbo->vertex_size,
                                    vbo->vertices);
                if (vbo->indices)
                        glDrawElements(GL_TRIANGLES, vbo->indices_len,
                                       GL_UNSIGNED_SHORT, vbo->indices);
                else
                        glDrawArrays(GL_TRIANGLES, 0, vbo->vertices_len);
        }

        /* Make sure these are off after the interleaved array calls */
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);

        R_check_errors();
}

/******************************************************************************\
 Cleanup a vertex buffer object.
\******************************************************************************/
void R_vbo_cleanup(r_vbo_t *vbo)
{
        if (!vbo)
                return;
        if (r_ext.vertex_buffers) {
                if (vbo->vertices_name) {
                        r_ext.glDeleteBuffers(1, &vbo->vertices_name);
                        r_video_mem -= vbo->vertex_size * vbo->vertices_len;
                }
                if (vbo->indices_name) {
                        r_ext.glDeleteBuffers(1, &vbo->indices_name);
                        r_video_mem -= vbo->indices_len * sizeof (short);
                }
        }
        C_zero(vbo);
}

/******************************************************************************\
 Initialize a vertex buffer object. Assumes unsigned short indices. Note that
 all data must remain in place as the [r_vbo_t] structure only stores pointers.
\******************************************************************************/
void R_vbo_init(r_vbo_t *vbo, void *vertices, int vertices_len, int vertex_size,
                int vertex_format, void *indices, int indices_len)
{
        vbo->vertices = vertices;
        vbo->vertices_len = vertices_len;
        vbo->vertex_size = vertex_size;
        vbo->vertex_format = vertex_format;
        vbo->indices = indices;
        vbo->indices_len = indices_len;
        vbo_upload(vbo);
}

