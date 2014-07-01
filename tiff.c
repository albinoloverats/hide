#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h>

#include <tiffio.h>

#include "main.h"

void read_file_tiff(char *file_name, image_info_t *image_info)
{
    TIFF *tif = TIFFOpen(file_name, "r");

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &image_info->pixel_width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &image_info->pixel_height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &image_info->bytes_per_pixel);

    image_info->buffer = (uint8_t **)malloc(sizeof( uint8_t * ) * image_info->pixel_height);
    for (int y = 0; y < image_info->pixel_height; y++)
    {
        image_info->buffer[y] = malloc(image_info->pixel_width * image_info->bytes_per_pixel);
        TIFFReadScanline(tif, image_info->buffer[y], y, 0);
    }

    TIFFClose(tif);

    return;
}

void write_file_tiff(char *file_name, image_info_t image_info)
{
    TIFF *tif = TIFFOpen(file_name, "w");

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, image_info.pixel_width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, image_info.pixel_height);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, image_info.bytes_per_pixel);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZMA);

    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, image_info.pixel_width * image_info.bytes_per_pixel));

    for (int y = 0; y < image_info.pixel_height; y++)
    {
        TIFFWriteScanline(tif, image_info.buffer[y], y, 0);
        free(image_info.buffer[y]);
    }
    free(image_info.buffer);

    TIFFClose(tif);

    return;
}
