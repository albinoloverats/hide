/*
 * hide ~ A tool for hiding data inside images
 * Copyright © 2014-2015, albinoloverats ~ Software Development
 * email: hide@albinoloverats.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>

#include <tiffio.h>

#include "hide.h"

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

static int read_tiff(image_info_t *image_info, void (*progress_update)(uint64_t, uint64_t))
{
	errno = EXIT_SUCCESS;

	TIFF *tif = TIFFOpen(image_info->file, "r");
	if (!tif)
		return errno;

	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &image_info->width);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &image_info->height);
	TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &image_info->bpp);

	image_info->buffer = malloc(sizeof (uint8_t *) * image_info->height);
	for (uint64_t y = 0; y < image_info->height; y++)
	{
		image_info->buffer[y] = malloc(image_info->width * image_info->bpp);
		TIFFReadScanline(tif, image_info->buffer[y], y, 0);
		if (progress_update)
			progress_update(y, image_info->height);
	}

	TIFFClose(tif);

	return errno;
}

static int write_tiff(image_info_t image_info, void (*progress_update)(uint64_t, uint64_t))
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
		if (progress_update)
			progress_update(y, image_info.height);
	}
	free(image_info.buffer);

	TIFFClose(tif);

	return errno;
}

static uint64_t info_tiff(image_info_t *image_info)
{
	read_tiff(image_info, NULL);
	return HIDE_CAPACITY;
}

static void free_tiff(image_info_t image_info)
{
	for (uint64_t y = 0; y < image_info.height; y++)
		free(image_info.buffer[y]);
	free(image_info.buffer);
}

extern image_type_t *init(void)
{
	static image_type_t tiff;
	tiff.type = "TIFF";
	tiff.is_type = is_tiff;
	tiff.read = read_tiff;
	tiff.write = write_tiff;
	tiff.info = info_tiff;
	tiff.free = free_tiff;
	return &tiff;
}
