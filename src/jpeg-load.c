/*
 * hide ~ A tool for hiding data inside images
 * Copyright © 2014-2015, albinoloverats ~ Software Development
 * email: hide@albinoloverats.net
 */

/**********************************************************************
 *                                                                    *
 *  Original File  : loadjpg.cpp                                      *
 *  Original Author: bkenwright@xbdev.net                             *
 *  Original Date  : 19-01-06                                         *
 *  Revised Date   : 26-07-07                                         *
 *                                                                    *
 **********************************************************************/

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>

#include "common/common.h"

#include "jpeg.h"

/**********************************************************************/

#define DQT  0xDB   // Define Quantization Table
#define SOF  0xC0   // Start of Frame (size information)
#define DHT  0xC4   // Huffman Table
#define SOI  0xD8   // Start of Image
#define SOS  0xDA   // Start of Scan
#define EOI  0xD9   // End of Image, or End of File
#define APP0 0xE0

#define BYTE_TO_WORD(x) (((x)[0]<<8)|(x)[1])

#define HUFFMAN_TABLES  4
#define COMPONENTS      4


static jpeg_message_t *message;
static jpeg_load_e action;
static uint64_t fill_size = 0;


static const int cY = 1;
static const int cCb = 2;
static const int cCr = 3;

static int ZigZagArray[64] =
{
	 0,  1,  5,  6, 14, 15, 27, 28,
	 2,  4,  7, 13, 16, 26, 29, 42,
	 3,  8, 12, 17, 25, 30, 41, 43,
	 9, 11, 18, 24, 31, 40, 44, 53,
	10, 19, 23, 32, 39, 45, 52, 54,
	20, 22, 33, 38, 46, 51, 55, 60,
	21, 34, 37, 47, 50, 56, 59, 61,
	35, 36, 48, 49, 57, 58, 62, 63,
};

/**********************************************************************/

typedef struct
{
	int value;      // Decodes to.
	int length;     // Length in bits.
	uint16_t code;  // 2 byte code (variable length)
} stBlock;

/**********************************************************************/

typedef struct
{
	uint8_t m_length[17];       // 17 values from jpg file,
	// k =1-16 ; L[k] indicates the number of Huffman codes of length k
	uint8_t m_hufVal[257];      // 256 codes read in from the jpeg file

	int m_numBlocks;
	stBlock m_blocks[1024];
} stHuffmanTable;

typedef struct
{
	uint32_t m_hFactor;
	uint32_t m_vFactor;
	double *m_qTable;            // Pointer to the quantisation table to use
	stHuffmanTable *m_acTable;
	stHuffmanTable *m_dcTable;
	int16_t m_DCT[65];          // DCT coef
	int m_previousDC;
} stComponent;

typedef struct
{
	uint8_t *m_rgb;             // Final Red Green Blue pixel data
	uint32_t m_width;           // Width of image
	uint32_t m_height;          // Height of image

	const uint8_t *m_stream;    // Pointer to the current stream
	int m_restart_interval;

	stComponent m_component_info[COMPONENTS];

	double m_Q_tables[COMPONENTS][64];       // quantization tables
	stHuffmanTable m_HTDC[HUFFMAN_TABLES];  // DC huffman tables
	stHuffmanTable m_HTAC[HUFFMAN_TABLES];  // AC huffman tables

	// Temp space used after the IDCT to store each components
	uint8_t m_Y[256];
	uint8_t m_Cr[64];
	uint8_t m_Cb[64];

	// Internal Pointer use for colorspace conversion, do not modify it !!!
	uint8_t *m_colourspace;
} stJpegData;

/**********************************************************************/

#define GenHuffCodes(num_codes, arr, huffVal)                           \
{                                                                       \
	for (int cc = 0, hc = 0, lc = 1; cc < num_codes; cc++, hc++)    \
	{                                                               \
		for (; arr[cc].length > lc; lc++)                       \
			hc = hc << 1;                                   \
		arr[cc].code = hc;                                      \
		arr[cc].value = huffVal[cc];                            \
	}                                                               \
}

/**********************************************************************/

#define PI_BY_16 0.19634954084936207740391521145497
/*
 * This is the biggest bottleneck when decoding an image.
 */
__attribute__((optimize("unroll-loops")))
static inline int innerIDCT(int x, int y, const int block[8][8])
{
	const double X = ((x << 1) + 1) * PI_BY_16;
	const double Y = ((y << 1) + 1) * PI_BY_16;

	double sum = 0.0;
	for (int u = 0; u < 8; u++)
	{
		const double cosxu = cos(X * u);
		for (int v = 0; v < 8; v++)
		{
			const double cosyv = cos(Y * v);
			sum += (u ? 1 : M_SQRT1_2) \
			     * (v ? 1 : M_SQRT1_2) \
			     *  block[u][v]        \
			     *  cosxu              \
			     *  cosyv;
		}
	}
	return (int)(sum / 4);
}

#define PerformIDCT(outBlock, inBlock)                                  \
{                                                                       \
	for (int y = 0; y < 8; y++)                                     \
		for (int x = 0; x < 8; x++)                             \
			outBlock[x][y] = innerIDCT(x, y, (const int (*)[8])inBlock); \
}

#define DequantizeBlock(block, quantBlock)                              \
{                                                                       \
	for (int c = 0; c < 64; c++)                                    \
		block[c] = (int)(block[c] * quantBlock[c]);             \
}

#define DeZigZag(outBlock, inBlock)                                     \
{                                                                       \
	for (int i = 0; i < 64; i++)                                    \
		outBlock[i] = inBlock[ZigZagArray[i]];                  \
}

#define TransformArray(outArray, inArray)                               \
{                                                                       \
	for (int y = 0, cc = 0; y < 8; y++)                             \
		for (int x = 0; x < 8; x++, cc++)                       \
			outArray[x][y] = inArray[cc];                   \
}

/***************************************************************************/

static void DecodeSingleBlock(stComponent *comp, uint8_t *outputBuf, int stride)
{
	int16_t *inptr = comp->m_DCT;
	double *quantptr = comp->m_qTable;

	// Create a temp 8x8, i.e. 64 array for the data
	int data[64] = { 0x0 };

	// Copy our data into the temp array
	for (int i = 0; i < 64; i++)
		data[i] = inptr[i];


	static uint64_t offset = 0;
	static uint8_t bit = 1;
	static bool aloc = false;

	for (int i = 0; i < 64 && !(aloc && offset >= message->size + sizeof message->size); i++)
		if (data[i] > 1)
		{
			switch (action)
			{
				case JPEG_LOAD_FIND:
				{
					uint8_t *ptr = (aloc ? message->data : (uint8_t *)&message->size) + offset;
					if (data[i] & 0x01)
						*ptr |= (0xFF & bit);
					bit <<= 1;
					if (!bit)
					{
						bit = 1;
						offset++;
					}
					if (offset >= sizeof message->size && !aloc)
					{
						message->size = fill_size ? : ntohll(message->size);
						message->data = calloc(message->size + sizeof message->size, sizeof (uint8_t));
						aloc = true;
					}
					break;
				}
				case JPEG_LOAD_READ:
					message->size++;
					break;
			}
		}


	// De-Quantize
	DequantizeBlock(data, quantptr);

	// De-Zig-Zag
	int block[64] = { 0x0 };
	DeZigZag(block, data);

	// Create an 8x8 array
	int arrayBlock[8][8] = { { 0x0 } };
	TransformArray(arrayBlock, block);

	// Inverse DCT
	int val[8][8] = { { 0x0 } };
	PerformIDCT(val, arrayBlock);

	// Level Shift each element (i.e. add 128), and copy to our output
	uint8_t *outptr = outputBuf;
	for (int y = 0; y < 8; y++)
	{
		for (int x = 0; x < 8; x++)
		{
			val[x][y] += 128;
			outptr[x] = byte_limit(val[x][y], 0);
		}
		outptr += stride;
	}
}

/**********************************************************************/

static void BuildHuffmanTable(const uint8_t *bits, stHuffmanTable *HT)
{
	// Takes two array of bits, and build the huffman table for size, and code
	for (int j = 1; j <= 16; j++)
		HT->m_length[j] = bits[j];

	// Work out the total number of codes
	int numBlocks = 0;
	for (int i = 1; i <= 16; i++)
		numBlocks += HT->m_length[i];
	HT->m_numBlocks = numBlocks;

	// Fill in the data our our blocks, so we know how many bits each
	// one is
	for (int i = 1, c = 0; i <= 16; i++)
		for (int j = 0; j < HT->m_length[i]; j++, c++)
			HT->m_blocks[c].length = i;

	GenHuffCodes(HT->m_numBlocks, HT->m_blocks, HT->m_hufVal);
}

/**********************************************************************/

static int ParseSOF(stJpegData *jdata, const uint8_t *stream)
{
	/*
	 * SOF          16              0xffc0          Start Of Frame
	 * Lf           16              3Nf+8           Frame header length
	 * P            8               8                       Sample precision
	 * Y            16              0-65535         Number of lines
	 * X            16              1-65535         Samples per line
	 * Nf           8               1-255           Number of image components (e.g. Y, U and V).
	 *
	 * ---------Repeats for the number of components (e.g. Nf)-----------------
	 * Ci           8               0-255           Component identifier
	 * Hi           4               1-4                     Horizontal Sampling Factor
	 * Vi           4               1-4                     Vertical Sampling Factor
	 * Tqi          8               0-3                     Quantization Table Selector.
	 */

	int height = BYTE_TO_WORD(stream + 3);
	int width = BYTE_TO_WORD(stream + 5);
	int nr_components = stream[7];

	stream += 8;
	for (int i = 0; i < nr_components; i++)
	{
		int cid = *stream++;
		int sampling_factor = *stream++;
		int Q_table = *stream++;

		stComponent *c = &jdata->m_component_info[cid];
		c->m_vFactor = sampling_factor & 0xf;
		c->m_hFactor = sampling_factor >> 4;
		c->m_qTable = jdata->m_Q_tables[Q_table];
	}
	jdata->m_width = width;
	jdata->m_height = height;

	return 0;
}

/**********************************************************************/

#define BuildQuantizationTable(qtable, ref_table)                       \
{                                                                       \
	for (int i = 0, c = 0; i < 8; i++)                              \
		for (int j = 0; j < 8; j++, c++)                        \
			qtable[c] = ref_table[c];                       \
}

/**********************************************************************/

static int ParseDQT(stJpegData *jdata, const uint8_t *stream)
{
	int length, qi;
	double *table;

	length = BYTE_TO_WORD(stream) - 2;
	stream += 2; // Skip length

	while (length > 0)
	{
		qi = *stream++;

		int qprecision = qi >> 4; // upper 4 bits specify the precision
		int qindex = qi & 0xf; // index is lower 4 bits

		if (qprecision)
		{
			// precision in this case is either 0 or 1 and indicates the precision
			// of the quantized values;
			// 8-bit (baseline) for 0 and  up to 16-bit for 1
			fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
			exit(-1);
		}
		if (qindex >= 4)
		{
			fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
			exit(-1);
		}

		// The quantization table is the next 64 bytes
		table = jdata->m_Q_tables[qindex];

		// the quantization tables are stored in zigzag format, so we
		// use this functino to read them all in and de-zig zag them
		BuildQuantizationTable(table, stream);
		stream += 64;
		length -= 65;
	}
	return 0;
}

/**********************************************************************/

static int ParseSOS(stJpegData *jdata, const uint8_t *stream)
{
	/*
	 * SOS          16              0xffd8                  Start Of Scan
	 * Ls           16              2Ns + 6                 Scan header length
	 * Ns           8               1-4                     Number of image components
	 * Csj          8               0-255                   Scan Component Selector
	 * Tdj          4               0-1                     DC Coding Table Selector
	 * Taj          4               0-1                     AC Coding Table Selector
	 * Ss           8               0                       Start of spectral selection
	 * Se           8               63                      End of spectral selection
	 * Ah           4               0                       Successive Approximation Bit High
	 * Ai           4               0                       Successive Approximation Bit Low
	 */

	uint32_t nr_components = stream[2];

	if (nr_components != 3)
	{
		fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
		exit(-1);
	}

	stream += 3;
	for (uint32_t i = 0; i < nr_components; i++)
	{
		uint32_t cid = *stream++;
		uint32_t table = *stream++;

		if ((table & 0xf) >= 4)
		{
			fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
			exit(-1);
		}
		if ((table >> 4) >= 4)
		{
			fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
			exit(-1);
		}

		jdata->m_component_info[cid].m_acTable = &jdata->m_HTAC[table & 0xf];
		jdata->m_component_info[cid].m_dcTable = &jdata->m_HTDC[table >> 4];
	}
	jdata->m_stream = stream + 3;
	return 0;
}

/**********************************************************************/

static int ParseDHT(stJpegData *jdata, const uint8_t *stream)
{
	/*
	 * u8 0xff
	 * u8 0xc4 (type of segment)
	 * u16 be length of segment
	 * 4-bits class (0 is DC, 1 is AC, more on this later)
	 * 4-bits table id
	 * array of 16 u8 number of elements for each of 16 depths
	 * array of u8 elements, in order of depth
	 */

	uint32_t count, i;
	uint8_t huff_bits[17];
	int length, index;

	length = BYTE_TO_WORD(stream) - 2;
	stream += 2; // Skip length

	while (length > 0)
	{
		index = *stream++;
		// We need to calculate the number of bytes 'vals' will takes
		huff_bits[0] = 0;
		count = 0;
		for (i = 1; i < 17; i++)
		{
			huff_bits[i] = *stream++;
			count += huff_bits[i];
		}
		if (count > 256)
		{
			fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
			exit(-1);
		}
		if ((index & 0xf) >= HUFFMAN_TABLES)
		{
			fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
			exit(-1);
		}
		if (index & 0xf0)
		{
			uint8_t *huffval = jdata->m_HTAC[index & 0xf].m_hufVal;
			for (i = 0; i < count; i++)
				huffval[i] = *stream++;
			BuildHuffmanTable(huff_bits, &jdata->m_HTAC[index & 0xf]); // AC
		}
		else
		{
			uint8_t *huffval = jdata->m_HTDC[index & 0xf].m_hufVal;
			for (i = 0; i < count; i++)
				huffval[i] = *stream++;
			BuildHuffmanTable(huff_bits, &jdata->m_HTDC[index & 0xf]); // DC
		}
		length -= 1;
		length -= 16;
		length -= count;
	}
	return 0;
}

/**********************************************************************/

static int ParseJFIF(stJpegData *jdata, const uint8_t *stream)
{
	int chuck_len;
	int marker;
	int sos_marker_found = 0;
	int dht_marker_found = 0;

	// Parse marker
	while (!sos_marker_found)
	{
		if (*stream++ != 0xff)
			goto bogus_jpeg_format;

		// Skip any padding ff byte (this is normal)
		while (*stream == 0xff)
			stream++;

		marker = *stream++;
		chuck_len = BYTE_TO_WORD(stream);

		switch (marker)
		{
			case SOF:
				if (ParseSOF(jdata, stream) < 0)
					return -1;
				break;

			case DQT:
				if (ParseDQT(jdata, stream) < 0)
					return -1;
				break;

			case SOS:
				if (ParseSOS(jdata, stream) < 0)
					return -1;
				sos_marker_found = 1;
				break;

			case DHT:
				if (ParseDHT(jdata, stream) < 0)
					return -1;
				dht_marker_found = 1;
				break;

				// The reason I added these additional skips here, is because for
				// certain jpg compressions, like swf, it splits the encoding
				// and image data with SOI & EOI extra tags, so we need to skip
				// over them here and decode the whole image
			case SOI:
			case EOI:
				chuck_len = 0;
				break;

			case 0xDD: //DRI: Restart_markers=1;
				jdata->m_restart_interval = BYTE_TO_WORD(stream);
				break;

			case APP0:
				break;

			default:
				break;
		}

		stream += chuck_len;
	}

	if (!dht_marker_found)
	{
		fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
		exit(-1);
	}

	return 0;

bogus_jpeg_format:
	fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
	exit(-1);
	return -1;
}

/**********************************************************************/

static int JpegParseHeader(stJpegData *jdata, const uint8_t *buf)
{
	// Identify the file
	if ((buf[0] != 0xFF) || (buf[1] != SOI))
	{
		fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
		exit(-1);
	}
	const uint8_t *startStream = buf + 2;
	return ParseJFIF(jdata, startStream);
}

/**********************************************************************/

static uint32_t g_reservoir = 0;
static uint32_t g_nbits_in_reservoir = 0;

#define FillNBits(stream, nbits_wanted)                                 \
{                                                                       \
	while (g_nbits_in_reservoir < (unsigned)nbits_wanted)           \
	{                                                               \
		uint8_t c = *(*stream)++;                               \
		g_reservoir <<= 8;                                      \
		if (c == 0xff && **stream == 0x00)                      \
			(*stream)++;                                    \
		g_reservoir |= c;                                       \
		g_nbits_in_reservoir += 8;                              \
	}                                                               \
}

#define shift_bits(nbits_wanted)                                        \
{                                                                       \
	g_nbits_in_reservoir -= nbits_wanted;                           \
	g_reservoir &= (1U << g_nbits_in_reservoir) - 1;                \
}

static inline int16_t GetNBits(const uint8_t **stream, int nbits_wanted)
{
	FillNBits(stream, nbits_wanted);
	int16_t result = g_reservoir >> (g_nbits_in_reservoir - (nbits_wanted));
	shift_bits(nbits_wanted);
	return result;
}

static inline int LookNBits(const uint8_t **stream, int nbits_wanted)
{
	FillNBits(stream, nbits_wanted);
	return g_reservoir >> (g_nbits_in_reservoir - (nbits_wanted));
}

#define SkipNBits(stream, nbits_wanted)                                 \
{                                                                       \
	FillNBits(stream, nbits_wanted);                                \
	shift_bits(nbits_wanted);                                       \
}

/**********************************************************************/

static bool IsInHuffmanCodes(int code, int numCodeBits, int numBlocks, stBlock *blocks, int *outValue)
{
	for (int j = 0; j < numBlocks; j++)
		if (code == blocks[j].code && numCodeBits == blocks[j].length)
		{
			*outValue = blocks[j].value;
			return true; // We've got a match!
		}
	return false;
}

/**********************************************************************/

#define DetermineSign(val, nBits) ((val < (1 << (nBits - 1))) ? val + (-1 << nBits) + 1 : val)

static void ProcessHuffmanDataUnit(stJpegData *jdata, int indx)
{
	stComponent *c = &jdata->m_component_info[indx];

	// Start Huffman decoding
	int16_t DCT_tcoeff[64] = { 0x0 };

	bool found = false;
	int decodedValue = 0;

	// Scan Decode Resync
	if (jdata->m_restart_interval > 0)
		if (jdata->m_stream[0] == 0xff && jdata->m_stream[1] != 0x00)
		{
			// Something might be wrong, we should have had an interval marker set
			jdata->m_stream += 2;
			g_reservoir = 0;
			g_nbits_in_reservoir = 0;

			// The value in the interval marker determines what number it will count
			// upto before looping back...i.e, for an interval marker of 4, we will
			// have 0xFFD0, 0xFFD1, 0xFFD2, 0xFFD3, 0xFFD4, 0xFFD0, 0xFFD1..etc..looping
			// so each time we get a resync, it will count from 0 to 4 then restart :)
		}

	// First thing is get the 1 DC coefficient at the start of our 64 element block
	for (int k = 1; k < 16; k++)
	{
		// Keep grabbing one bit at a time till we find one thats a huffman code
		int code = LookNBits(&jdata->m_stream, k);

		// Check if its one of our huffman codes
		if (IsInHuffmanCodes(code, k, c->m_dcTable->m_numBlocks, c->m_dcTable->m_blocks, &decodedValue))
		{
			// Skip over the rest of the bits now.
			SkipNBits(&jdata->m_stream, k);

			found = true;

			// The decoded value is the number of bits we have to read in next
			int numDataBits = decodedValue;
			// We know the next k bits are for the actual data
			if (numDataBits == 0)
				DCT_tcoeff[0] = c->m_previousDC;
			else
			{
				if (jdata->m_restart_interval > 0)
					if (jdata->m_stream[0] == 0xff && jdata->m_stream[1] != 0x00)
					{
						jdata->m_stream += 2;
						g_reservoir = 0;
						g_nbits_in_reservoir = 0;
					}

				int16_t data = GetNBits(&jdata->m_stream, numDataBits);
				data = DetermineSign(data, numDataBits);
				DCT_tcoeff[0] = data + c->m_previousDC;
				c->m_previousDC = DCT_tcoeff[0];
			}
			// Found so we can exit out
			break;
		}
	}

	if (!found)
	{
		fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
		exit(-1);
	}

	// Second, the 63 AC coefficient
	int nr = 1;
	bool EOB_found = false;
	while ((nr <= 63) && (!EOB_found))
	{
		int k = 0;
		for (k = 1; k <= 16; k++)
		{
			// Keep grabbing one bit at a time till we find one thats a huffman code
			int code = LookNBits(&jdata->m_stream, k);

			// Check if its one of our huffman codes
			if (IsInHuffmanCodes(code, k, c->m_acTable->m_numBlocks, c->m_acTable->m_blocks, &decodedValue))
			{
				// Skip over k bits, since we found the huffman value
				SkipNBits(&jdata->m_stream, k);

				// Our decoded value is broken down into 2 parts, repeating RLE, and then
				// the number of bits that make up the actual value next
				int valCode = decodedValue;

				uint8_t size_val = valCode & 0xF; // Number of bits for our data
				uint8_t count_0 = valCode >> 4; // Number RunLengthZeros

				if (size_val == 0)
				{ // RLE
					if (count_0 == 0)
						EOB_found = true; // EOB found, go out
					else if (count_0 == 0xF)
						nr += 16; // skip 16 zeros
				}
				else
				{
					nr += count_0; //skip count_0 zeroes
					if (nr > 63)
					{
						fprintf(stderr, "Fail @ %s:%d\n", __FILE__, __LINE__);
						exit(-1);
					}

					int16_t data = GetNBits(&jdata->m_stream, size_val);
					data = DetermineSign(data, size_val);
					DCT_tcoeff[nr++] = data;
				}
				break;
			}
		}

		if (k > 16)
			nr++;
	}

	// We've decoded a block of data, so copy it across to our buffer
	for (int j = 0; j < 64; j++)
		c->m_DCT[j] = DCT_tcoeff[j];
}

/**********************************************************************/

static void ConvertYCrCbtoRGB(int y, int cb, int cr, int *r, int *g, int *b)
{
	*r = byte_limit((int)(y + 1.402   * (cb - 128)), 0);
	*g = byte_limit((int)(y - 0.34414 * (cr - 128) - 0.71414 * (cb - 128)), 0);
	*b = byte_limit((int)(y + 1.772   * (cr - 128)), 0);
}

/**********************************************************************/

static void YCrCB_to_RGB24_Block8x8(stJpegData *jdata, int w, int h, int imgx, int imgy, int imgw, int imgh)
{
	const uint8_t *Y  = jdata->m_Y;
	const uint8_t *Cb = jdata->m_Cb;
	const uint8_t *Cr = jdata->m_Cr;

	for (int y = 0; y < (8 * h); y++)
		for (int x = 0; x < (8 * w); x++)
		{
			if ((x + imgx >= imgw) || (y + imgy >= imgh))
				continue;

			int poff = x * 3 + jdata->m_width * 3 * y;
			uint8_t *pix = &(jdata->m_colourspace[poff]);

			int yoff = x + y * (w << 3);
			int coff = (int)(x * (1.0 / w)) + ((int)(y * (1.0 / h)) << 3);

			int r, g, b;
			ConvertYCrCbtoRGB(Y[yoff], Cr[coff], Cb[coff], &r, &g, &b);

			pix[0] = byte_limit(r, 0);
			pix[1] = byte_limit(g, 0);
			pix[2] = byte_limit(b, 0);
		}
}

/**********************************************************************/
//
//  Decoding
//  .-------.
//  | 1 | 2 |
//  |---+---|
//  | 3 | 4 |
//  `-------'
//
/**********************************************************************/
static void DecodeMCU(stJpegData *jdata, int w, int h)
{
	// Y
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
		{
			int stride = w << 3;
			int offset = (x << 3) + (y * w << 6);
			ProcessHuffmanDataUnit(jdata, cY);
			DecodeSingleBlock(&jdata->m_component_info[cY], &jdata->m_Y[offset], stride);
		}

	// Cb
	ProcessHuffmanDataUnit(jdata, cCb);
	DecodeSingleBlock(&jdata->m_component_info[cCb], jdata->m_Cb, 8);

	// Cr
	ProcessHuffmanDataUnit(jdata, cCr);
	DecodeSingleBlock(&jdata->m_component_info[cCr], jdata->m_Cr, 8);
}

/**********************************************************************/

static int JpegDecode(stJpegData *jdata)
{
	g_reservoir = 0;
	g_nbits_in_reservoir = 0;

	int hFactor = jdata->m_component_info[cY].m_hFactor;
	int vFactor = jdata->m_component_info[cY].m_vFactor;

	// RGB24:
	if (jdata->m_rgb == NULL)
	{
		int h = jdata->m_height * 3;
		int w = jdata->m_width * 3;
		int height = h + (hFactor << 3) - (h % (hFactor << 3));
		int width = w + (vFactor << 3) - (w % (vFactor << 3));
		jdata->m_rgb = calloc(width * height, sizeof (uint8_t));
	}

	jdata->m_component_info[0].m_previousDC = 0;
	jdata->m_component_info[1].m_previousDC = 0;
	jdata->m_component_info[2].m_previousDC = 0;
	jdata->m_component_info[3].m_previousDC = 0;

	int xstride_by_mcu = hFactor << 3;
	int ystride_by_mcu = vFactor << 3;

	// Just the decode the image by 'macroblock' (size is 8x8, 8x16, or 16x16)
	for (int y = 0; y < (int)jdata->m_height; y += ystride_by_mcu)
	{
		for (int x = 0; x < (int)jdata->m_width; x += xstride_by_mcu)
		{
			jdata->m_colourspace = jdata->m_rgb + x * 3 + (y * jdata->m_width * 3);
			// Decode MCU Plane
			DecodeMCU(jdata, hFactor, vFactor);
			YCrCB_to_RGB24_Block8x8(jdata, hFactor, vFactor, x, y, jdata->m_width, jdata->m_height);
		}
	}

	return 0;
}

/**********************************************************************/
//
// Take Jpg data, i.e. jpg file read into memory, and decompress it to an
// array of rgb pixel values.
//
// Note - Memory is allocated for this function, so delete it when finished
//
/**********************************************************************/


extern bool jpeg_decode_data(FILE *file, jpeg_message_t *msg, jpeg_image_t *info, jpeg_load_e a, bool fill)
{
	message = msg;
	action = a;

	fseek(file, 0, SEEK_END);
	int64_t length = ftell(file);
	fseek(file, 0, SEEK_SET);
	uint8_t *buf = malloc(length);
	fread(buf, length, 1, file);

	// Allocate memory for our decoded jpg structure, all our data will be
	// decompressed and stored in here for the various stages of our jpeg decoding
	stJpegData jdec;
	memset(&jdec, 0x00, sizeof jdec);

	// Start Parsing.....reading & storing data
	if (JpegParseHeader(&jdec, buf) < 0)
	{
		free(buf);
		return false;
	}

	if (fill)
		fill_size = jdec.m_width * jdec.m_height * 3;

	// We've read it all in, now start using it, to decompress and create rgb values
	JpegDecode(&jdec);

	// Get the size of the image
	info->width = jdec.m_width;
	info->height = jdec.m_height;

	info->rgb = calloc(info->height, sizeof (uint8_t *));
	for (uint32_t i = 0; i < info->height; i++)
	{
		info->rgb[i] = calloc(info->width, 3);
		memcpy(info->rgb[i], jdec.m_rgb + i * info->width * 3, info->width * 3);
	}

	// Release the memory for our jpeg decoder structure jdec
	free(jdec.m_rgb);
	free(buf);

	return true;
}
