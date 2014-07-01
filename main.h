#ifndef _INCLUDED_ALREADY_
#define _INCLUDED_ALREADY_

typedef struct
{
    uint64_t pixel_height;
    uint64_t pixel_width;
    uint16_t bytes_per_pixel;
    uint8_t **buffer;
}
image_info_t;

void read_file_png(char *, image_info_t *);
void write_file_png(char *, image_info_t);

void read_file_tiff(char *, image_info_t *);
void write_file_tiff(char *, image_info_t);

#endif
