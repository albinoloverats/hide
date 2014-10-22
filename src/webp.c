#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <unistd.h>

#include <webp/encode.h>
#include <webp/decode.h>

#include "imagine.h"

static bool is_webp(char *file_name)
{
    FILE *fp = fopen(file_name, "rb");
    if (!fp)
        return false;

    uint8_t header[1024];
    fread(header, 1, sizeof header, fp);
    fclose(fp);

    return WebPGetInfo(header, sizeof header, NULL, NULL);
}

static int read_webp(image_info_t *image_info)
{
    errno = EXIT_SUCCESS;

    FILE *fp = fopen(image_info->file, "rb");
    if (!fp)
        return errno;

    uint8_t header[1024];
    fread(header, 1, sizeof header, fp);

    WebPBitstreamFeatures feat;
    WebPGetFeatures(header, sizeof header, &feat);

    image_info->width = feat.width;
    image_info->height = feat.height;
    image_info->bpp = feat.has_alpha ? 4 : 3;

    fseek(fp, 0, SEEK_END);
    uint64_t l = ftell(fp);
    uint8_t *raw = malloc(l);
    fseek(fp, 0, SEEK_SET);

    fread(raw, 1, l, fp);
    fclose(fp);

    uint8_t *(*webpdecode)(const uint8_t *, size_t, int *, int *) = feat.has_alpha ? WebPDecodeRGBA : WebPDecodeRGB;
    uint8_t *img = webpdecode(raw, l, &feat.width, &feat.height);
    free(raw);

    image_info->buffer = malloc(sizeof (uint8_t *) * image_info->height);
    l = image_info->width * image_info->bpp;
    for (uint64_t y = 0; y < image_info->height; y++)
    {
        image_info->buffer[y] = malloc(l);
        memcpy(image_info->buffer[y], img + y * l, l);
    }
    free(img);

    return errno;
}

static int write_webp(image_info_t image_info)
{
    errno = EXIT_SUCCESS;

    FILE *fp = fopen(image_info.file, "wb");
    if (!fp)
        return errno;

    uint8_t *raw = NULL;
    uint64_t l = image_info.width * image_info.bpp;
    uint8_t *img = malloc(l * image_info.height);
    for (uint64_t y = 0; y < image_info.height; y++)
    {
        memcpy(img + y * l, image_info.buffer[y], l);
        free(image_info.buffer[y]);
    }
    free(image_info.buffer);

    size_t (*webpencode)(const uint8_t *, int, int, int, uint8_t **) = image_info.bpp == 4 ? WebPEncodeLosslessRGBA : WebPEncodeLosslessRGB;
    l = webpencode(img, image_info.width, image_info.height, l, &raw);

    fwrite(raw, 1, l, fp);
    free(raw);
    fclose(fp);

    return errno;
}

extern image_type_t *init(void)
{
    image_type_t *webp = malloc(sizeof (image_type_t));
    webp->type = strdup("WEBP");
    webp->is_type = is_webp;
    webp->read = read_webp;
    webp->write = write_webp;
    return webp;
}
