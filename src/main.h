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

typedef struct
{
    char *type;
    bool (*is_type)(char *);
    int (*read)(struct _image_info_t *);
    int (*write)(struct _image_info_t);
}
image_type_t;

extern image_type_t *init_png(void);
extern image_type_t *init_tiff(void);

#endif
