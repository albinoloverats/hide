#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

#ifndef _WIN32
    #include <netinet/in.h>
#endif

#include "common/common.h"

#include "main.h"

#define CAPACITY (image_info.width * image_info.height - sizeof( uint64_t ))

static int process_file(data_info_t data_info, image_info_t image_info)
{
    errno = EXIT_SUCCESS;

    int64_t f = 0;
    uint8_t *map = NULL;

    if ((f = open(data_info.file, data_info.hide ? O_RDONLY : (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR)) < 0)
        return errno;
    if (data_info.hide && (map = mmap(NULL, ntohll(data_info.size), PROT_READ, MAP_SHARED, f, 0)) == MAP_FAILED)
        return errno;

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
                            goto done;
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

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4)
    {
        fprintf(stderr, "Usage: %s <source image> <data to hide> <output image>\n", argv[0]);
        fprintf(stderr, "       %s <image> <recovered data>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *image_in = argv[1];
    char *file = argv[2];
    char *image_out = argv[3];

    image_info_t image_info = { image_in, NULL, NULL, 0, 0, 0, NULL, NULL };
    data_info_t data_info = { file, 0, false };

    if (is_png(image_in))
    {
        image_info.read = read_file_png;
        image_info.write = write_file_png;
    }
    else if (is_tiff(image_in))
    {
        image_info.read = read_file_tiff;
        image_info.write = write_file_tiff;
    }
    else
    {
        fprintf(stderr, "Unsupported image format; please use either PNG or TIFF\n");
        return EFTYPE;
    }

    /*
     * read the source image
     */
    if (image_info.read(&image_info))
    {
        perror("Failed to read source image");
        return errno;
    }

    if (argc == 4)
    {
        if (!will_fit(&data_info, image_info))
        {
            fprintf(stderr, "Too much data to hide; find a larger image\nAvailable capacity: %" PRIu64 " bytes\n", CAPACITY);
            return errno;
        }
        /*
         * overlay the data on the image
         */
        if (process_file(data_info, image_info))
        {
            perror("Failed during data processing");
            return errno;
        }
        /*
         * write the image with the hidden data
         */
        image_info.file = image_out;
        if (image_info.write(image_info))
        {
            perror("Failed to write output image");
            return errno;
        }
    }
    else
    {
        /*
         * extract the hidden data
         */
        if (process_file(data_info, image_info))
        {
            perror("Failed during data processing");
            return errno;
        }
    }

    fprintf(stderr, "Done.\n");

    return EXIT_SUCCESS;
}
