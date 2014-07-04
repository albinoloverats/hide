#include <errno.h>

#include <stdio.h>
#include <stdlib.h>

#include "main.h"

typedef struct
{
    uint8_t *head;
    uint64_t len;
}
jp_t;

void read_file_jpeg(char *file_name, image_info_t *image_info)
{
    errno = EXIT_SUCCESS;

    FILE *jpg = fopen(file_name, "rb");
    if (!jpg)
        return;

    jp_t *data = calloc(1, sizeof( jp_t ));

    for (uint64_t sz = 0;; sz++)
    {
        if (sz == data->len)
        {
            data->len += 1024;
            data->head = realloc(data->head, data->len);
            if (!data->head)
                goto cleanup;
        }
        uint8_t b;
        fread(&b, sizeof b, 1, jpg);
        data->head[sz] = b;
        if (b == 0xFF)
        {
            sz++;
            fread(&b, sizeof b, 1, jpg);
            data->head[sz] = b;
            if (b == 0xDA)
            {
                data->len = (++sz);
                break;
            }
        }
    }

    image_info->pixel_height = 1;
    image_info->pixel_width = 0;
    image_info->bytes_per_pixel = 3;
    image_info->buffer = (uint8_t **)malloc(sizeof( uint8_t * ));
    image_info->extra = data;

    for (uint64_t sz = 0;; sz++)
    {
        if (sz == image_info->pixel_width)
        {
            image_info->pixel_width += 1024;
            image_info->buffer[0] = realloc(image_info->buffer[0], image_info->pixel_width);
            if (!image_info->buffer[0])
                goto cleanup;
        }
        uint8_t b;
        fread(&b, sizeof b, 1, jpg);
        image_info->buffer[0][sz] = b;
        if (b == 0xFF)
        {
            sz++;
            if (sz == image_info->pixel_width)
            {
                image_info->pixel_width += 1024;
                image_info->buffer[0] = realloc(image_info->buffer[0], image_info->pixel_width);
                if (!image_info->buffer[0])
                    goto cleanup;
            }
            fread(&b, sizeof b, 1, jpg);
            image_info->buffer[0][sz] = b;
            if (b == 0xD9)
            {
                image_info->pixel_width = (++sz) - 2;
                break;
            }
        }
    }

cleanup:
    fclose(jpg);
}

void write_file_jpeg(char *file_name, image_info_t image_info)
{
    errno = EXIT_SUCCESS;

    FILE *jpg = fopen(file_name, "wb");
    if (!jpg)
        return;

    jp_t *data = image_info.extra;
    fwrite(data->head, data->len, 1, jpg);
    fwrite(image_info.buffer[0], image_info.pixel_width + 2, 1, jpg);
    uint8_t b = 0x0A;
    fwrite(&b, sizeof b, 1, jpg);

    fclose(jpg);

    free(data->head);
    free(image_info.buffer[0]);
    free(image_info.buffer);
}
