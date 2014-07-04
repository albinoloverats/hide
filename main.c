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

#include "main.h"

int process_file(bool store, char *file_name, uint64_t file_size, image_info_t image_info)
{
    errno = EXIT_SUCCESS;

    int64_t f = 0;
    uint8_t *map = NULL;

    if ((f = open(file_name, store ? O_RDONLY : (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR)) < 0)
        return errno;
    if (store && (map = mmap(NULL, ntohll(file_size), PROT_READ, MAP_SHARED, f, 0)) == MAP_FAILED)
        return errno;

    uint8_t *z = (uint8_t *)&file_size;
    uint64_t i = 0;
    for (uint64_t y = 0; y < image_info.pixel_height; y++)
    {
        uint8_t *row = image_info.buffer[y];
        for (uint64_t x = 0; x < image_info.pixel_width; x++)
        {
            uint8_t *ptr = &(row[x * image_info.bytes_per_pixel]);

            if (store)
            {
                unsigned char c = y == 0 && x < sizeof file_size ? z[x] : map[i];
#if 0
                ptr[0] = (ptr[0] & 0xF0) | ((c & 0xF0) >> 4);
                ptr[1] = (ptr[1] & 0xF0) | ((c & 0x3C) >> 2);
                ptr[2] = (ptr[2] & 0xF0) |  (c & 0x0F);
#else
                ptr[0] = (ptr[0] & 0xF8) | ((c & 0xE0) >> 5);
                ptr[1] = (ptr[1] & 0xFC) | ((c & 0x18) >> 3);
                ptr[2] = (ptr[2] & 0xF8) |  (c & 0x07);
#endif
            }
            else
            {
                unsigned char c = (ptr[0] & 0x07) << 5;
                c |= (ptr[1] & 0x03) << 3;
                c |= (ptr[2] & 0x07);

                if (y == 0 && x < sizeof file_size)
                {
                    z[x] = c;
                    if (x == sizeof file_size - 1)
                    {
                        ftruncate(f, ntohll(file_size));
                        if ((map = mmap(NULL, ntohll(file_size), PROT_READ | PROT_WRITE, MAP_SHARED, f, 0)) == MAP_FAILED)
                            goto done;
                    }
                }
                else
                    map[i] = c;
            }

            if (y > 0 || x >= sizeof file_size)
                i++;
            if (map && i >= ntohll(file_size))
                goto done;
        }
    }

done:
    munmap(map, ntohll(file_size));
    close(f);
    return errno;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4)
    {
        fprintf(stderr, "Usage: %s <source image> <data to hide> <output image>\n", argv[0]);
        fprintf(stderr, "       %s <image> <recovered data>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *image_in= argv[1];
    char *file = argv[2];
    char *image_out = argv[3];

    int (*read_file_func)(char *, image_info_t *);
    int (*write_file_func)(char *, image_info_t);

    if (is_png(image_in))
    {
        read_file_func = read_file_png;
        write_file_func = write_file_png;
    }
    else if (is_tiff(image_in))
    {
        read_file_func = read_file_tiff;
        write_file_func = write_file_tiff;
    }
    else
    {
        fprintf(stderr, "Unsupported image format; please use either PNG or TIFF\n");
        return EFTYPE;
    }

    /*
     * read the source image
     */
    image_info_t image_info = { 0, 0, 0, NULL, NULL };
    if (read_file_func(image_in, &image_info))
    {
        perror("Failed to read source image");
        return errno;
    }

    if (argc == 4)
    {
        /*
         * figure out how much data we can hide
         */
        struct stat s;
        stat(file, &s);
        uint64_t capacity = image_info.pixel_width * image_info.pixel_height - sizeof( uint64_t );
        if ((uint64_t)s.st_size > capacity)
        {
            fprintf(stderr, "Too much data to hide; available capacity: %" PRIu64 " bytes", capacity);
            return ENOSPC;
        }
        /*
         * overlay the data on the image
         */
        if (process_file(true, file, htonll(s.st_size), image_info))
        {
            perror("Failed during data processing");
            return errno;
        }
        /*
         * write the image with the hidden data
         */
        if (write_file_func(image_out, image_info))
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
        if (process_file(false, file, 0, image_info))
        {
            perror("Failed during data processing");
            return errno;
        }
    }

    fprintf(stderr, "Done.\n");

    return EXIT_SUCCESS;
}
