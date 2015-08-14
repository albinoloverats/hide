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

#ifndef _HIDE_H_
#define _HIDE_H_

#include <stdint.h>
#include <stdbool.h>

#ifndef EFTYPE
	#define EFTYPE 79 /*!< Unsupported file/image type */
#endif

#define HIDE_CAPACITY (image_info->width * image_info->height - sizeof (uint64_t))

typedef struct _image_info_t
{
	char *file;
	int (*read)(struct _image_info_t *, void (*progress_update)(uint64_t, uint64_t));
	int (*write)(struct _image_info_t, void (*progress_update)(uint64_t, uint64_t));
	uint64_t (*info)(struct _image_info_t *);
	uint64_t height;
	uint64_t width;
	uint16_t bpp;
	uint8_t **buffer;
	void *extra;
}
image_info_t;

typedef struct
{
	char *file;
	uint64_t size;
	bool hide;
}
data_info_t;

typedef struct
{
	char *type;
	bool (*is_type)(char *);
	int (*read)(image_info_t *, void (*progress_update)(uint64_t, uint64_t));
	int (*write)(image_info_t, void (*progress_update)(uint64_t, uint64_t));
	uint64_t (*info)(image_info_t *);
}
image_type_t;

typedef struct
{
	char *image_in;
	char *file;
	char *image_out;
}
hide_files_t;

extern void *process(void *files);

#endif
