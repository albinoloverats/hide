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

#include "common/common.h"

#include "hide.h"
#include "jpeg.h"

static const char jpeg_header[] = { 0xFF, 0xD8, 0xFF };

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

	jpeg_message_t msg = { 0x00, NULL };
	jpeg_image_t *image = calloc(sizeof (jpeg_image_t *), 1);
	jpeg_load_e action = *(bool *)image_info->extra ? JPEG_LOAD_READ : JPEG_LOAD_FIND;

	jpeg_decode_data(fp, &msg, image, action);
	if (action == JPEG_LOAD_FIND)
	{
		msg.size = htonll(msg.size);
		memcpy(msg.data, &msg.size, sizeof msg.size);
		msg.size = ntohll(msg.size) + sizeof msg.size;
	}
	else
		msg.size -= sizeof msg.size; /* available capacity */

	image_info->bpp = 3;
	image_info->width = msg.size;
	image_info->height = 1;
	image_info->buffer = malloc(sizeof (uint8_t *) * image_info->height);
	image_info->buffer[0] = malloc(image_info->width * image_info->bpp);
	/* (if necessary) copy message.data into image_info->buffer */
	if (msg.data)
		for (uint64_t x = 0, i = 0; i < msg.size; x += image_info->bpp, i++)
		{
			image_info->buffer[0][x + 0] = (msg.data[i] & 0xE0) >> 5;
			image_info->buffer[0][x + 1] = (msg.data[i] & 0x18) >> 3;
			image_info->buffer[0][x + 2] = (msg.data[i] & 0x07);
			if (progress_update)
				progress_update(x, image_info->width);
		}
	else if (progress_update)
		progress_update(image_info->width, image_info->width);

	/* store the image data where we can get it back later */
	image_info->extra = image;

	fclose(fp);
	return errno;
}

static int write_jpeg(image_info_t image_info, void (*progress_update)(uint64_t, uint64_t))
{
	errno = EXIT_SUCCESS;

	FILE *fp = fopen(image_info.file, "wb");
	if (!fp)
		return errno;

	jpeg_message_t msg = { image_info.width, NULL };
	msg.data = malloc(msg.size); // total capacity

	/* retrieve the image data */
	jpeg_image_t *image = image_info.extra;

	/* copy message from image_info.buffer to message.data */
	for (uint64_t x = 0, i = 0; x < image_info.width * image_info.bpp; x += image_info.bpp, i++)
	{
		msg.data[i]  = (image_info.buffer[0][x + 0] & 0x07) << 5;
		msg.data[i] |= (image_info.buffer[0][x + 1] & 0x03) << 3;
		msg.data[i] |= (image_info.buffer[0][x + 2] & 0x07);
		if (progress_update)
			progress_update(x, image_info.width);
	}
	/* get actual message length from data */
	memcpy(&msg.size, msg.data, sizeof msg.size);
	msg.size = ntohll(msg.size);

	/* write the message to the image */
	jpeg_encode_data(fp, &msg, image);

	fclose(fp);
	return errno;
}

static uint64_t info_jpeg(image_info_t *image_info)
{
	bool t = true;
	image_info->extra = &t;
	read_jpeg(image_info, NULL);
	return CAPACITY;
}

extern image_type_t *init(void)
{
	image_type_t *jpeg = malloc(sizeof (image_type_t));
	jpeg->type = strdup("JPEG");
	jpeg->is_type = is_jpeg;
	jpeg->read = read_jpeg;
	jpeg->write = write_jpeg;
	jpeg->info =info_jpeg;
	return jpeg;
}
