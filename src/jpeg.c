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
#include <setjmp.h>

#include <jpeglib.h>

#include "hide.h"

typedef struct
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
}
jpg_err_mgr;

static const char jpeg_header[] = { 0xFF, 0xD8, 0xFF };

static void jpeg_error_exit(j_common_ptr ptr)
{
	jpg_err_mgr *myerr = (jpg_err_mgr *)ptr->err;
	//(*ptr->err->output_message)(ptr);
	longjmp(myerr->setjmp_buffer, 1);
}

static bool is_jpeg(char *file_name)
{
	FILE *fp = fopen(file_name, "rb");
	if (!fp)
		return false;

	uint8_t header[3];
	fread(header, 1, sizeof header, fp);
	fclose(fp);

	return !memcmp(header, jpeg_header, sizeof header);
}

static int read_jpeg(image_info_t *image_info, void (*progress_update)(uint64_t, uint64_t))
{
	errno = EXIT_SUCCESS;

	FILE *fp = fopen(image_info->file, "rb");
	if (!fp)
		return errno;

	jpg_err_mgr err;
	struct jpeg_decompress_struct jpeg_ptr;
	/* Step 1: allocate and initialize JPEG decompression object */

	jpeg_ptr.err = jpeg_std_error(&err.pub);
	err.pub.error_exit = jpeg_error_exit;
	/* Establish the setjmp return context for jpeg_error_exit to use. */
	if (setjmp(err.setjmp_buffer))
		goto cleanup;

	jpeg_create_decompress(&jpeg_ptr);

	/* Step 2: specify data source (eg, a file) */
	jpeg_stdio_src(&jpeg_ptr, fp);

	/* Step 3: read file parameters with jpeg_read_header() */
	jpeg_read_header(&jpeg_ptr, true);

	/* Step 4: set parameters for decompression */
	/* Step 5: Start decompressor */
	jpeg_start_decompress(&jpeg_ptr);

	image_info->width = jpeg_ptr.output_width;
	image_info->height = jpeg_ptr.output_height;
	image_info->bpp = jpeg_ptr.output_components;

	image_info->buffer = malloc(sizeof (uint8_t *) * image_info->height);
	for (uint64_t y = 0; y < image_info->height; y++)
	{
		image_info->buffer[y] = malloc(image_info->width * image_info->bpp);
		if (progress_update)
			progress_update(y, image_info->height);

		jpeg_read_scanlines(&jpeg_ptr, &image_info->buffer[y], 1);
	}

	/* Step 7: Finish decompression */
	jpeg_finish_decompress(&jpeg_ptr);

cleanup:
	/* Step 8: Release JPEG decompression object */
	jpeg_destroy_decompress(&jpeg_ptr);
	fclose(fp);
	return errno;
}

static int write_jpeg(image_info_t image_info, void (*progress_update)(uint64_t, uint64_t))
{
	errno = EXIT_SUCCESS;

	/* create file */
	FILE *fp = fopen(image_info.file, "wb");
	if (!fp)
		return errno;

	/* Step 1: allocate and initialize JPEG compression object */
	struct jpeg_compress_struct jpeg_ptr;
	struct jpeg_error_mgr err;
	jpeg_ptr.err = jpeg_std_error(&err);
	jpeg_create_compress(&jpeg_ptr);

	/* Step 2: specify data destination (eg, a file) */
	jpeg_stdio_dest(&jpeg_ptr, fp);

	/* Step 3: set parameters for compression */
	jpeg_ptr.image_width = image_info.width;
	jpeg_ptr.image_height = image_info.height;
	jpeg_ptr.input_components = image_info.bpp;
	jpeg_ptr.in_color_space = JCS_RGB;

	jpeg_set_defaults(&jpeg_ptr);
	jpeg_set_quality(&jpeg_ptr, 100, true);

	/* Step 4: Start compressor */
	jpeg_start_compress(&jpeg_ptr, true);

	/* Step 5: while (scan lines remain to be written) */
	for (uint64_t y = 0; y < image_info.height; y++)
	{
		jpeg_write_scanlines(&jpeg_ptr, &image_info.buffer[y], 1);

		free(image_info.buffer[y]);
		if (progress_update)
			progress_update(y, image_info.height);
	}
	free(image_info.buffer);

	/* Step 6: Finish compression */
	jpeg_finish_compress(&jpeg_ptr);
	/* Step 7: release JPEG compression object */
	jpeg_destroy_compress(&jpeg_ptr);
	/* And we're done! */
	fclose(fp);
	return errno;
}

extern image_type_t *init(void)
{
	image_type_t *jpeg = malloc(sizeof (image_type_t));
	jpeg->type = strdup("JPEG");
	jpeg->is_type = is_jpeg;
	jpeg->read = read_jpeg;
	jpeg->write = write_jpeg;
	return jpeg;
}
