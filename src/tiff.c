#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>

#include <tiffio.h>

#include "imagine.h"

static bool is_tiff(char *file_name)
{
    TIFFSetErrorHandler(NULL);
    TIFFSetWarningHandler(NULL);

    TIFF *tif = TIFFOpen(file_name, "r");
    if (!tif)
        return false;
    TIFFClose(tif);
    return true;
}

static int read_tiff(image_info_t *image_info)
{
    errno = EXIT_SUCCESS;

    TIFF *tif = TIFFOpen(image_info->file, "r");
    if (!tif)
        return errno;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &image_info->width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &image_info->height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &image_info->bpp);

    image_info->buffer = (uint8_t **)malloc(sizeof (uint8_t *) * image_info->height);
    for (uint64_t y = 0; y < image_info->height; y++)
    {
        image_info->buffer[y] = malloc(image_info->width * image_info->bpp);
        TIFFReadScanline(tif, image_info->buffer[y], y, 0);
    }

    TIFFClose(tif);

    return errno;
}

static int write_tiff(image_info_t image_info)
{
    errno = EXIT_SUCCESS;

    TIFF *tif = TIFFOpen(image_info.file, "w");
    if (!tif)
        return errno;

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, image_info.width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, image_info.height);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, image_info.bpp);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZMA);

    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, image_info.width * image_info.bpp));

    for (uint64_t y = 0; y < image_info.height; y++)
    {
        TIFFWriteScanline(tif, image_info.buffer[y], y, 0);
        free(image_info.buffer[y]);
    }
    free(image_info.buffer);

    TIFFClose(tif);

    return errno;
}

extern image_type_t *init(void)
{
    image_type_t *tiff = malloc(sizeof (image_type_t));
    tiff->type = strdup("TIFF");
    tiff->is_type = is_tiff;
    tiff->read = read_tiff;
    tiff->write = write_tiff;
    return tiff;
}
