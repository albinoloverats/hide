/*
 * hide ~ A tool for hiding data inside images
 * Copyright © 2014-2015, albinoloverats ~ Software Development
 * email: hide@albinoloverats.net
 */

/**********************************************************************
 *                                                                    *
 *  Original File  : savejpg.cpp                                      *
 *  Original Author: bkenwright@xbdev.net                             *
 *  Original URL   : www.xbdev.net                                    *
 *  Original Date  : 19-01-06                                         *
 *                                                                    *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include "jpeg.h"

/**********************************************************************/


static jpeg_message_t *message;


typedef struct
{
	uint16_t marker;            // = 0xFFE0
	uint16_t length;            // = 16 for usual JPEG, no thumbnail
	char JFIFsignature[5];      // = "JFIF",'\0'
	uint8_t versionhi;          // 1
	uint8_t versionlo;          // 1
	uint8_t xyunits;            // 0 = no units, normal density
	uint16_t xdensity;          // 1
	uint16_t ydensity;          // 1
	uint8_t thumbnwidth;        // 0
	uint8_t thumbnheight;       // 0
} APP0infotype;

static APP0infotype APP0info = { 0xFFE0, 16, "JFIF", 1, 1, 0, 1, 1, 0, 0 };

typedef struct
{
	uint16_t marker;            // = 0xFFC0
	uint16_t length;            // = 17 for a truecolor YCbCr JPG
	uint8_t precision;          // Should be 8: 8 bits/sample
	uint16_t height;
	uint16_t width;
	uint8_t nrofcomponents;     //Should be 3: We encode a truecolor JPG
	uint8_t IdY;                // = 1
	uint8_t HVY;                // sampling factors for Y (bit 0-3 vert., 4-7 hor.)
	uint8_t QTY;                // Quantization Table number for Y = 0
	uint8_t IdCb;               // = 2
	uint8_t HVCb;
	uint8_t QTCb;               // 1
	uint8_t IdCr;               // = 3
	uint8_t HVCr;
	uint8_t QTCr;               // Normally equal to QTCb = 1
} SOF0infotype;

static SOF0infotype SOF0info = { 0xFFC0, 17, 8, 0, 0, 3, 1, 0x11, 0, 2, 0x11, 1, 3, 0x11, 1 };

// Default sampling factors are 1,1 for every image component: No downsampling

typedef struct
{
	uint16_t marker;            // = 0xFFDB
	uint16_t length;            // = 132
	uint8_t QTYinfo;            // = 0:  bit 0..3: number of QT = 0 (table for Y)
	//       bit 4..7: precision of QT, 0 = 8 bit
	uint8_t Ytable[64];
	uint8_t QTCbinfo;           // = 1 (quantization table for Cb,Cr}
	uint8_t Cbtable[64];
} DQTinfotype;

static DQTinfotype DQTinfo;

// Ytable from DQTinfo should be equal to a scaled and zizag reordered version
// of the table which can be found in "tables.h": std_luminance_qt
// Cbtable , similar = std_chrominance_qt
// We'll init them in the program using set_DQTinfo function

typedef struct
{
	uint16_t marker;            // = 0xFFC4
	uint16_t length;            //0x01A2
	uint8_t HTYDCinfo;          // bit 0..3: number of HT (0..3), for Y =0
	// bit 4  :type of HT, 0 = DC table,1 = AC table
	// bit 5..7: not used, must be 0
	uint8_t YDC_nrcodes[16];    //at index i = nr of codes with length i
	uint8_t YDC_values[12];
	uint8_t HTYACinfo;          // = 0x10
	uint8_t YAC_nrcodes[16];
	uint8_t YAC_values[162];    //we'll use the standard Huffman tables
	uint8_t HTCbDCinfo;         // = 1
	uint8_t CbDC_nrcodes[16];
	uint8_t CbDC_values[12];
	uint8_t HTCbACinfo;         //  = 0x11
	uint8_t CbAC_nrcodes[16];
	uint8_t CbAC_values[162];
} DHTinfotype;

static DHTinfotype DHTinfo;

typedef struct
{
	uint16_t marker;            // = 0xFFDA
	uint16_t length;            // = 12
	uint8_t nrofcomponents;     // Should be 3: truecolor JPG
	uint8_t IdY;                //1
	uint8_t HTY;                //0 // bits 0..3: AC table (0..3)
	// bits 4..7: DC table (0..3)
	uint8_t IdCb;               //2
	uint8_t HTCb;               //0x11
	uint8_t IdCr;               //3
	uint8_t HTCr;               //0x11
	uint8_t Ss, Se, Bf;         // not interesting, they should be 0,63,0
} SOSinfotype;

static SOSinfotype SOSinfo = { 0xFFDA, 12, 3, 1, 0, 2, 0x11, 3, 0x11, 0, 0x3F, 0 };

typedef struct
{
	uint8_t R;
	uint8_t G;
	uint8_t B;
} colorRGB;

typedef struct
{
	uint8_t length;
	uint16_t value;
} bitstring;

#define  Y(R,G,B) ((uint8_t)((YRtab[(R)]  + YGtab[(G)]  + YBtab[(B)])  >> 16) - 128)
#define Cb(R,G,B) ((uint8_t)((CbRtab[(R)] + CbGtab[(G)] + CbBtab[(B)]) >> 16))
#define Cr(R,G,B) ((uint8_t)((CrRtab[(R)] + CrGtab[(G)] + CrBtab[(B)]) >> 16))

#define writebyte(b) fputc((b), fp_jpeg_stream)
#define writeword(w) writebyte((w) / 256); writebyte((w) % 256)
#define writetext(t) fputs((t), fp_jpeg_stream)

static uint8_t zigzag[64] =
{
	 0,  1,  5,  6, 14, 15, 27, 28,
	 2,  4,  7, 13, 16, 26, 29, 42,
	 3,  8, 12, 17, 25, 30, 41, 43,
	 9, 11, 18, 24, 31, 40, 44, 53,
	10, 19, 23, 32, 39, 45, 52, 54,
	20, 22, 33, 38, 46, 51, 55, 60,
	21, 34, 37, 47, 50, 56, 59, 61,
	35, 36, 48, 49, 57, 58, 62, 63
};

// These are the sample quantization tables given in JPEG spec section K.1.
// The spec says that the values given produce "good" quality, and
// when divided by 2, "very good" quality
static uint8_t std_luminance_qt[64] =
{
	16, 11, 10, 16,  24,  40,  51,  61,
	12, 12, 14, 19,  26,  58,  60,  55,
	14, 13, 16, 24,  40,  57,  69,  56,
	14, 17, 22, 29,  51,  87,  80,  62,
	18, 22, 37, 56,  68, 109, 103,  77,
	24, 35, 55, 64,  81, 104, 113,  92,
	49, 64, 78, 87, 103, 121, 120, 101,
	72, 92, 95, 98, 112, 100, 103,  99
};

static uint8_t std_chrominance_qt[64] =
{
	17, 18, 24, 47, 99, 99, 99, 99,
	18, 21, 26, 66, 99, 99, 99, 99,
	24, 26, 56, 99, 99, 99, 99, 99,
	47, 66, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99
};

// Standard Huffman tables (cf. JPEG standard section K.3)

static uint8_t std_dc_luminance_nrcodes[17] = { 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static uint8_t std_dc_luminance_values[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

static uint8_t std_dc_chrominance_nrcodes[17] = { 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static uint8_t std_dc_chrominance_values[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

static uint8_t std_ac_luminance_nrcodes[17] = { 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };

static uint8_t std_ac_luminance_values[162] =
{
	0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
	0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
	0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
	0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
	0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
	0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
	0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
	0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
	0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
	0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa
};

static uint8_t std_ac_chrominance_nrcodes[17] = { 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };

static uint8_t std_ac_chrominance_values[162] =
{
	0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
	0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
	0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
	0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
	0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
	0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
	0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
	0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
	0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
	0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
	0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
	0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
	0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa
};

static const double AAN_SCALE_FACTOR[8] =
{
	1.0, 1.387039845, 1.306562965, 1.175875602,
	1.0, 0.785694958, 0.541196100, 0.275899379
};

/**********************************************************************/

static uint8_t bytenew = 0;     // The byte that will be written in the JPG file
static int8_t bytepos = 7;      // bit position in the byte we write (bytenew)
                                // should be<=7 and >=0
static uint16_t mask[16] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 };

// The Huffman tables we'll use:
static bitstring   YDC_HT[12]   = { { 0x00, 0x0000 } };
static bitstring  CbDC_HT[12]   = { { 0x00, 0x0000 } };
static bitstring  YAC_HT[256]   = { { 0x00, 0x0000 } };
static bitstring CbAC_HT[256]   = { { 0x00, 0x0000 } };

static uint8_t    category_alloc[65535] = { 0x0 };
static uint8_t   *category      = NULL;     // Here we'll keep the category of the numbers in range: -32767..32767
static bitstring  bitcode_alloc[65535]  = { { 0x00, 0x0000 } };
static bitstring *bitcode       = NULL;     // their bitcoded representation

// Precalculated tables for a faster YCbCr->RGB transformation
// We use a int32_t table because we'll scale values by 2^16 and work with integers
static int32_t  YRtab[256]      = { 0x0 };
static int32_t  YGtab[256]      = { 0x0 };
static int32_t  YBtab[256]      = { 0x0 };
static int32_t CbRtab[256]      = { 0x0 };
static int32_t CbGtab[256]      = { 0x0 };
static int32_t CbBtab[256]      = { 0x0 };
static int32_t CrRtab[256]      = { 0x0 };
static int32_t CrGtab[256]      = { 0x0 };
static int32_t CrBtab[256]      = { 0x0 };

static double  fdtbl_Y[64]       = { 0x0 };
static double fdtbl_Cb[64]       = { 0x0 };  // the same with the fdtbl_Cr[64]

static colorRGB *RGB_buffer     = NULL;     // image to be encoded
static uint32_t Ximage;
static uint32_t Yimage;                     // image dimensions divisible by 8

static int8_t     YDU[64]       = { 0x0 };  // This is the Data Unit of Y after YCbCr->RGB transformation
static int8_t    CbDU[64]       = { 0x0 };
static int8_t    CrDU[64]       = { 0x0 };
static int16_t DU_DCT[64]       = { 0x0 };  // Current DU (after DCT and quantization) which we'll zigzag
static int16_t     DU[64]       = { 0x0 };  // zigzag reordered DU which will be Huffman coded

static FILE *fp_jpeg_stream     = NULL;

/**********************************************************************/

static void write_APP0info(void)
// Nothing to overwrite for APP0info
{
	writeword(APP0info.marker);
	writeword(APP0info.length);
	writetext(APP0info.JFIFsignature);
	writebyte(0); // extra nul necessary as JFIF should be \0 terminated
	writebyte(APP0info.versionhi);
	writebyte(APP0info.versionlo);
	writebyte(APP0info.xyunits);
	writeword(APP0info.xdensity);
	writeword(APP0info.ydensity);
	writebyte(APP0info.thumbnwidth);
	writebyte(APP0info.thumbnheight);
}

static void write_SOF0info(void)
// We should overwrite width and height
{
	writeword(SOF0info.marker);
	writeword(SOF0info.length);
	writebyte(SOF0info.precision);
	writeword(SOF0info.height);
	writeword(SOF0info.width);
	writebyte(SOF0info.nrofcomponents);
	writebyte(SOF0info.IdY);
	writebyte(SOF0info.HVY);
	writebyte(SOF0info.QTY);
	writebyte(SOF0info.IdCb);
	writebyte(SOF0info.HVCb);
	writebyte(SOF0info.QTCb);
	writebyte(SOF0info.IdCr);
	writebyte(SOF0info.HVCr);
	writebyte(SOF0info.QTCr);
}

static void write_DQTinfo(void)
{
	writeword(DQTinfo.marker);
	writeword(DQTinfo.length);
	writebyte(DQTinfo.QTYinfo);
	for (int i = 0; i < 64; i++)
		writebyte(DQTinfo.Ytable[i]);
	writebyte(DQTinfo.QTCbinfo);
	for (int i = 0; i < 64; i++)
		writebyte(DQTinfo.Cbtable[i]);
}

// Set quantization table and zigzag reorder it
#define set_quant_table(basic_table, scale_factor, newtable)            \
	for (int i = 0; i < 64; i++)                                    \
		newtable[zigzag[i]] = byte_limit((basic_table[i] * scale_factor + 50) / 100, 1)

static void set_DQTinfo(void)
{
	// scalefactor controls the visual quality of the image
	// the smaller is, the better image we'll get, and the smaller
	// compression we'll achieve
	uint8_t scalefactor = 1; /* this could be a parameter */
	DQTinfo.marker = 0xFFDB;
	DQTinfo.length = 132;
	DQTinfo.QTYinfo = 0;
	DQTinfo.QTCbinfo = 1;
	set_quant_table(std_luminance_qt, scalefactor, DQTinfo.Ytable);
	set_quant_table(std_chrominance_qt, scalefactor, DQTinfo.Cbtable);
}

static void write_DHTinfo(void)
{
	writeword(DHTinfo.marker);
	writeword(DHTinfo.length);
	writebyte(DHTinfo.HTYDCinfo);
	for (int i = 0; i < 16; i++)
		writebyte(DHTinfo.YDC_nrcodes[i]);
	for (int i = 0; i <= 11; i++)
		writebyte(DHTinfo.YDC_values[i]);
	writebyte(DHTinfo.HTYACinfo);
	for (int i = 0; i < 16; i++)
		writebyte(DHTinfo.YAC_nrcodes[i]);
	for (int i = 0; i <= 161; i++)
		writebyte(DHTinfo.YAC_values[i]);
	writebyte(DHTinfo.HTCbDCinfo);
	for (int i = 0; i < 16; i++)
		writebyte(DHTinfo.CbDC_nrcodes[i]);
	for (int i = 0; i <= 11; i++)
		writebyte(DHTinfo.CbDC_values[i]);
	writebyte(DHTinfo.HTCbACinfo);
	for (int i = 0; i < 16; i++)
		writebyte(DHTinfo.CbAC_nrcodes[i]);
	for (int i = 0; i <= 161; i++)
		writebyte(DHTinfo.CbAC_values[i]);
}

static void set_DHTinfo(void)
{
	DHTinfo.marker = 0xFFC4;
	DHTinfo.length = 0x01A2;
	DHTinfo.HTYDCinfo = 0;
	for (int i = 0; i < 16; i++)
		DHTinfo.YDC_nrcodes[i] = std_dc_luminance_nrcodes[i + 1];
	for (int i = 0; i <= 11; i++)
		DHTinfo.YDC_values[i] = std_dc_luminance_values[i];
	DHTinfo.HTYACinfo = 0x10;
	for (int i = 0; i < 16; i++)
		DHTinfo.YAC_nrcodes[i] = std_ac_luminance_nrcodes[i + 1];
	for (int i = 0; i <= 161; i++)
		DHTinfo.YAC_values[i] = std_ac_luminance_values[i];
	DHTinfo.HTCbDCinfo = 1;
	for (int i = 0; i < 16; i++)
		DHTinfo.CbDC_nrcodes[i] = std_dc_chrominance_nrcodes[i + 1];
	for (int i = 0; i <= 11; i++)
		DHTinfo.CbDC_values[i] = std_dc_chrominance_values[i];
	DHTinfo.HTCbACinfo = 0x11;
	for (int i = 0; i < 16; i++)
		DHTinfo.CbAC_nrcodes[i] = std_ac_chrominance_nrcodes[i + 1];
	for (int i = 0; i <= 161; i++)
		DHTinfo.CbAC_values[i] = std_ac_chrominance_values[i];
}

static void write_SOSinfo(void)
// Nothing to overwrite for SOSinfo
{
	writeword(SOSinfo.marker);
	writeword(SOSinfo.length);
	writebyte(SOSinfo.nrofcomponents);
	writebyte(SOSinfo.IdY);
	writebyte(SOSinfo.HTY);
	writebyte(SOSinfo.IdCb);
	writebyte(SOSinfo.HTCb);
	writebyte(SOSinfo.IdCr);
	writebyte(SOSinfo.HTCr);
	writebyte(SOSinfo.Ss);
	writebyte(SOSinfo.Se);
	writebyte(SOSinfo.Bf);
}

static void writebits(bitstring bs)
{
	// bit position in the bitstring we read, should be<=15 and >=0
	for (int posval = bs.length - 1; posval >= 0; )
	{
		if (bs.value & mask[posval])
			bytenew |= mask[bytepos];
		posval--;
		bytepos--;
		if (bytepos < 0)
		{
			if (bytenew == 0xFF)
			{
				writebyte(0xFF);
				writebyte(0);
			}
			else
				writebyte(bytenew);
			bytepos = 7;
			bytenew = 0;
		}
	}
}

#define compute_Huffman_table(nrcodes, std_table, HT)                   \
	do                                                              \
		for (int i = 1, c = 0, p = 0; i <= 16; i++, c *= 2)     \
			for (int j = 1; j <= nrcodes[i]; j++, p++, c++) \
			{                                               \
				HT[std_table[p]].value = c;             \
				HT[std_table[p]].length = i;            \
			}                                               \
	while (0)

static void init_Huffman_tables(void)
{
	compute_Huffman_table(std_dc_luminance_nrcodes,   std_dc_luminance_values,   YDC_HT);
	compute_Huffman_table(std_dc_chrominance_nrcodes, std_dc_chrominance_values, CbDC_HT);
	compute_Huffman_table(std_ac_luminance_nrcodes,   std_ac_luminance_values,   YAC_HT);
	compute_Huffman_table(std_ac_chrominance_nrcodes, std_ac_chrominance_values, CbAC_HT);
}

static void set_numbers_category_and_bitcode(void)
{
	int32_t nrlower = 1;
	int32_t nrupper = 2;

	category = category_alloc + 32767; // allow negative subscripts
	bitcode = bitcode_alloc + 32767;

	for (int cat = 1; cat <= 15; cat++)
	{
		// Positive numbers
		for (int nr = nrlower; nr < nrupper; nr++)
		{
			category[nr] = cat;
			bitcode[nr].length = cat;
			bitcode[nr].value = (uint16_t)nr;
		}
		// Negative numbers
		for (int nr = -(nrupper - 1); nr <= -nrlower; nr++)
		{
			category[nr] = cat;
			bitcode[nr].length = cat;
			bitcode[nr].value = (uint16_t)(nrupper - 1 + nr);
		}
		nrlower <<= 1;
		nrupper <<= 1;
	}
}

#define RED_Y     19595.76400
#define RED_Cb   -11058.04464
#define RED_Cr    32768.00000

#define GREEN_Y   38470.13200
#define GREEN_Cb -21708.95536
#define GREEN_Cr -27438.76784

#define BLUE_Y     7471.60400
#define BLUE_Cb   32768.00000
#define BLUE_Cr   -5328.23216

static void precalculate_YCbCr_tables(void)
{
	for (int i = 0; i <= 255; i++)
	{
		// red values
		YRtab[i]  = (int32_t)(RED_Y    * i); // 65536 *  0.29900 + 0.5
		CbRtab[i] = (int32_t)(RED_Cb   * i); // 65536 * -0.16874 + 0.5
		CrRtab[i] = (int32_t)(RED_Cr   * i); // 32768
		// green values
		YGtab[i]  = (int32_t)(GREEN_Y  * i); // 65536 *  0.58700 + 0.5
		CbGtab[i] = (int32_t)(GREEN_Cb * i); // 65536 * -0.33126 + 0.5
		CrGtab[i] = (int32_t)(GREEN_Cr * i); // 65536 * -0.41869 + 0.5
		// blue values
		YBtab[i]  = (int32_t)(BLUE_Y   * i); // 65536 *  0.11400 + 0.5
		CbBtab[i] = (int32_t)(BLUE_Cb  * i); // 32768
		CrBtab[i] = (int32_t)(BLUE_Cr  * i); // 65536 * -0.08131 + 0.5
	}
}

// Using a bit modified form of the FDCT routine from IJG's C source:
// Forward DCT routine idea taken from Independent JPEG Group's C source for
// JPEG encoders/decoders

// For double AA&N IDCT method, divisors are equal to quantization
// coefficients scaled by scalefactor[row]*scalefactor[col], where
// scalefactor[0] = 1
// scalefactor[k] = cos(k*M_PI/16) * sqrt(2)    for k=1..7
// We apply a further scale factor of 8.
// What's actually stored is 1/divisor so that the inner loop can
// use a multiplication rather than a division.
static void prepare_quant_tables(void)
{
	for (int i = 0, row = 0; row < 8; row++)
		for (int col = 0; col < 8; col++, i++)
		{
			fdtbl_Y[i] = 1.0 /                   \
				((DQTinfo.Ytable[zigzag[i]]  \
				* AAN_SCALE_FACTOR[row]      \
				* AAN_SCALE_FACTOR[col])     \
				* 8);
			fdtbl_Cb[i] = 1.0 /                  \
				((DQTinfo.Cbtable[zigzag[i]] \
				* AAN_SCALE_FACTOR[row]      \
				* AAN_SCALE_FACTOR[col])     \
				* 8);
		}
}

static void fdct_and_quantization(int8_t *data, double *fdtbl, int16_t *outdata)
{
	double tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
	double tmp10, tmp11, tmp12, tmp13;
	double z1, z2, z3, z4, z5, z11, z13;
	double *dataptr;
	double datafloat[64];

	for (int i = 0; i < 64; i++)
		datafloat[i] = data[i];

	// Pass 1: process rows.
	dataptr = datafloat;
	for (int i = 7; i >= 0; i--)
	{
		tmp0 = dataptr[0] + dataptr[7];
		tmp7 = dataptr[0] - dataptr[7];
		tmp1 = dataptr[1] + dataptr[6];
		tmp6 = dataptr[1] - dataptr[6];
		tmp2 = dataptr[2] + dataptr[5];
		tmp5 = dataptr[2] - dataptr[5];
		tmp3 = dataptr[3] + dataptr[4];
		tmp4 = dataptr[3] - dataptr[4];

		// Even part

		tmp10 = tmp0 + tmp3;                // phase 2
		tmp13 = tmp0 - tmp3;
		tmp11 = tmp1 + tmp2;
		tmp12 = tmp1 - tmp2;

		dataptr[0] = tmp10 + tmp11;         // phase 3
		dataptr[4] = tmp10 - tmp11;

		z1 = 0.707106781 * (tmp12 + tmp13); // c4
		dataptr[2] = tmp13 + z1;            // phase 5
		dataptr[6] = tmp13 - z1;

		// Odd part

		tmp10 = tmp4 + tmp5;                // phase 2
		tmp11 = tmp5 + tmp6;
		tmp12 = tmp6 + tmp7;

		// The rotator is modified from fig 4-8 to avoid extra negations
		z5 = 0.382683433 * (tmp10 - tmp12); // c6
		z2 = 0.541196100 * tmp10 + z5;      // c2-c6
		z4 = 1.306562965 * tmp12 + z5;      // c2+c6
		z3 = 0.707106781 * tmp11;           // c4

		z11 = tmp7 + z3;                    // phase 5
		z13 = tmp7 - z3;

		dataptr[5] = z13 + z2;              // phase 6
		dataptr[3] = z13 - z2;
		dataptr[1] = z11 + z4;
		dataptr[7] = z11 - z4;

		dataptr += 8;                       // advance pointer to next row
	}

	// Pass 2: process columns

	dataptr = datafloat;
	for (int i = 7; i >= 0; i--)
	{
		tmp0 = dataptr[0] + dataptr[56];
		tmp7 = dataptr[0] - dataptr[56];
		tmp1 = dataptr[8] + dataptr[48];
		tmp6 = dataptr[8] - dataptr[48];
		tmp2 = dataptr[16] + dataptr[40];
		tmp5 = dataptr[16] - dataptr[40];
		tmp3 = dataptr[24] + dataptr[32];
		tmp4 = dataptr[24] - dataptr[32];

		// Even part

		tmp10 = tmp0 + tmp3;                // phase 2
		tmp13 = tmp0 - tmp3;
		tmp11 = tmp1 + tmp2;
		tmp12 = tmp1 - tmp2;

		dataptr[0] = tmp10 + tmp11;         // phase 3
		dataptr[32] = tmp10 - tmp11;

		z1 = 0.707106781 * (tmp12 + tmp13); // c4
		dataptr[16] = tmp13 + z1;           // phase 5
		dataptr[48] = tmp13 - z1;

		// Odd part

		tmp10 = tmp4 + tmp5;                // phase 2
		tmp11 = tmp5 + tmp6;
		tmp12 = tmp6 + tmp7;

		// The rotator is modified from fig 4-8 to avoid extra negations.
		z5 = 0.382683433 * (tmp10 - tmp12); // c6
		z2 = 0.541196100 * tmp10 + z5;      // c2-c6
		z4 = 1.306562965 * tmp12 + z5;      // c2+c6
		z3 = 0.707106781 * tmp11;           // c4

		z11 = tmp7 + z3;                    // phase 5
		z13 = tmp7 - z3;
		dataptr[40] = z13 + z2;             // phase 6
		dataptr[24] = z13 - z2;
		dataptr[8] = z11 + z4;
		dataptr[56] = z11 - z4;

		dataptr++;                          // advance pointer to next column
	}

	// Quantize/descale the coefficients, and store into output array
	// Apply the quantization and scaling factor
	// Round to nearest integer.
	// Since C does not specify the direction of rounding for negative
	// quotients, we have to force the dividend positive for portability.
	// The maximum coefficient size is +-16K (for 12-bit data), so this
	// code should work for either 16-bit or 32-bit ints.
	for (int i = 0; i < 64; i++)
		outdata[i] = (int16_t)((int16_t)(datafloat[i] * fdtbl[i] + 16384.5) - 16384);
}

static void process_DU(int8_t *ComponentDU, double *fdtbl, int16_t *DC, bitstring *HTDC, bitstring *HTAC)
{
	bitstring EOB = HTAC[0x00];
	bitstring M16zeroes = HTAC[0xF0];
	uint8_t end0pos;

	fdct_and_quantization(ComponentDU, fdtbl, DU_DCT);
	// zigzag reorder
	for (int i = 0; i <= 63; i++)
		DU[zigzag[i]] = DU_DCT[i];


	static uint64_t offset = 0;
	static uint8_t bit = 1;

	for (uint64_t i = 0; i < 64 && offset < message->size + sizeof message->size; i++)
		if (DU[i] > 1)
		{
			uint8_t v = (*(message->data + offset) & bit);
			if (v)
				DU[i] |= 0x0001;
			else
				DU[i] &= 0xFFFE;
			bit <<= 1;
			if (!bit)
			{
				bit = 1;
				offset++;
			}
		}


	int16_t diff = DU[0] - *DC;
	*DC = DU[0];
	// Encode DC
	if (diff == 0)
		writebits(HTDC[0]); // diff might be 0
	else
	{
		writebits(HTDC[category[diff]]);
		writebits(bitcode[diff]);
	}
	// Encode ACs
	for (end0pos = 63; (end0pos > 0) && (DU[end0pos] == 0); end0pos--);
	// end0pos = first element in reverse order !=0
	if (end0pos == 0)
	{
		writebits(EOB);
		return;
	}

	for (int i = 1; i <= end0pos; i++)
	{
		int startpos = i;
		for (; (DU[i] == 0) && (i <= end0pos); i++);
		int nrzeroes = i - startpos;
		if (nrzeroes >= 16)
		{
			for (int nrmarker = 1; nrmarker <= (nrzeroes >> 4); nrmarker++)
				writebits(M16zeroes);
			nrzeroes %= 16;
		}
		writebits(HTAC[(nrzeroes << 4) + category[DU[i]]]);
		writebits(bitcode[DU[i]]);
	}
	if (end0pos != 63)
		writebits(EOB);
}

static void load_data_units_from_RGB_buffer(int xpos, int ypos)
{
	uint8_t pos = 0;
	uint32_t location = ypos * Ximage + xpos;
	for (int y = 0; y < 8; y++)
	{
		for (int x = 0; x < 8; x++)
		{
			int R = RGB_buffer[location].R;
			int G = RGB_buffer[location].G;
			int B = RGB_buffer[location].B;
			YDU[pos] = Y(R, G, B);
			CbDU[pos] = Cb(R, G, B);
			CrDU[pos] = Cr(R, G, B);
			location++;
			pos++;
		}
		location += Ximage - 8;
	}
}

static void main_encoder(void)
{
	int16_t DCY = 0, DCCb = 0, DCCr = 0; // DC coefficients used for differential encoding
	for (uint32_t ypos = 0; ypos < Yimage; ypos += 8)
		for (uint32_t xpos = 0; xpos < Ximage; xpos += 8)
		{
			load_data_units_from_RGB_buffer(xpos, ypos);
			process_DU(YDU, fdtbl_Y, &DCY, YDC_HT, YAC_HT);
			process_DU(CbDU, fdtbl_Cb, &DCCb, CbDC_HT, CbAC_HT);
			process_DU(CrDU, fdtbl_Cb, &DCCr, CbDC_HT, CbAC_HT);
		}
}

static void init_all(void)
{
	set_DQTinfo();
	set_DHTinfo();
	init_Huffman_tables();
	set_numbers_category_and_bitcode();
	precalculate_YCbCr_tables();
	prepare_quant_tables();
}


extern void jpeg_encode_data(FILE *file, jpeg_message_t *msg, jpeg_image_t *info)
{
	message = msg;

	bitstring fillbits; // filling bitstring for the bit alignment of the EOI marker

	Ximage = info->width;
	Yimage = info->height;

	uint32_t Xdiv8 = (Ximage % 8) ? ((Ximage / 8) << 3) + 8 : Ximage;
	uint32_t Ydiv8 = (Yimage % 8) ? ((Yimage / 8) << 3) + 8 : Yimage;

	// The image we encode shall be filled with the last line and the last column
	// from the original bitmap, until Ximage and Yimage are divisible by 8
	// Load BMP image from disk and complete X
	RGB_buffer = calloc(Xdiv8 * Ydiv8, sizeof (colorRGB));

	//uint8_t nr_fillingbytes = (Ximage % 4) ? 4 - (Ximage % 4) : 0;
	for (uint32_t nrline = 0; nrline < Yimage; nrline++)
		memcpy(RGB_buffer + nrline * Xdiv8, info->rgb[nrline], Ximage * 3);
	Ximage = Xdiv8;
	Yimage = Ydiv8;


	fp_jpeg_stream = file;
	init_all();
	SOF0info.width = info->width;
	SOF0info.height = info->height;
	writeword(0xFFD8); // SOI
	write_APP0info();

	write_DQTinfo();
	write_SOF0info();
	write_DHTinfo();
	write_SOSinfo();

	bytenew = 0;
	bytepos = 7;
	main_encoder();
	// Do the bit alignment of the EOI marker
	if (bytepos >= 0)
	{
		fillbits.length = bytepos + 1;
		fillbits.value = (1 << (bytepos + 1)) - 1;
		writebits(fillbits);
	}
	writeword(0xFFD9); //EOI
	free(RGB_buffer);
}
