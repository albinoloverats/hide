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
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <locale.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include "common/common.h"
#include "common/error.h"
#include "common/cli.h"

#include "hide.h"

#ifdef BUILD_GUI
	#include "gui-gtk.h"
#endif

#define DIR_LIBRARY "/usr/lib/"
#define DIR_LOCAL "./"

#undef HIDE_CAPACITY /* here image_info isn't a pointer but a local variable */
#define HIDE_CAPACITY (image_info.width * image_info.height - sizeof (uint64_t))

#ifndef __DEBUG__
static cli_t ui;
#endif

static int process_file(data_info_t data_info, image_info_t image_info, void (*progress_update)(uint64_t, uint64_t))
{
	errno = EXIT_SUCCESS;

	int64_t f = 0;
	uint8_t *map = NULL;

	if ((f = open(data_info.file, data_info.hide ? O_RDONLY : (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR)) < 0)
		die("Could not open %s", data_info.file);
	if (data_info.hide && (map = mmap(NULL, ntohll(data_info.size), PROT_READ, MAP_SHARED, f, 0)) == MAP_FAILED)
		die("Could not map file %s into memory", data_info.file);

	uint8_t *z = (uint8_t *)&data_info.size;
	for (uint64_t i = 0, y = 0; y < image_info.height; y++)
	{
		uint8_t *row = image_info.buffer[y];
		for (uint64_t x = 0; x < image_info.width; x++)
		{
			uint8_t *ptr = &(row[x * image_info.bpp]);

			if (data_info.hide)
			{
				unsigned char c;
				if (y == 0 && x < sizeof data_info.size && !data_info.fill)
					c = z[x];
				else if (i < ntohll(data_info.size))
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
						ftruncate(f, ntohll(data_info.size));
						if ((map = mmap(NULL, ntohll(data_info.size), PROT_READ | PROT_WRITE, MAP_SHARED, f, 0)) == MAP_FAILED)
							die("Could not map file %s into memory", data_info.file);
					}
				}
				else if (data_info.fill && map == NULL)
				{
					data_info.size = htonll(image_info.height * image_info.width * image_info.bpp);
					ftruncate(f, ntohll(data_info.size));
					if ((map = mmap(NULL, ntohll(data_info.size), PROT_READ | PROT_WRITE, MAP_SHARED, f, 0)) == MAP_FAILED)
						die("Could not map file %s into memory", data_info.file);
					map[i] = c;
					errno = EXIT_SUCCESS;
				}
				else
					map[i] = c;
			}
			if (y > 0 || x >= sizeof data_info.size)
			{
				i++;
				if (progress_update)
					progress_update(i, ntohll(data_info.size));
			}
			if (map && i >= ntohll(data_info.size) && !data_info.fill)
				goto done;
		}
	}

done:
	munmap(map, ntohll(data_info.size));
	close(f);
	return errno;
}

static bool will_fit(data_info_t *data_info, image_info_t image_info)
{
	/*
	 * figure out how much data we can hide
	 */
	struct stat s;
	stat(data_info->file, &s);
	if ((uint64_t)s.st_size > HIDE_CAPACITY)
	{
		errno = ENOSPC;
		return false;
	}
	data_info->size = htonll(s.st_size);
	data_info->hide = true;
	return true;
}

static int selector(const struct dirent *d)
{
	return !strncmp("hide-", d->d_name, 5);
}

static void *find_supported_formats(char *path, image_info_t *image_info)
{
	void *so = NULL;
	struct dirent **eps;
	int n = scandir(path, &eps, selector, NULL);
	char buffer[80] = "Supported image formats: ";
	if (n == 0)
	{
		if (strcmp(path, DIR_LOCAL))
			return find_supported_formats(DIR_LOCAL, image_info);
		fprintf(stderr, "Could not find any hide image libraries!\n");
		return NULL;
	}
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
			fprintf(stderr, "%s\n", dlerror());
			continue;
		}
		image_type_t *(*init)();
		if (!(init = dlsym(so, "init")))
		{
			fprintf(stderr, "%s\n", dlerror());
			dlclose(so);
			continue;
		}
		image_type_t *format = init();
		if (!image_info)
		{
			strcat(buffer, format->type);
			strcat(buffer, " ");
			if (strlen(buffer) > 72)
			{
				fprintf(stderr, "%s\n", buffer);
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
		fprintf(stderr, "%s\n", buffer);

	for (int i = 0; i < n; ++i)
		free(eps[i]);
	free(eps);

	return so;
}

#ifndef __DEBUG__
static void progress_current_update(uint64_t i, uint64_t j)
{
	if (i < j && i > 0)
	{
		ui.current->offset = i;
		ui.current->size = j;
	}
	return;
}
#else
	#define progress_current_update NULL
#endif

extern void *process(void *args)
{
	process_options_t *options = args;
	hide_files_t files = options->files;
	image_info_t image_info = { files.image_in, NULL, NULL, NULL, NULL, 0, 0, 0, NULL, NULL };
	data_info_t data_info = { files.data_file, 0, false, options->fill };

	void *so = find_supported_formats(DIR_LIBRARY, &image_info);
	if (!so)
		pthread_exit(&errno);

	if (!(image_info.read && image_info.write))
	{
		fprintf(stderr, "Unsupported image format\n");
		find_supported_formats(DIR_LIBRARY, NULL);
		errno = EFTYPE;
		goto done;
	}

	/*
	 * current hack for JPEG images: use ->extra to indicate whether
	 * hiding or finding
	 */
	data_info.hide = (bool)files.image_out;
	image_info.extra = &data_info;

#ifndef __DEBUG__
	*ui.status = CLI_RUN;
	ui.total->offset = 0;
	ui.total->size = files.image_out ? 3 : 2;
#endif

	/*
	 * read the source image
	 */
	if (image_info.read(&image_info, progress_current_update))
		die("Failed to read source image");

	if (files.image_out)
	{
		if (!will_fit(&data_info, image_info))
			die("Too much data to hide; find a larger image\nAvailable capacity: %" PRIu64 " bytes\n", HIDE_CAPACITY);
		/*
		 * overlay the data on the image
		 */
#ifndef __DEBUG__
		ui.total->offset++;
#endif
		if (process_file(data_info, image_info, progress_current_update))
			die("Failed during data processing");
		/*
		 * write the image with the hidden data
		 */
#ifndef __DEBUG__
		ui.total->offset++;
#endif
		image_info.file = files.image_out;
		if (image_info.write(image_info, progress_current_update))
			die("Failed to write output image");
	}
	else
	{
		/*
		 * extract the hidden data
		 */
#ifndef __DEBUG__
		ui.total->offset++;
#endif
		if (process_file(data_info, image_info, progress_current_update))
			die("Failed during data processing");
		image_info.free(image_info);
	}

#ifndef __DEBUG__
	ui.total->offset = ui.total->size;
#endif
	errno = EXIT_SUCCESS;
done:
	dlclose(so);
#ifndef __DEBUG__
	*ui.status = CLI_DONE;
	pthread_exit(&errno);
#else
	return &errno;
#endif
}

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 5)
	{
		fprintf(stderr, "Usage: %s [-f] <source image> <file to hide> <output image>\n", argv[0]);
		fprintf(stderr, "       %s [-f] <image> <recovered file>\n", argv[0]);
		fprintf(stderr, "       %s <image>\n", argv[0]);
		find_supported_formats(DIR_LIBRARY, NULL);
		return EXIT_FAILURE;
	}
	else if (argc == 2)
	{
		struct stat s;
		if (stat(argv[1], &s) < 0 && errno == ENOENT)
		{
			fprintf(stderr, "Could not read file %s\n", argv[1]);
			return errno;
		}
		image_info_t image_info = { argv[1], NULL, NULL, NULL, NULL, 0, 0, 0, NULL, NULL };
#ifndef __DEBUG_JPEG__
		void *so = find_supported_formats(DIR_LIBRARY, &image_info);
		if (!so)
			return errno;
#else
		extern uint64_t info_jpeg(image_info_t *image_info);
		extern void free_jpeg(image_info_t image_info);
		image_info.info = info_jpeg;
		image_info.free = free_jpeg;
#endif
		if (!image_info.info)
		{
			fprintf(stderr, "Unsupported image format\n");
			find_supported_formats(DIR_LIBRARY, NULL);
			errno = EFTYPE;
		}
		else
		{
			setlocale(LC_NUMERIC, "");
			printf("File capacity: %'" PRIu64 " bytes\n", image_info.info(&image_info));
			image_info.free(image_info);
			errno = EXIT_SUCCESS;
		}
#ifndef __DEBUG_JPEG__
		dlclose(so);
#endif
		return errno;
	}

	int o = strcmp(argv[1], "-f") ? 0 : 1;

	hide_files_t files = { argv[o + 1], argv[o + 2], argv[o + 3] };
	process_options_t options = { files, o };

#ifndef __DEBUG__
	{
		cli_status_e ui_status = CLI_INIT;
		cli_progress_t ui_current = { 0, 1, NULL }; /* updated after reading image */
		cli_progress_t ui_total   = { 0, 3, NULL }; /* maximum of 3 steps (read, update, write) */
		ui.status = &ui_status;
		ui.current = &ui_current;
		ui.total = &ui_total;
	}
	/*
	 * TODO start process() in own thread, then call cli_display()
	 *
	 * remember that process() will have to set the status once it's finished
	 */
	pthread_t t;
	pthread_attr_t a;
	pthread_attr_init(&a);
	pthread_attr_setdetachstate(&a, PTHREAD_CREATE_JOINABLE);
	pthread_create(&t, &a, process, &options);
	pthread_attr_destroy(&a);

	cli_display(&ui);

	pthread_join(t, NULL);
#else
	process(&options);
#endif

	return errno;
}
