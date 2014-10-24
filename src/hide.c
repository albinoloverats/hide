/*
 * hide ~ A tool for hiding data inside images
 * Copyright © 2009-2014, albinoloverats ~ Software Development
 * email: webmaster@albinoloverats.net
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

#include <sys/mman.h>
#include <sys/stat.h>

#include "common/common.h"
#include "common/error.h"

#include "hide.h"

#define LIB_DIR "/usr/lib/"
#define DBG_DIR "./"

#define CAPACITY (image_info.width * image_info.height - sizeof (uint64_t))

static int process_file(data_info_t data_info, image_info_t image_info)
{
    errno = EXIT_SUCCESS;

    int64_t f = 0;
    uint8_t *map = NULL;

    if ((f = open(data_info.file, data_info.hide ? O_RDONLY : (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR)) < 0)
        die("Could not open %s", data_info.file);
    if (data_info.hide && (map = mmap(NULL, ntohll(data_info.size), PROT_READ, MAP_SHARED, f, 0)) == MAP_FAILED)
        die("Could not map file %s into memory", data_info.file);

    uint8_t *z = (uint8_t *)&data_info.size;
    uint64_t i = 0;
    for (uint64_t y = 0; y < image_info.height; y++)
    {
        uint8_t *row = image_info.buffer[y];
        for (uint64_t x = 0; x < image_info.width; x++)
        {
            uint8_t *ptr = &(row[x * image_info.bpp]);

            if (data_info.hide)
            {
                unsigned char c = y == 0 && x < sizeof data_info.size ? z[x] : map[i];
                ptr[0] = (ptr[0] & 0xF8) | ((c & 0xE0) >> 5);
                ptr[1] = (ptr[1] & 0xFC) | ((c & 0x18) >> 3);
                ptr[2] = (ptr[2] & 0xF8) |  (c & 0x07);
            }
            else
            {
                unsigned char c = (ptr[0] & 0x07) << 5;
                c |= (ptr[1] & 0x03) << 3;
                c |= (ptr[2] & 0x07);

                if (y == 0 && x < sizeof data_info.size)
                {
                    z[x] = c;
                    if (x == sizeof data_info.size - 1)
                    {
                        ftruncate(f, ntohll(data_info.size));
                        if ((map = mmap(NULL, ntohll(data_info.size), PROT_READ | PROT_WRITE, MAP_SHARED, f, 0)) == MAP_FAILED)
                            die("Could not map file %s into memory", data_info.file);
                    }
                }
                else
                    map[i] = c;
            }

            if (y > 0 || x >= sizeof data_info.size)
                i++;
            if (map && i >= ntohll(data_info.size))
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
    if ((uint64_t)s.st_size > CAPACITY)
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
    return !strncmp("hide-", d->d_name, 8);
}

static void *find_supported_formats(image_info_t *image_info)
{
    void *so = NULL;
    struct dirent **eps;
#if !defined(__DEBUG__)
    int n = scandir(LIB_DIR, &eps, selector, NULL);
#else
    int n = scandir(DBG_DIR, &eps, selector, NULL);
#endif
    char buffer[80] = "Supported image formats: ";
    for (int i = 0; i < n; ++i)
    {
#if !defined(__DEBUG__)
        char *l = eps[i]->d_name;
#else
        char *l = NULL;
        asprintf(&l, "%s%s", DBG_DIR, eps[i]->d_name);
#endif
        if ((so = dlopen(l, RTLD_LAZY)) == NULL)
        {
#ifdef __DEBUG__
            free(l);
#endif
            continue;
        }
#ifdef __DEBUG__
        free(l);
#endif
        image_type_t *(*init)();
        if (!(init = dlsym(so, "init")))
        {
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
            break;
        }
        dlclose(so);
    }
    if (!image_info && strlen(buffer))
        fprintf(stderr, "%s\n", buffer);

    for (int i = 0; i < n; ++i)
        free(eps[i]);
    free(eps);

    return so;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4)
    {
        fprintf(stderr, "Usage: %s <source image> <file to hide> <output image>\n", argv[0]);
        fprintf(stderr, "       %s <image> <recovered file>\n", argv[0]);
        find_supported_formats(NULL);
        return EXIT_FAILURE;
    }

    char *image_in = argv[1];
    char *file = argv[2];
    char *image_out = argv[3];

    image_info_t image_info = { image_in, NULL, NULL, 0, 0, 0, NULL, NULL };
    data_info_t data_info = { file, 0, false };

    void *so = find_supported_formats(&image_info);
    if (!so)
        return errno;

    if (!(image_info.read && image_info.write))
    {
        fprintf(stderr, "Unsupported image format\n");
        find_supported_formats(NULL);
        return EFTYPE;
    }

    /*
     * read the source image
     */
    if (image_info.read(&image_info))
        die("Failed to read source image");

    if (argc == 4)
    {
        if (!will_fit(&data_info, image_info))
            die("Too much data to hide; find a larger image\nAvailable capacity: %" PRIu64 " bytes\n", CAPACITY);
        /*
         * overlay the data on the image
         */
        if (process_file(data_info, image_info))
            die("Failed during data processing");
        /*
         * write the image with the hidden data
         */
        image_info.file = image_out;
        if (image_info.write(image_info))
            die("Failed to write output image");
    }
    else
        /*
         * extract the hidden data
         */
        if (process_file(data_info, image_info))
            die("Failed during data processing");

    dlclose(so);

    return EXIT_SUCCESS;
}
