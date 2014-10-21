#ifndef _INCLUDED_ALREADY_
#define _INCLUDED_ALREADY_

#include <stdint.h>

#ifndef EFTYPE
    #define EFTYPE 79 /*!< Unsupported file/image type */
#endif


typedef struct
{
    uint64_t pixel_height;
    uint64_t pixel_width;
    uint16_t bytes_per_pixel;
    uint8_t **buffer;
    void *extra;
}
image_info_t;

extern bool is_png(char *);
extern int read_file_png(char *, image_info_t *);
extern int write_file_png(char *, image_info_t);

extern bool is_tiff(char *);
extern int read_file_tiff(char *, image_info_t *);
extern int write_file_tiff(char *, image_info_t);

#endif
