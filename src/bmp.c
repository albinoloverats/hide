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
#include <netinet/in.h>

#include "hide.h"

#define BI_RGB 0

static const uint8_t HEADER[] = { 'B', 'M' };

typedef struct
{
	uint32_t size;
	uint8_t *data;
}
bmp_extra_t;

/*
 * all BMP values are little endian; convert everything to network-order
 * and then into the correct order for this machine
 */

#if defined __FreeBSD__
	#define from_little_endian_16(s) ntohs(__bswap16(s))
	#define from_little_endian_32(i) ntohl(__bswap32(i))
#else
	#define from_little_endian_16(s) ntohs(__bswap_16(s))
	#define from_little_endian_32(i) ntohl(__bswap_32(i))
#endif

static bool is_bmp(char *file_name)
{
	FILE *bmp = fopen(file_name, "rb");
	if (!bmp)
		return false;
	uint8_t header[2];
	fread(header, sizeof header, 1, bmp);
	fclose(bmp);
	return !memcmp(HEADER, header, sizeof header);
}

static int read_bmp(image_info_t *image_info, void (*progress_update)(uint64_t, uint64_t))
{
	errno = EXIT_SUCCESS;

	FILE *bmp = fopen(image_info->file, "rb");
	if (!bmp)
		return errno;

	fseek(bmp, 0x12, SEEK_SET);
	uint32_t dword;
	fread(&dword, sizeof dword, 1, bmp);
	image_info->width = from_little_endian_32(dword);
	fread(&dword, sizeof dword, 1, bmp);
	image_info->height = from_little_endian_32(dword);

	fseek(bmp, 0x1C, SEEK_SET);
	uint16_t word;
	fread(&word, sizeof word, 1, bmp);
	switch (from_little_endian_16(word))
	{
		case 32:
			image_info->bpp = 4;
			break;
		case 24:
			image_info->bpp = 3;
			break;
		default:
			errno = ENOTSUP;
			goto done;
	}
	fread(&dword, sizeof dword, 1, bmp);
	if (dword != BI_RGB)
	{
		errno = ENOTSUP;
		goto done;
	}

	bmp_extra_t *extra = malloc(sizeof (bmp_extra_t));
	fseek(bmp, 0x0A, SEEK_SET);
	fread(&extra->size, sizeof extra->size, 1, bmp);
	extra->size = from_little_endian_32(extra->size);
	extra->data = malloc(extra->size);
	fseek(bmp, 0x00, SEEK_SET);
	fread(extra->data, extra->size, 1, bmp);
	image_info->extra = extra;

	uint32_t padding = image_info->width % 4;
	if (padding == 4)
		padding = 0;

	image_info->buffer = malloc(sizeof (uint8_t *) * image_info->height);
	fseek(bmp, extra->size, SEEK_SET);
	for (uint64_t y = 0; y < image_info->height; y++)
	{
		image_info->buffer[y] = malloc(image_info->width * image_info->bpp);
		fread(image_info->buffer[y], image_info->width, image_info->bpp, bmp);
		uint32_t ignored = 0x00;
		if (padding)
			fread(&ignored, padding, 1, bmp);
		if (progress_update)
			progress_update(y, image_info->height);
	}

done:
	fclose(bmp);

	return errno;
}

static int write_bmp(image_info_t image_info, void (*progress_update)(uint64_t, uint64_t))
{
	errno = EXIT_SUCCESS;

	FILE *bmp = fopen(image_info.file, "wb");
	if (!bmp)
		return errno;

	bmp_extra_t *extra = image_info.extra;
	fwrite(extra->data, extra->size, 1, bmp);

	uint32_t padding = image_info.width % 4;
	if (padding == 4)
		padding = 0;

	fseek(bmp, extra->size, SEEK_SET);
	for (uint64_t y = 0; y < image_info.height; y++)
	{
		fwrite(image_info.buffer[y], image_info.width, image_info.bpp, bmp);
		free(image_info.buffer[y]);
		uint32_t ignored = 0;
		if (padding)
			fwrite(&ignored, padding, 1, bmp);
		if (progress_update)
			progress_update(y, image_info.height);
	}
	free(image_info.buffer);

	free(extra->data);
	free(extra);

	fclose(bmp);

	return errno;
}

static uint64_t info_bmp(image_info_t *image_info)
{
	read_bmp(image_info, NULL);
	return HIDE_CAPACITY;
}

static void free_bmp(image_info_t image_info)
{
	for (uint64_t y = 0; y < image_info.height; y++)
		free(image_info.buffer[y]);
	free(image_info.buffer);
	bmp_extra_t *extra = image_info.extra;
	free(extra->data);
	free(extra);
}

extern image_type_t *init(void)
{
	static image_type_t bmp;
	bmp.type = "BMP";
	bmp.is_type = is_bmp;
	bmp.read = read_bmp;
	bmp.write = write_bmp;
	bmp.info = info_bmp;
	bmp.free = free_bmp;
	return &bmp;
}
