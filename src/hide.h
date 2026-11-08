/*
 * hide ~ A tool for hiding data inside images
 * Copyright © 2014-2026, albinoloverats ~ Software Development
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

#include "cli.h"

#ifndef EFTYPE
	#define EFTYPE 79 /*!< Unsupported file/image type */
#endif

#define HIDE_VERSION "N/A"
#define PROJECT_URL "https://albinoloverats.net/projects/hide"

#define DIR_LIBRARY "/usr/lib/"
#define DIR_LOCAL "./"


static inline uint64_t hide_capacity(uint64_t width, uint64_t height)
{
	if (height && width > UINT64_MAX / height)
		return 0;
	uint64_t pixels = width * height;
	return pixels > sizeof (uint64_t) ? pixels - sizeof (uint64_t) : 0;
}

typedef struct _image_info_s
{
	char *file;
	int (*read)(struct _image_info_s *, void (*progress_update)(uint64_t, uint64_t));
	int (*write)(struct _image_info_s, void (*progress_update)(uint64_t, uint64_t));
	uint64_t (*info)(struct _image_info_s *);
	void (*free)(struct _image_info_s);
	uint64_t height;
	uint64_t width;
	uint16_t bpp;
	uint8_t **buffer;
	void *extra;
}
image_info_s;

typedef enum
{
	UNKNOWN,
	HIDE,
	FIND
}
hide_action_e;

typedef struct
{
	char *file;
	uint64_t size;
	hide_action_e action;
	bool fill;
}
data_info_s;

typedef struct
{
	char *type;
	bool (*is_type)(char *);
	int (*read)(image_info_s *, void (*progress_update)(uint64_t, uint64_t));
	int (*write)(image_info_s, void (*progress_update)(uint64_t, uint64_t));
	uint64_t (*info)(image_info_s *);
	void (*free)(image_info_s);
}
image_type_s;

typedef struct
{
	char *image_in;
	char *data_file;
	char *image_out;
}
hide_files_s;

typedef struct
{
	cli_s *ui;
	hide_files_s files;
	hide_action_e action;
	bool fill;
	int result;
}
process_options_s;


extern void *process(void *options);

extern void *find_supported_formats(char *path, image_info_s *image_info);

#endif
