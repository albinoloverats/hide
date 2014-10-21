#ifndef _INCLUDED_ALREADY_
#define _INCLUDED_ALREADY_

#include <stdint.h>

#ifndef EFTYPE
    #define EFTYPE 79 /*!< Unsupported file/image type */
#endif


typedef struct _image_info_t
{
    char *file;
    int (*read)(struct _image_info_t *);
    int (*write)(struct _image_info_t);
    uint64_t height;
    uint64_t width;
    uint16_t bpp;
    uint8_t **buffer;
    void *extra;
}
image_info_t;

typedef struct
{
    char *file;
    uint64_t size;
    bool hide;
}
data_info_t;

extern bool is_png(char *);
extern int read_file_png(image_info_t *);
extern int write_file_png(image_info_t);

extern bool is_tiff(char *);
extern int read_file_tiff(image_info_t *);
extern int write_file_tiff(image_info_t);

#endif
