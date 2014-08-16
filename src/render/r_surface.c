/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Functions for manipulating SDL surfaces */

#include "r_common.h"

/******************************************************************************\
 Gets a pixel from an SDL surface. Lock the surface before calling.
\******************************************************************************/
c_color_t R_surface_get(const SDL_Surface *surf, int x, int y)
{
        Uint8 *p, r, g, b, a;
        Uint32 pixel = -1;
        int bpp;

        bpp = surf->format->BytesPerPixel;
        p = (Uint8 *)surf->pixels + y * surf->pitch + x * bpp;
        switch(bpp) {
        case 1: pixel = *p;
                break;
        case 2: pixel = *(Uint16 *)p;
                break;
        case 3: pixel = SDL_BYTEORDER == SDL_BIG_ENDIAN ?
                        p[0] << 16 | p[1] << 8 | p[2] :
                        p[0] | p[1] << 8 | p[2] << 16;
                break;
        case 4: pixel = *(Uint32 *)p;
                break;
        default:
                C_error("Invalid surface format");
        }
        SDL_GetRGBA(pixel, surf->format, &r, &g, &b, &a);
        return C_color_rgba(r, g, b, a);
}

/******************************************************************************\
 Sets the color of a pixel on an SDL surface. Lock the surface before
 calling.
\******************************************************************************/
void R_surface_put(SDL_Surface *surf, int x, int y, c_color_t color)
{
        Uint8 *p;
        Uint32 pixel;
        int bpp;

        bpp = surf->format->BytesPerPixel;
        p = (Uint8 *)surf->pixels + y * surf->pitch + x * bpp;
        pixel = SDL_MapRGBA(surf->format, (Uint8)(255.f * color.r),
                            (Uint8)(255.f * color.g), (Uint8)(255.f * color.b),
                            (Uint8)(255.f * color.a));
        switch(bpp) {
        case 1: *p = pixel;
                break;
        case 2: *(Uint16 *)p = pixel;
                break;
        case 3: if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                        p[0] = (pixel >> 16) & 0xff;
                        p[1] = (pixel >> 8) & 0xff;
                        p[2] = pixel & 0xff;
                } else {
                        p[0] = pixel & 0xff;
                        p[1] = (pixel >> 8) & 0xff;
                        p[2] = (pixel >> 16) & 0xff;
                }
                break;
        case 4: *(Uint32 *)p = pixel;
                break;
        default:
                C_error("Invalid surface format");
        }
}

/******************************************************************************\
 Inverts a surface. Surface should not be locked when this is called.
\******************************************************************************/
void R_surface_invert(SDL_Surface *surf, int rgb, int alpha)
{
        c_color_t color;
        int x, y;

        if (SDL_LockSurface(surf) < 0) {
                C_warning("Failed to lock surface");
                return;
        }
        for (y = 0; y < surf->h; y++)
                for (x = 0; x < surf->w; x++) {
                        color = R_surface_get(surf, x, y);
                        if (rgb) {
                                color.r = 1.f - color.r;
                                color.g = 1.f - color.g;
                                color.b = 1.f - color.b;
                        }
                        if (alpha)
                                color.a = 1.f - color.a;
                        R_surface_put(surf, x, y, color);
                }
        SDL_UnlockSurface(surf);
}

/******************************************************************************\
 Overwrite's [dest]'s alpha channel with [src]'s intensity. If [dest] is
 larger than [src], [src] is tiled. Do not call on locked surfaces.
\******************************************************************************/
void R_surface_mask(SDL_Surface *dest, SDL_Surface *src)
{
        int x, y;

        if (SDL_LockSurface(dest) < 0) {
                C_warning("Failed to lock destination surface");
                return;
        }
        if (SDL_LockSurface(src) < 0) {
                C_warning("Failed to lock source surface");
                return;
        }
        for (y = 0; y < dest->h; y++)
                for (x = 0; x < dest->w; x++) {
                        c_color_t color, mask;

                        color = R_surface_get(dest, x, y);
                        mask = R_surface_get(src, x % src->w, y % src->h);
                        color.a = C_color_luma(mask);
                        R_surface_put(dest, x, y, color);
                }
        SDL_UnlockSurface(dest);
        SDL_UnlockSurface(src);
}

/******************************************************************************\
 Vertically flip [surf]'s pixels. Do not call on a locked surface.
\******************************************************************************/
void R_surface_flip_v(SDL_Surface *surf)
{
        int x, y;

        if (SDL_LockSurface(surf) < 0) {
                C_warning("Failed to lock surface");
                return;
        }
        for (y = 0; y < surf->h / 2; y++)
                for (x = 0; x < surf->w; x++) {
                        c_color_t color_top, color_bottom;

                        color_top = R_surface_get(surf, x, y);
                        color_bottom = R_surface_get(surf, x, surf->h - y - 1);
                        R_surface_put(surf, x, y, color_bottom);
                        R_surface_put(surf, x, surf->h - y - 1, color_top);
                }
        SDL_UnlockSurface(surf);
}

/******************************************************************************\
 Wrapper around surface allocation that keeps track of memory allocated.
\******************************************************************************/
SDL_Surface *R_surface_alloc(int width, int height, int alpha)
{
        SDL_Surface *surface;
        int flags;

        flags = SDL_HWSURFACE;
        if (alpha)
                flags |= SDL_SRCALPHA;
        surface = SDL_CreateRGBSurface(flags, width, height,
                                       r_sdl_format.BitsPerPixel,
                                       r_sdl_format.Rmask, r_sdl_format.Gmask,
                                       r_sdl_format.Bmask, r_sdl_format.Amask);
        SDL_SetAlpha(surface, SDL_RLEACCEL, SDL_ALPHA_OPAQUE);

        /* Keep track of allocated memory */
        r_video_mem += width * height * r_sdl_format.BytesPerPixel;
        if (r_video_mem > r_video_mem_high)
                r_video_mem_high = r_video_mem;

        return surface;
}

/******************************************************************************\
 Frees a surface, updating video memory estimate.
\******************************************************************************/
void R_surface_free(SDL_Surface *surface)
{
        if (!surface)
                return;

        /* Keep track of free'd memory */
        r_video_mem -= surface->w * surface->h * surface->format->BytesPerPixel;

        SDL_FreeSurface(surface);
}

/******************************************************************************\
 Low-level callbacks for the PNG library.
\******************************************************************************/
static void user_png_read(png_structp png_ptr, png_bytep data,
                          png_size_t length)
{
        C_file_read((c_file_t *)png_get_io_ptr(png_ptr), (char *)data,
                    (int)length);
}

static void user_png_write(png_structp png_ptr, png_bytep data,
                          png_size_t length)
{
        C_file_write((c_file_t *)png_get_io_ptr(png_ptr), (char *)data,
                     (int)length);
}

static void user_png_flush(png_structp png_ptr)
{
        C_file_flush((c_file_t *)png_get_io_ptr(png_ptr));
}

/******************************************************************************\
 Loads a PNG file and allocates a new SDL surface for it. Returns NULL on
 failure.

 Based on tutorial implementation in the libpng manual:
 http://www.libpng.org/pub/png/libpng-1.2.5-manual.html
\******************************************************************************/
SDL_Surface *R_surface_load_png(const char *filename, bool *alpha)
{
        SDL_Surface *surface;
        png_byte png_header[8];
        png_bytep row_pointers[4096];
        png_infop info_ptr;
        png_structp png_ptr;
        png_uint_32 width, height;
        c_file_t file;
        int i, bit_depth, color_type;

        surface = NULL;

        /* We have to read everything ourselves for libpng */
        if (!C_file_init_read(&file, filename)) {
                C_warning("Failed to open PNG '%s' for reading", filename);
                return NULL;
        }

        /* Check the first few bytes of the file to see if it is PNG format */
        C_file_read(&file, (char *)png_header, sizeof (png_header));
        if (png_sig_cmp(png_header, 0, sizeof (png_header))) {
                C_warning("'%s' is not in PNG format", filename);
                C_file_cleanup(&file);
                return NULL;
        }

        /* Allocate the PNG structures */
        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                         NULL, NULL, NULL);
        if (!png_ptr) {
                C_warning("Failed to allocate PNG read struct");
                return NULL;
        }

        /* Tells libpng that we've read a bit of the header already */
        png_set_sig_bytes(png_ptr, sizeof (png_header));

        /* Set read callback before proceeding */
        png_set_read_fn(png_ptr, (voidp)&file, (png_rw_ptr)user_png_read);

        /* If an error occurs in libpng, it will longjmp back here */
        if (setjmp(png_ptr->jmpbuf)) {
                C_warning("Error loading PNG '%s'", filename);
                goto cleanup;
        }

        /* Allocate a PNG info struct */
        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
                C_warning("Failed to allocate PNG info struct");
                goto cleanup;
        }

        /* Read the image info */
        png_read_info(png_ptr, info_ptr);
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
                     &color_type, NULL, NULL, NULL);

        /* If the image has an alpha channel we want to store it properly */
        *alpha = FALSE;
        if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
                *alpha = TRUE;

        /* Convert paletted images to RGB */
        if (color_type == PNG_COLOR_TYPE_PALETTE)
                png_set_palette_to_rgb(png_ptr);

        /* Convert transparent regions to alpha channel */
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
                png_set_tRNS_to_alpha(png_ptr);
                *alpha = TRUE;
        }

        /* Convert grayscale images to RGB */
        if (color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                png_set_expand(png_ptr);
                png_set_gray_to_rgb(png_ptr);
        }

        /* Crop 16-bit image data to 8-bit */
        if (bit_depth == 16)
                png_set_strip_16(png_ptr);

        /* Give opaque images an opaque alpha channel (ARGB) */
        if (!(color_type & PNG_COLOR_MASK_ALPHA))
                png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

        /* Convert 1-, 2-, and 4-bit samples to 8-bit */
        png_set_packing(png_ptr);

        /* Let libpng handle interlacing */
        png_set_interlace_handling(png_ptr);

        /* Update our image information */
        png_read_update_info(png_ptr, info_ptr);
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
                     &color_type, NULL, NULL, NULL);

        /* Sanity check */
        if (width < 1 || height < 1) {
                C_warning("PNG image '%s' has invalid dimensions %dx%d",
                          filename, width, height);
                goto cleanup;
        }
        if (height > sizeof (row_pointers)) {
                C_warning("PNG image '%s' is too tall (%dpx), cropping",
                          filename, height);
                height = sizeof (row_pointers);
        }

        /* Allocate the SDL surface and get image data */
        surface = R_surface_alloc(width, height, *alpha);
        if (SDL_LockSurface(surface) < 0) {
                C_warning("Failed to lock surface");
                goto cleanup;
        }
        for (i = 0; i < (int)height; i++)
                row_pointers[i] = (png_bytep)surface->pixels +
                                  surface->pitch * i;
        png_read_image(png_ptr, row_pointers);
        SDL_UnlockSurface(surface);

cleanup:
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        C_file_cleanup(&file);
        return surface;
}

/******************************************************************************\
 Write the contents of a surface as a PNG file. Returns TRUE if a file was
 written.
\******************************************************************************/
int R_surface_save(SDL_Surface *surface, const char *filename)
{
        struct tm *local;
        time_t msec;
        png_bytep row_pointers[4096];
        png_infop info_ptr;
        png_structp png_ptr;
        png_text text[2];
        png_time mod_time;
        c_file_t file;
        int i, height, success;
        char buf[64];

        if (!surface || surface->w < 1 || surface->h < 1)
                return FALSE;
        success = FALSE;

        /* We have to read everything ourselves for libpng */
        if (!C_file_init_write(&file, filename)) {
                C_warning("Failed to open PNG '%s' for writing", filename);
                return FALSE;
        }

        /* Allocate the PNG structures */
        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                          NULL, NULL, NULL);
        if (!png_ptr) {
                C_warning("Failed to allocate PNG write struct");
                return FALSE;
        }

        /* Set write callbacks before proceeding */
        png_set_write_fn(png_ptr, (voidp)&file, (png_rw_ptr)user_png_write,
                         (png_flush_ptr)user_png_flush);

        /* If an error occurs in libpng, it will longjmp back here */
        if (setjmp(png_ptr->jmpbuf)) {
                C_warning("Error saving PNG '%s'", filename);
                goto cleanup;
        }

        /* Allocate a PNG info struct */
        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
                C_warning("Failed to allocate PNG info struct");
                goto cleanup;
        }

        /* Sanity check */
        height = surface->h;
        if (height > 4096) {
                C_warning("Image too tall (%dpx), cropping", height);
                height = 4096;
        }

        /* Setup the info structure for writing our texture */
        png_set_IHDR(png_ptr, info_ptr, surface->w, height, 8,
                     PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

        /* Setup the comment text */
        text[0].key = "Title";
        text[0].text = PACKAGE_STRING;
        text[0].text_length = C_strlen(text[0].text);
        text[0].compression = PNG_TEXT_COMPRESSION_NONE;
        time(&msec);
        local = localtime(&msec);
        text[1].key = "Creation Time";
        text[1].text = buf;
        text[1].text_length = strftime(buf, sizeof (buf),
                                       "%d %b %Y %H:%M:%S GMT", local);
        text[1].compression = PNG_TEXT_COMPRESSION_NONE;
        png_set_text(png_ptr, info_ptr, text, 2);

        /* Set modified time */
        mod_time.day = local->tm_mday;
        mod_time.hour = local->tm_hour;
        mod_time.minute = local->tm_min;
        mod_time.second = local->tm_sec;
        mod_time.year = local->tm_year + 1900;
        png_set_tIME(png_ptr, info_ptr, &mod_time);

        /* Write image header */
        png_write_info(png_ptr, info_ptr);

        /* Write the image data */
        if (SDL_LockSurface(surface) < 0) {
                C_warning("Failed to lock surface");
                goto cleanup;
        }
        for (i = 0; i < (int)height; i++)
                row_pointers[i] = (png_bytep)surface->pixels +
                                  surface->pitch * i;
        png_write_image(png_ptr, row_pointers);
        png_write_end(png_ptr, NULL);
        SDL_UnlockSurface(surface);
        success = TRUE;

cleanup:
        png_destroy_write_struct(&png_ptr, &info_ptr);
        C_file_cleanup(&file);
        return success;
}

