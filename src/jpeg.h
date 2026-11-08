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

#ifndef _HIDE_JPEG_H_

#include <inttypes.h>

#define byte_limit(i, j) (i < j ? j : (i > 255 ? 255 : i))

typedef enum
{
	JPEG_LOAD_READ,
	JPEG_LOAD_FIND
}
jpeg_load_e;

typedef struct
{
	uint64_t size;
	uint8_t *data;
}
jpeg_message_s;

typedef struct
{
	uint8_t **rgb;
	uint32_t width;
	uint32_t height;
}
jpeg_image_s;


extern bool jpeg_decode_data(FILE *, jpeg_message_s *, jpeg_image_s *, jpeg_load_e, bool);
extern void jpeg_encode_data(FILE *, jpeg_message_s *, jpeg_image_s *);

#endif /* _HIDE_JPEG_H */
