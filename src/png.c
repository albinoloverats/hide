#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>

#include <png.h>

#include "imagine.h"

static bool is_png(char *file_name)
{
    FILE *fp = fopen(file_name, "rb");
    if (!fp)
        return false;

    uint8_t header[8];
    fread(header, 1, 8, fp);
    fclose(fp);

    return !png_sig_cmp(header, 0, 8);
}

static int read_png(image_info_t *image_info)
{
    errno = EXIT_SUCCESS;

    FILE *fp = fopen(image_info->file, "rb");
    if (!fp)
        return errno;

    uint8_t header[8];
    fread(header, 1, 8, fp);

    /* initialize stuff */
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        goto cf;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        goto cleanup;

    /*
     * libpng uses setjmp as a replacement for exceptions; so “catch”
     * them and handle them our own way
     */
    if (setjmp(png_jmpbuf(png_ptr)))
        goto cleanup;

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    image_info->width = png_get_image_width(png_ptr, info_ptr);
    image_info->height = png_get_image_height(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    image_info->extra = malloc(sizeof bit_depth);
    memcpy(image_info->extra, &bit_depth, sizeof bit_depth);

    switch (png_get_color_type(png_ptr, info_ptr))
    {
        case PNG_COLOR_TYPE_RGB:
            image_info->bpp = 3;
            break;
        case PNG_COLOR_TYPE_RGBA:
            image_info->bpp = 4;
            break;
        default:
            goto cleanup;
    }

    //number_of_passes = png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    /* read file */
    if (setjmp(png_jmpbuf(png_ptr)))
        goto cleanup;

    image_info->buffer = (uint8_t **)malloc(sizeof (uint8_t *) * image_info->height);
    for (uint64_t y = 0; y < image_info->height; y++)
        image_info->buffer[y] = malloc(image_info->width * image_info->bpp);

    png_read_image(png_ptr, image_info->buffer);

cleanup:
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
cf:
    fclose(fp);

    return errno;
}

static int write_png(image_info_t image_info)
{
    errno = EXIT_SUCCESS;

    /* create file */
    FILE *fp = fopen(image_info.file, "wb");
    if (!fp)
        return errno;

    /* initialize stuff */
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        goto cf;

    png_set_compression_level(png_ptr, 9/*Z_BEST_COMPRESSION*/);

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        goto cleanup;

    if (setjmp(png_jmpbuf(png_ptr)))
        goto cleanup;

    png_init_io(png_ptr, fp);

    /* write header */
    if (setjmp(png_jmpbuf(png_ptr)))
        goto cleanup;

    png_byte color_type;
    switch (image_info.bpp)
    {
        case 3:
            color_type = PNG_COLOR_TYPE_RGB;
            break;
        case 4:
            color_type = PNG_COLOR_TYPE_RGBA;
            break;
        default:
            goto cleanup;
    }

    png_byte bit_depth;
    memcpy(&bit_depth, image_info.extra, sizeof bit_depth);
    png_set_IHDR(png_ptr, info_ptr, image_info.width, image_info.height, bit_depth,
             color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    free(image_info.extra);

    png_write_info(png_ptr, info_ptr);

    /* write bytes */
    if (setjmp(png_jmpbuf(png_ptr)))
        goto cleanup;

    png_write_image(png_ptr, image_info.buffer);

    /* end write */
    if (setjmp(png_jmpbuf(png_ptr)))
        goto cleanup;

    png_write_end(png_ptr, NULL);

    /* clean up heap allocation */
    for (uint64_t y = 0; y < image_info.height; y++)
        free(image_info.buffer[y]);
    free(image_info.buffer);

cleanup:
    png_destroy_write_struct(&png_ptr, &info_ptr);
cf:
    fclose(fp);
    return errno;
}

extern image_type_t *init(void)
{
    image_type_t *png = malloc(sizeof (image_type_t));
    png->type = strdup("PNG");
    png->is_type = is_png;
    png->read = read_png;
    png->write = write_png;
    return png;
}
