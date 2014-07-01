#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include <png.h>

#include "main.h"

void process_file(bool store, char *file_name, uint64_t file_size, image_info_t image_info)
{
    int64_t f = 0;
    uint8_t *map = NULL;

    if ((f = open(file_name, store ? O_RDONLY : (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR)) < 0)
        return;
    if (store && (map = mmap(NULL, file_size, PROT_READ, MAP_SHARED, f, 0)) == MAP_FAILED)
        return;

    uint8_t *z = (uint8_t *)&file_size;
    int i = 0;
    for (int y = 0; y < image_info.pixel_height; y++)
    {
        uint8_t *row = image_info.buffer[y];
        for (int x = 0; x < image_info.pixel_width; x++)
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
                        ftruncate(f, file_size);
                        if ((map = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, f, 0)) == MAP_FAILED)
                            goto done;
                    }
                }
                else
                    map[i] = c;
            }

            if (y > 0 || x >= sizeof file_size)
                i++;
            if (map && i >= file_size)
                goto done;
        }
    }

done:
    munmap(map, file_size);
    close(f);
    return;
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4)
        return EXIT_FAILURE;

    char *image_in= argv[1];
    char *file = argv[2];
    char *image_out = argv[3];

    image_info_t image_info = { 0, 0, 0, NULL };

    void (*read_file_func)(char *, image_info_t *);
    void (*write_file_func)(char *, image_info_t);
    if (!strcasecmp(".png", strrchr(image_in, '.')))
    {
        read_file_func = read_file_png;
        write_file_func = write_file_png;
    }
    else if (!strcasecmp(".tiff", strrchr(image_in, '.')))
    {
        read_file_func = read_file_tiff;
        write_file_func = write_file_tiff;
    }
    else
        return EXIT_SUCCESS;

    read_file_func(image_in, &image_info);

    if (argc == 4)
    {
        struct stat s;
        stat(file, &s);
        if (s.st_size > image_info.pixel_width * image_info.pixel_height - sizeof( uint64_t))
            return EXIT_FAILURE;
        process_file(true, file, s.st_size, image_info);

        write_file_func(image_out, image_info);
    }
    else
        process_file(false, file, 0, image_info);

    return EXIT_SUCCESS;
}
