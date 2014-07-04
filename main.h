#ifndef _INCLUDED_ALREADY_
#define _INCLUDED_ALREADY_

#ifndef EFTYPE
    #define EFTYPE 79
#endif

/*
 * (currently) taken from our common.h
 *
 * TODO use common.h
 */
#ifndef __bswap_64
    #define __bswap_64(x) /*!< Define ourselves a 8-byte swap macro */ \
        ((((x) & 0xff00000000000000ull) >> 56) \
       | (((x) & 0x00ff000000000000ull) >> 40) \
       | (((x) & 0x0000ff0000000000ull) >> 24) \
       | (((x) & 0x000000ff00000000ull) >> 8)  \
       | (((x) & 0x00000000ff000000ull) << 8)  \
       | (((x) & 0x0000000000ff0000ull) << 24) \
       | (((x) & 0x000000000000ff00ull) << 40) \
       | (((x) & 0x00000000000000ffull) << 56))
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN || BYTE_ORDER == LITTLE_ENDIAN || _WIN32
    #define ntohll(x) __bswap_64(x) /*!< Do need to swap bytes from network byte order */
    #define htonll(x) __bswap_64(x) /*!< Do need to swap bytes to network byte order */
#elif __BYTE_ORDER == __BIG_ENDIAN || BYTE_ORDER == BIG_ENDIAN
    #define ntohll(x) (x) /*!< No need to swap bytes from network byte order */
    #define htonll(x) (x) /*!< No need to swap bytes to network byte order */
#else
    #error "Unknown endianness!"
#endif

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
