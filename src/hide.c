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

#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>

#include <sys/mman.h>
#include <sys/stat.h>

/* submodule includes */

#include "common.h"
#include "error.h"
#include "cli.h"

/* project includes */

#include "hide.h"


static int process_file(data_info_s data_info, image_info_s image_info, void (*progress_update)(uint64_t, uint64_t))
{
	errno = EXIT_SUCCESS;

	int f = -1;
	uint8_t *map = NULL;
	uint64_t size = ntohll(data_info.size);

	if ((f = open(data_info.file, data_info.action == HIDE ? O_RDONLY : (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR)) < 0)
		return errno;

	uint64_t capacity = hide_capacity(image_info.width, image_info.height);

	if (data_info.action == HIDE)
	{
		struct stat s;
		if (fstat(f, &s) < 0 || !S_ISREG(s.st_mode))
			goto done;
		size = (uint64_t)s.st_size;
		data_info.size = htonll(size);
		if (size > SIZE_MAX)
		{
			errno = EFBIG;
			goto done;
		}
		if (size && (map = mmap(NULL, (size_t)size, PROT_READ, MAP_SHARED, f, 0)) == MAP_FAILED)
		{
			map = NULL;
			goto done;
		}
	}
	else if (data_info.fill)
	{
		size = capacity;
		if (size > SIZE_MAX || (off_t)size < 0 || (uint64_t)(off_t)size != size)
		{
			errno = EFBIG;
			goto done;
		}
		if (ftruncate(f, (off_t)size) < 0)
			goto done;
		if (size && (map = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, f, 0)) == MAP_FAILED)
		{
			map = NULL;
			goto done;
		}
	}

	uint8_t *z = (uint8_t *)&data_info.size;
	for (uint64_t i = 0, y = 0; y < image_info.height; y++)
	{
		uint8_t *row = image_info.buffer[y];
		for (uint64_t x = 0; x < image_info.width; x++)
		{
			uint8_t *ptr = &(row[x * image_info.bpp]);

			if (data_info.action == HIDE)
			{
				unsigned char c;
				if (y == 0 && x < sizeof data_info.size && !data_info.fill)
					c = z[x];
				else if (i < size)
					c = map[i];
				else /* TODO use something more secure */
					c = (uint8_t)lrand48();
				ptr[0] = (ptr[0] & 0xF8) | ((c & 0xE0) >> 5); // r 3 lsb
				ptr[1] = (ptr[1] & 0xFC) | ((c & 0x18) >> 3); // g 2 lsb
				ptr[2] = (ptr[2] & 0xF8) |  (c & 0x07);       // b 3 lsb
			}
			else
			{
				unsigned char c = (ptr[0] & 0x07) << 5;
				c |= (ptr[1] & 0x03) << 3;
				c |= (ptr[2] & 0x07);
				if (y == 0 && x < sizeof data_info.size && !data_info.fill)
				{
					z[x] = c;
					if (x == sizeof data_info.size - 1)
					{
						size = ntohll(data_info.size);
						if (size > capacity || size > SIZE_MAX ||
						    (off_t)size < 0 || (uint64_t)(off_t)size != size)
						{
							errno = EFBIG;
							goto done;
						}
						if (ftruncate(f, (off_t)size) < 0)
							goto done;
						if (!size)
							goto done;
						if ((map = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, f, 0)) == MAP_FAILED)
						{
							map = NULL;
							goto done;
						}
					}
				}
				else
				{
					if (i >= size || !map)
					{
						errno = EIO;
						goto done;
					}
					map[i] = c;
				}
			}
			if (y > 0 || x >= sizeof data_info.size)
			{
				i++;
				if (progress_update)
					progress_update(i, ntohll(data_info.size));
			}
			if (map && i >= size && !data_info.fill)
				goto done;
		}
	}

done:
	if (map)
		munmap(map, (size_t)size);
	if (f >= 0)
		close(f);
	return errno;
}

static bool will_fit(data_info_s *data_info, image_info_s image_info, uint64_t *capacity)
{
	/*
	 * figure out how much data we can hide
	 */
	struct stat s;
	if (stat(data_info->file, &s) < 0)
		return false;
	if (!S_ISREG(s.st_mode) || s.st_size < 0)
	{
		errno = EINVAL;
		return false;
	}
	*capacity = hide_capacity(image_info.width, image_info.height);
	if ((uint64_t)s.st_size > *capacity)
	{
		errno = ENOSPC;
		return false;
	}
	data_info->size = htonll(s.st_size);
	return true;
}

static int selector(const struct dirent *d)
{
	size_t length = strlen(d->d_name);
	return length > 8 && !strncmp("hide-", d->d_name, 5) &&
	       !strcmp(d->d_name + length - 3, ".so");
}

extern void *find_supported_formats(char *path, image_info_s *image_info)
{
	void *so = NULL;
	struct dirent **eps = NULL;
	int n = scandir(path, &eps, selector, NULL);
	char buffer[80] = "Supported image formats: ";
	if (n < 0 && strcmp(path, DIR_LOCAL))
		return find_supported_formats(DIR_LOCAL, image_info);
	if (n == 0)
	{
		if (strcmp(path, DIR_LOCAL))
			return find_supported_formats(DIR_LOCAL, image_info);
		cli_eprintf("Could not find any hide image libraries!\n");
		return NULL;
	}
	if (n < 0)
		return NULL;
	for (int i = 0; i < n; ++i)
	{
		char *l = NULL;
		if (strcmp(path, DIR_LOCAL))
			l = eps[i]->d_name;
		else
			asprintf(&l, "%s%s", path, eps[i]->d_name);
		so = dlopen(l, RTLD_LAZY);
		if (!strcmp(path, DIR_LOCAL))
			free(l);
		if (so == NULL)
		{
			cli_eprintf("%s\n", dlerror());
			continue;
		}
		dlerror();
		image_type_s *(*init)(void);
		if (!(init = dlsym(so, "init")))
		{
			cli_eprintf("%s\n", dlerror());
			dlclose(so);
			continue;
		}
		image_type_s *format = init();
		if (!format || !format->type || !format->is_type || !format->read ||
		    !format->write || !format->info || !format->free)
		{
			dlclose(so);
			so = NULL;
			continue;
		}
		if (!image_info)
		{
			strcat(buffer, format->type);
			strcat(buffer, " ");
			if (strlen(buffer) > 72)
			{
				cli_eprintf("%s\n", buffer);
				memset(buffer, 0x00, sizeof buffer);
			}
		}
		else if (format->is_type(image_info->file))
		{
			image_info->read = format->read;
			image_info->write = format->write;
			image_info->info = format->info;
			image_info->free = format->free;
			break;
		}
		dlclose(so);
		so = NULL;
	}
	if (!image_info && strlen(buffer))
		cli_eprintf("%s\n", buffer);

	for (int i = 0; i < n; ++i)
		free(eps[i]);
	free(eps);

	return so;
}

static cli_s *ui = NULL;

#ifndef __DEBUG__
static void progress_current_update(uint64_t i, uint64_t j)
{
	if (i < j && i > 0)
	{
		ui->current->offset = i;
		ui->current->size = j;
	}
	return;
}
#else
	#define progress_current_update NULL
#endif

extern void *process(void *args)
{
	process_options_s *options = args;
	ui = options->ui;
	options->result = EXIT_FAILURE;
	hide_files_s files = options->files;
	image_info_s image_info = { files.image_in, NULL, NULL, NULL, NULL, 0, 0, 0, NULL, NULL };
	data_info_s data_info = { files.data_file, 0, options->action, options->fill };

	errno = 0;
	void *so = find_supported_formats(DIR_LIBRARY, &image_info);
	if (!so)
	{
		options->result = errno ? errno : EFTYPE;
		if (ui)
			*ui->status = CLI_DONE;
		return NULL;
	}

	if (!(image_info.read && image_info.write))
	{
		cli_eprintf("Unsupported image format\n");
		find_supported_formats(DIR_LIBRARY, NULL);
		errno = EFTYPE;
		goto done;
	}

	/*
	 * current hack for JPEG images: use ->extra to indicate whether
	 * hiding or finding
	 */
	image_info.extra = &data_info;

	if (ui)
	{
		*ui->status = CLI_RUN;
		ui->total->offset = 0;
		ui->total->size = files.image_out ? 3 : 2;
	}

	/*
	 * read the source image
	 */
	if (image_info.read(&image_info, progress_current_update))
		die("Failed to read source image");

	if (files.image_out)
	{
		uint64_t capacity = 0;
		if (!will_fit(&data_info, image_info, &capacity))
			die("Too much data to hide; find a larger image\nAvailable capacity: %" PRIu64 " bytes\n", capacity);
		/*
		 * overlay the data on the image
		 */
		if (ui)
			ui->total->offset++;
		if (process_file(data_info, image_info, progress_current_update))
			die("Failed during data processing");
		/*
		 * write the image with the hidden data
		 */
		if (ui)
			ui->total->offset++;
		image_info.file = files.image_out;
		if (image_info.write(image_info, progress_current_update))
			die("Failed to write output image");
	}
	else
	{
		/*
		 * extract the hidden data
		 */
		if (ui)
			ui->total->offset++;
		if (process_file(data_info, image_info, progress_current_update))
			die("Failed during data processing");
		image_info.free(image_info);
	}

	if (ui)
		ui->total->offset = ui->total->size;
	errno = EXIT_SUCCESS;
done:
	options->result = errno;
	dlclose(so);
	if (ui)
		*ui->status = CLI_DONE;
	return NULL;
}
