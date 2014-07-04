#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h>

#include <png.h>

#include "main.h"

static png_byte bit_depth;

//int number_of_passes;

void read_file_png(char *file_name, image_info_t *image_info)
{
    errno = EXIT_SUCCESS;

    /* 8 is the maximum size that can be checked */
    uint8_t header[8];

    /* open file and test for it being a png */
    FILE *fp = fopen(file_name, "rb");
    if (!fp)
        return;
    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8))
        goto cf;

    /* initialize stuff */
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
        goto cf;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
        goto cleanup;

    if (setjmp(png_jmpbuf(png_ptr)))
        goto cleanup;

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    image_info->pixel_width = png_get_image_width(png_ptr, info_ptr);
    image_info->pixel_height = png_get_image_height(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    switch (png_get_color_type(png_ptr, info_ptr))
    {
        case PNG_COLOR_TYPE_RGB:
            image_info->bytes_per_pixel = 3;
            break;
        case PNG_COLOR_TYPE_RGBA:
            image_info->bytes_per_pixel = 4;
            break;
        default:
            goto cleanup;
    }

    //number_of_passes = png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    /* read file */
    if (setjmp(png_jmpbuf(png_ptr)))
        goto cleanup;

    image_info->buffer = (uint8_t **)malloc(sizeof( uint8_t * ) * image_info->pixel_height);
    for (uint64_t y = 0; y < image_info->pixel_height; y++)
        image_info->buffer[y] = malloc(image_info->pixel_width * image_info->bytes_per_pixel);

    png_read_image(png_ptr, image_info->buffer);

cleanup:
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
cf:
    fclose(fp);

    return;
}

void write_file_png(char *file_name, image_info_t image_info)
{
    errno = EXIT_SUCCESS;

    /* create file */
    FILE *fp = fopen(file_name, "wb");
    if (!fp)
        return;

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
    switch (image_info.bytes_per_pixel)
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


    png_set_IHDR(png_ptr, info_ptr, image_info.pixel_width, image_info.pixel_height, bit_depth,
             color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

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
    for (uint64_t y = 0; y < image_info.pixel_height; y++)
        free(image_info.buffer[y]);
    free(image_info.buffer);

cleanup:
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
cf:
    fclose(fp);
    return;
}
