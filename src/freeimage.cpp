////////////////////////////////////////////////////////////////
//
// RwgTex / FreeImage helpe functions
// (c) Pavel [VorteX] Timofeyev
// See LICENSE text file for a license agreement
//
////////////////////////////////

#include "main.h"
#include "freeimage.h"
#include "scale2x.h"

size_t fiGetSize(FIBITMAP *bitmap)
{
	size_t size;

	if (!bitmap)
		return 0;
	size = sizeof(FIBITMAP);
	if (FreeImage_HasPixels(bitmap))
		size += FreeImage_GetWidth(bitmap)*FreeImage_GetHeight(bitmap)*FreeImage_GetBPP(bitmap)/8;
	return size;
}

/*
==========================================================================================

  ICC PROFILE ROUTINES

==========================================================================================
*/

typedef struct ICCHeader_s
{
	unsigned int size;                   // 0-3
	unsigned int CMMType;                // 4-7
	unsigned int profileVersion;         // 8-11
	unsigned int profileDeviceClass;     // 12-15
	unsigned char colorSpaceData[8];     // 16-19
	unsigned int profileConnectionSpace; // 20-23
	unsigned char dateTime[12];          // 24-35
	unsigned char magicWord[4];          // 36-39
	unsigned int primaryPlatformTarget;  // 40-43

}ICCHeader_t;

/*
43-47 Flags to indicate various options for the
CMM such as distributed processing and
caching options
see below
48-51 Device manufacturer of the device for which
this proﬁle is created
see below
52-55 Device model of the device for which this
proﬁle is created
see below
56-63 Device attributes unique to the particular
device setup such as media type
see below
64-67 Rendering Intent see below
68-79 The XYZ values of the illuminant of the pro-
ﬁle connection space. This must correspond
to D50. It is explained in more detail in
Annex A.1 'Proﬁle Connection Spaces'.
XYZNumber
80-83 Identiﬁes the creator of the proﬁle see be
*/

/*
==========================================================================================

  FREE HELP ROUTINES

==========================================================================================
*/

// get bitmap data
byte *fiGetData(FIBITMAP *bitmap, int *pitch)
{
	if (pitch)
		*pitch = FreeImage_GetPitch(bitmap);
	return FreeImage_GetBits(bitmap);
}

// copy out image pixel data, solves alignment
byte *fiGetUnalignedData(FIBITMAP *bitmap, bool *data_allocated, bool force_allocate)
{
	int w, h, bpp, pitch, y;
	byte *data, *in, *dataptr;

	w = FreeImage_GetWidth(bitmap);
	h = FreeImage_GetHeight(bitmap);
	bpp = FreeImage_GetBPP(bitmap) / 8;
	data = fiGetData(bitmap, &pitch);
	if (data_allocated != NULL)
		*data_allocated = false;

	// easy case - data is already properly aligned
	if (w*bpp == pitch && force_allocate == false)
		return data;

	// convert aligned lines to single chunk
	dataptr = (byte *)mem_alloc(w * h * bpp);
	in = dataptr;
	for (y = 0; y < h; y++)
	{
		memcpy(in, data, w * bpp);
		in += w * bpp;
		data += pitch; 
	}
	if (data_allocated != NULL)
		*data_allocated = true;
	return dataptr;
}

// copy unalighed data back
void fiStoreUnalignedData(FIBITMAP *bitmap, byte *dataptr, int width, int height, int bpp)
{
	int pitch, y;
	byte *data;

	data = fiGetData(bitmap, &pitch);
	if (dataptr == data)
		return; // easy case
	for (y = 0; y < height; y++)
	{
		memcpy(data, dataptr, width*bpp);
		dataptr += width*bpp;
		data += pitch;
	}
}

// free allocated data got on fiGetDataUnaligned
void fiFreeUnalignedData(byte *dataptr, bool data_allocated)
{
	if (data_allocated)
		mem_free(dataptr);
}

// remove bitmap and set pointer
FIBITMAP *_fiFree(FIBITMAP *bitmap, char *file, int line)
{
	if (bitmap)
		if (_mem_sentinel_free("fiFree", bitmap, file, line))
			FreeImage_Unload(bitmap);
	return NULL;
}

// create empty bitmap
FIBITMAP *fiCreate(int width, int height, int bpp, char *sentinelName)
{
	FIBITMAP *bitmap;

	bitmap = FreeImage_Allocate(width, height, bpp * 8, 0x00FF0000, 0x0000FF00, 0x000000FF);
	if (!bitmap)
		Error("fiCreate: failed to allocate new bitmap (%ix%i bpp %i)", width, height, bpp);
	//if (width*bpp != FreeImage_GetPitch(bitmap))
	//	Error("fiCreate: failed to allocate new bitmap (%ix%i bpp %i) - width*bpp not matching pitch, seems memory is aligned!", width, height, bpp);
	mem_sentinel(sentinelName, bitmap, fiGetSize(bitmap));
	return bitmap;
}

// clone bitmap
FIBITMAP *fiClone(FIBITMAP *bitmap)
{
	if (!bitmap)
		return NULL;
	FIBITMAP *cloned = FreeImage_Clone(bitmap);
	mem_sentinel("fiClone", cloned, fiGetSize(cloned));
	return cloned;
}

// rescale bitmap
FIBITMAP *fiRescale(FIBITMAP *bitmap, int width, int height, FREE_IMAGE_FILTER filter, bool removeSource)
{
	FIBITMAP *scaled;

	scaled = FreeImage_Rescale(bitmap, width, height, filter);
	if (!scaled)
		Error("fiRescale: failed to rescale bitmap to %ix%i", width, height);
	if (removeSource)
		fiFree(bitmap);
	mem_sentinel("fiRescale", scaled, fiGetSize(scaled));
	return scaled;
}

// rescale bitmap with nearest neighbor
// picked from AForge Image Processing Library
// Copyright © Andrew Kirillov, 2005-2007
// andrew.kirillov@gmail.com
FIBITMAP *fiRescaleNearestNeighbor(FIBITMAP* bitmap, int new_width, int new_height, bool removeSource)
{
	// get source image size
	unsigned width = FreeImage_GetWidth(bitmap);
	unsigned height = FreeImage_GetHeight(bitmap);
	unsigned src_pitch = FreeImage_GetPitch(bitmap);
	unsigned bpp = FreeImage_GetBPP(bitmap);
	unsigned btpp = bpp / 8;
	
	FIBITMAP *scaled = fiCreate(new_width, new_height, btpp, "fiRescaleNearestNeighbor");
	if (bpp == 8)
	{
		if(FreeImage_GetColorType(bitmap) == FIC_MINISWHITE) 
		{
			// build an inverted greyscale palette
			RGBQUAD *dst_pal = FreeImage_GetPalette(scaled);
			for(int i = 0; i < 256; i++) {
				dst_pal[i].rgbRed = dst_pal[i].rgbGreen =
					dst_pal[i].rgbBlue = (BYTE)(255 - i);
			}
		} 
		else 
		{
			// build a greyscale palette
			RGBQUAD *dst_pal = FreeImage_GetPalette(scaled);
			for(int i = 0; i < 256; i++) {
				dst_pal[i].rgbRed = dst_pal[i].rgbGreen =
					dst_pal[i].rgbBlue = (BYTE)i;
			}
		}
	}
	
	unsigned dst_pitch = FreeImage_GetPitch(scaled);
	BYTE* src_bits = (BYTE*)FreeImage_GetBits(bitmap); // The image raster
	BYTE* dst_bits = (BYTE*)FreeImage_GetBits(scaled); // The image raster
	BYTE* lines, *lined;

	unsigned d;
	double xFactor = (double) width / new_width;
	double yFactor = (double) height / new_height;
	
    // cooridinates of nearest point
    int ox, oy;
	
	// for each line
	for ( int y = 0; y < new_height; y++ )
	{
        // Y coordinate of the nearest point
        oy = (int) ( y * yFactor );
		lined = dst_bits + y * dst_pitch;
		lines = src_bits + oy * src_pitch;
		// for each pixel
		for ( int x = 0; x < new_width; x++ )
		{
			// X coordinate of the nearest point
			ox = (int) ( x * xFactor );			
			for (d = 0; d < btpp; d++)
				lined[x * btpp + d] = lines[ox * btpp + d];
		}
	}
	
	// Copying the DPI...
	FreeImage_SetDotsPerMeterX(scaled, FreeImage_GetDotsPerMeterX(bitmap));
	FreeImage_SetDotsPerMeterY(scaled, FreeImage_GetDotsPerMeterY(bitmap));
	if (removeSource)
		fiFree(bitmap);
	return scaled;
}


// check if ICC profile sets sRGB colorspace
bool ICCProfile_Test_sRGB(void *profile_data, int datasize)
{
	int i, numTags, tag_ofs, tag_size;
	unsigned char *icc, *tag, *icc_end;
	char tagdata[256];

	if (!profile_data)
		return false; // not loaded
	icc = (byte *)profile_data;
	if (icc[36] != 'a' || icc[37] != 'c' || icc[38] != 's' || icc[39] != 'p')
		return false; // not an ICC file
	icc_end = icc + datasize;
	numTags = icc[128+0]*0x1000000 + icc[128+1]*0x10000 + icc[128+2]*0x100 + icc[128+3];
	// search for 'desc' tag
	for (i = 0; i < numTags; i++)
	{
		tag = icc + 128 + 4 + i*12;
		if (tag > icc_end)
			return false; // invalid ICC file
		// check for a desc flag
		if (!memcmp(tag, "desc", 4))
		{
			tag_ofs = tag[4]*0x1000000 + tag[5]*0x10000 + tag[6]*0x100 + tag[7];
			tag_size = tag[8]*0x1000000 + tag[9]*0x10000 + tag[10]*0x100 + tag[11];
			if (tag_ofs + tag_size > datasize)
				return false; // invalid ICC file
			strncpy(tagdata, (char *)(icc+tag_ofs+12), min(255, tag_size-12));
			if (!strcmp(tagdata, "sRGB IEC61966-2.1") || !strcmp(tagdata, "sRGB IEC61966-2-1") || !strcmp(tagdata, "sRGB IEC61966") || !strcmp(tagdata, "* wsRGB"))
				return true;
			return false;
		}
	}
	return false;
}

// bind bitmap to image
bool fiBindToImage(FIBITMAP *bitmap, LoadedImage *image, FREE_IMAGE_FORMAT format, bool keep_color_profile)
{
	FIICCPROFILE *icc_profile;

	if (keep_color_profile == true)
	{
		icc_profile = NULL;
		if (image->bitmap != NULL)
			icc_profile = FreeImage_GetICCProfile(bitmap);
		if (icc_profile != NULL)
			FreeImage_CreateICCProfile(bitmap, icc_profile->data, icc_profile->size);
	}	
	image->bitmap = fiFree(image->bitmap);
	image->width = 0;
	image->height = 0;
	image->colorSwap = false;

	if (!bitmap)
		return false;
	if (!FreeImage_HasPixels(bitmap))
	{
		fiFree(bitmap);
		return false;
	}

	// read ICC profile to determine whether image is using sRGB color space
	if (keep_color_profile == false)
	{
		icc_profile = FreeImage_GetICCProfile(bitmap);
		if (icc_profile != NULL)
			image->sRGB = (icc_profile != NULL) ? ICCProfile_Test_sRGB(icc_profile->data, icc_profile->size) : false;
		// debug code - save icc profile
		//FILE *f = fopen("profile.ICC", "wb");
		//fwrite(icc_profile->data, icc_profile->size, 1, f);
		//fclose(f);
	}

	// fill image
	//if (FreeImage_GetWidth(image->bitmap)*FreeImage_GetBPP(image->bitmap) / 8 != FreeImage_GetPitch(bitmap))
	//	Error("fiBindToImage: failed to bind bitmap (%ix%i bpp %i) - width*bpp not matching pitch, seems memory is aligned!", (int)FreeImage_GetWidth(bitmap), (int)FreeImage_GetHeight(bitmap), (int)FreeImage_GetBPP(bitmap) / 8);
	image->colorSwap = true;
	if (format == FIF_TARGA || format == FIF_PNG || format == FIF_BMP || format == FIF_JPEG)
		FreeImage_FlipVertical(bitmap);
	image->bitmap = bitmap;
	image->width = FreeImage_GetWidth(image->bitmap);
	image->height = FreeImage_GetHeight(image->bitmap);
	return true;
}

// load bitmap from memory
bool fiLoadData(FREE_IMAGE_FORMAT format, FS_File *file, byte *data, size_t datasize, LoadedImage *image)
{
	FIMEMORY *memory;

	// get format
	if (format == FIF_UNKNOWN)
		format = FreeImage_GetFIFFromFilename(file->ext.c_str());
	if (format == FIF_UNKNOWN)
	{
		Warning("%s%s.%s : FreeImage unabled to load file (unknown format)\n", file->path.c_str(), file->name.c_str(), file->ext.c_str());
		return NULL;
	}
	if (!FreeImage_FIFSupportsReading(format))
	{
		Warning("%s%s.%s : FreeImage is not supporting loading of this format (%i)\n", file->path.c_str(), file->name.c_str(), file->ext.c_str(), format);
		return NULL;
	}

	// load from memory
	memory = FreeImage_OpenMemory(data, datasize);
	FIBITMAP *bitmap = FreeImage_LoadFromMemory(format, memory, 0);
	FreeImage_CloseMemory(memory);
	mem_sentinel("fiLoadData", bitmap, fiGetSize(bitmap));
	return fiBindToImage(bitmap, image, format, false);
}

// load bitmap from raw data
bool fiLoadDataRaw(int width, int height, int bpp, byte *data, size_t datasize, byte *palette, bool dataIsBGR, LoadedImage *image)
{
	FIBITMAP *bitmap;
	int pitch, y;
	byte *bits;

	if (width < 1 || height < 1 || !data)
		return NULL;
	// create image
	bitmap = fiCreate(width, height, bpp, "fiLoadDataRaw");
	if (!bitmap)
		return NULL;
	// fill pixels
	bits = fiGetData(bitmap, &pitch);
	if ((size_t)(pitch*height) > datasize)
	{
		fiFree(bitmap);
		Warning("fiLoadDataRaw : failed to read stream (unexpected end of data)\n");
		return NULL;
	}
	// fill colors
	memcpy(bits, data, pitch*height);
	// swap colors if needed (FreeImage loads as BGR)
	if (!dataIsBGR)
	{
		if (bpp == 3 || bpp == 4)
		{
			byte *in = data;
			for (y = 0; y < height; y++)
			{
				byte *end = in + width*bpp;
				while(data < end)
				{
					bits[0] = data[2];
					bits[2] = data[0];
					data += bpp;
					bits += bpp;
				}
				in += pitch;
			}
		}
		else
			Error("fiLoadDataRaw: bpp %i not supporting color swap", bpp);
	}
	// fill colormap
	// FreeImage loads as BGR
	if (bpp == 1 && palette)
	{
		RGBQUAD *pal = FreeImage_GetPalette(bitmap);
		if (pal)
		{
			int i;
			if (dataIsBGR)
			{
				// load standart
				for (i = 0; i < 256; i++)
				{
					pal[i].rgbRed = palette[i*3 + 0];
					pal[i].rgbGreen = palette[i*3 + 1];
					pal[i].rgbBlue = palette[i*3 + 2];
				}
			}
			else
			{
				// load swapped
				for (i = 0; i < 256; i++)
				{
					pal[i].rgbRed = palette[i*3 + 2];
					pal[i].rgbGreen = palette[i*3 + 1];
					pal[i].rgbBlue = palette[i*3 + 0];
				}
			}
		}
	}
	return fiBindToImage(bitmap, image);
}

// load bitmap from file
bool fiLoadFile(FREE_IMAGE_FORMAT format, const char *filename, LoadedImage *image)
{
	if (format == FIF_UNKNOWN)
		format = FreeImage_GetFIFFromFilename(filename);
	if (format == FIF_UNKNOWN)
	{
		Warning("%s : FreeImage unabled to load file (unknown format)\n", filename);
		return NULL;
	}
	if (!FreeImage_FIFSupportsReading(format))
	{
		Warning("%s : FreeImage is not supporting loading of this format (%i)\n", filename, format);
		return NULL;
	}
	FIBITMAP *bitmap = FreeImage_Load(format, filename, 0);
	mem_sentinel("fiLoadFile", bitmap, fiGetSize(bitmap));
	return fiBindToImage(bitmap, image, format, false);
}

// save bitmap to file
bool fiSave(FIBITMAP *bitmap, FREE_IMAGE_FORMAT format, const char *filename)
{
	if (format == FIF_UNKNOWN)
		format = FreeImage_GetFIFFromFilename(filename);
	if (format == FIF_UNKNOWN)
	{
		Warning("%s : FreeImage unabled to save file (unknown format)\n", filename);
		return false;
	}
	if (!FreeImage_FIFSupportsWriting(format))
	{
		Warning("%s : FreeImage is not supporting riting of this format (%i)\n", filename, format);
		return false;
	}
	return FreeImage_Save(format, bitmap, filename) ? true : false;
}

/*
==========================================================================================

  COMBINE

==========================================================================================
*/

// combine two bitmaps
void fiCombine(FIBITMAP *source, FIBITMAP *combine, FREE_IMAGE_COMBINE mode, float blend, bool destroyCombine)
{
	int inpitch, y, outpitch;

	if (!source || !combine)
		return;

	// check
	int cbpp = FreeImage_GetBPP(combine)/8;
	int sbpp = FreeImage_GetBPP(source)/8;
	if (cbpp != 1 && cbpp != 3 && cbpp != 4)
		Error("fiCombine: combined bitmap should be 8, 24 or 32-bit");
	if (sbpp != 1 && sbpp != 3 && sbpp != 4)
		Error("fiCombine: source bitmap should be 8, 24 or 32-bit");
	if (FreeImage_GetWidth(source) != FreeImage_GetWidth(combine) || FreeImage_GetHeight(source) != FreeImage_GetHeight(combine))
		Error("fiCombine: source and blend bitmaps having different width/height/BPP");

	// combine
	float rb = 1 - blend;
	int w = FreeImage_GetWidth(source);
	int h = FreeImage_GetHeight(source);
	byte *in_data = fiGetData(combine, &inpitch);
	byte *out_data = fiGetData(source, &outpitch);
	for (y = 0; y < h; y++)
	{
		byte *in = in_data;
		byte *end = in_data + w*cbpp;
		byte *out = out_data;
		switch(mode)
		{
			case COMBINE_RGB:
				if (sbpp != 3 && sbpp != 4)
					Warning("fiCombine(COMBINE_RGB): source bitmap should be RGB or RGBA");
				else if (cbpp != 3 && cbpp != 4)
					Warning("fiCombine(COMBINE_RGB): combined bitmap should be RGB or RGBA");
				else
				{
					while(in < end)
					{
						out[0] = (byte)floor(out[0]*rb + in[0]*blend + 0.5);
						out[1] = (byte)floor(out[1]*rb + in[1]*blend + 0.5);
						out[2] = (byte)floor(out[2]*rb + in[2]*blend + 0.5);
						out += sbpp;
						in  += cbpp;
					}
				}
				break;
			case COMBINE_ALPHA:
				if (sbpp != 4)
					Warning("fiCombine(COMBINE_ALPHA): source bitmap should be RGBA");
				else if (cbpp != 4)
					Warning("fiCombine(COMBINE_ALPHA): combined bitmap should be RGBA");
				else
				{
					while(in < end)
					{
						out[3] = (byte)floor(out[3]*rb + in[3]*blend + 0.5);
						out += sbpp;
						in  += cbpp;
					}
				}
				break;
			case COMBINE_R_TO_ALPHA:
				if (sbpp != 4)
					Warning("fiCombine(COMBINE_R_TO_ALPHA): source bitmap should be RGBA");
				else
				{
					while(in < end)
					{
						out[3] = (byte)floor(out[3]*rb + in[0]*blend + 0.5);
						out += sbpp;
						in  += cbpp;
					}
				}
				break;
			case COMBINE_ALPHA_TO_RGB:
				if (sbpp != 3 && sbpp != 4)
					Warning("fiCombine(COMBINE_ALPHA_TO_RGB): source bitmap should be RGB or RGBA");
				else if (cbpp != 4)
					Warning("fiCombine(COMBINE_ALPHA_TO_RGB): combined bitmap should be RGBA");
				else
				{
					while(in < end)
					{
						out[0] = (byte)floor(out[0]*rb + in[3]*blend + 0.5);
						out[1] = (byte)floor(out[1]*rb + in[3]*blend + 0.5);
						out[2] = (byte)floor(out[2]*rb + in[3]*blend + 0.5);
						out[3] = 255;
						out += sbpp;
						in  += cbpp;
					}
				}
				break;
			case COMBINE_ADD:
				if (sbpp != 3 && sbpp != 4)
					Warning("fiCombine(COMBINE_ADD): source bitmap should be RGB or RGBA");
				else if (cbpp != 3 && cbpp != 4)
					Warning("fiCombine(COMBINE_ADD): combined bitmap should be RGB or RGBA");
				else
				{
					while(in < end)
					{
						out[0] = (byte)max(0, min(floor(out[0] + in[0]*blend), 255));
						out[1] = (byte)max(0, min(floor(out[1] + in[1]*blend), 255));
						out[2] = (byte)max(0, min(floor(out[2] + in[2]*blend), 255));
						out += sbpp;
						in  += cbpp;
					}
				}
				break;
			case COMBINE_MIN:
				if (sbpp != 3 && sbpp != 4)
					Warning("fiCombine(COMBINE_MIN): source bitmap should be RGB or RGBA");
				else if (cbpp != 3 && cbpp != 4)
					Warning("fiCombine(COMBINE_MIN): combined bitmap should be RGB or RGBA");
				else
				{
					while(in < end)
					{
						out[0] = min(out[0], in[0]);
						out[1] = min(out[1], in[1]);
						out[2] = min(out[2], in[2]);
						out += sbpp;
						in  += cbpp;
					}
				}
				break;
			case COMBINE_MAX:
				if (sbpp != 3 && sbpp != 4)
					Warning("fiCombine(COMBINE_MAX): source bitmap should be RGB or RGBA");
				else if (cbpp != 3 && cbpp != 4)
					Warning("fiCombine(COMBINE_MAX): combined bitmap should be RGB or RGBA");
				else
				{
					while(in < end)
					{
						out[0] = max(out[0], in[0]);
						out[1] = max(out[1], in[1]);
						out[2] = max(out[2], in[2]);
						out += sbpp;
						in  += cbpp;
					}
				}
				break;
			default:
				Warning("fiCombine: bad mode");
				break;
		}
		in_data += inpitch;
		out_data += outpitch;
	}
	if (destroyCombine)
		fiFree(combine);
}

// converts image to requested BPP
FIBITMAP *fiConvertBPP(FIBITMAP *bitmap, int want_bpp, int want_palette_size, byte *want_external_palette)
{
	FIBITMAP *converted;

	// check if needs conversion
	int bpp = FreeImage_GetBPP(bitmap) / 8;
	if (bpp == want_bpp)
		return bitmap;

	// convert
	if (want_bpp == 1)
	{
		if (want_external_palette != NULL || want_palette_size)
		{
			if (bpp != 3) // color quantize only acepts 24bit images
				bitmap = fiConvertBPP(bitmap, 3, 0, NULL);
			if (want_external_palette)
				converted = FreeImage_ColorQuantizeEx(bitmap, FIQ_WUQUANT, want_palette_size, want_palette_size, (RGBQUAD *)want_external_palette);
			else
				converted = FreeImage_ColorQuantizeEx(bitmap, FIQ_WUQUANT, want_palette_size, 0, NULL);
		}
		else
			converted = FreeImage_ConvertToGreyscale(bitmap);
	}
	else if (want_bpp == 3)
		converted = FreeImage_ConvertTo24Bits(bitmap);
	else if (want_bpp == 4)
		converted = FreeImage_ConvertTo32Bits(bitmap);
	else
		Error("fiConvertBPP: bad bpp %i", bpp);
	if (!converted)
	{
		Warning("fiConvertBPP: conversion failed");
		return bitmap;
	}
	if (converted == bitmap)
		return bitmap;
	fiFree(bitmap);
	mem_sentinel("fiConvertBPP", converted, fiGetSize(converted));
	return converted;
}

// get image palette
bool fiGetPalette(FIBITMAP *bitmap, byte *palette, int palettesize)
{
	RGBQUAD *pal;

	pal = FreeImage_GetPalette(bitmap);
	if (pal)
	{
		memcpy(palette, pal, 4*palettesize);
		return true;
	}
	return false;
}

// converts image to requested type
FIBITMAP *fiConvertType(FIBITMAP *bitmap, FREE_IMAGE_TYPE want_type)
{
	// check if needs conversion
	FREE_IMAGE_TYPE type = FreeImage_GetImageType(bitmap);
	if (type == want_type)
		return bitmap;

	// convert
	FIBITMAP *converted = FreeImage_ConvertToType(bitmap, want_type);
	if (!converted)
	{
		Warning("fiConvertType: conversion failed");
		return bitmap;
	}
	if (converted == bitmap)
		return bitmap;
	fiFree(bitmap);
	mem_sentinel("fiConvertType", converted, fiGetSize(converted));
	return converted;
}

/*
==========================================================================================
 
 SCALE2x

==========================================================================================
*/

// scale bitmap with scale2x
FIBITMAP *fiScale2x(byte *data, int pitch, int width, int height, int bpp, int scaler, bool freeData)
{
	byte *scaled_data;
	int scaled_pitch;

	if (sxCheck(scaler, bpp, width, height) != SCALEX_OK)
		return NULL;

	FIBITMAP *scaled = fiCreate(width*scaler, height*scaler, bpp, "fiScale2x");
	scaled_data = fiGetData(scaled, &scaled_pitch);
	sxScale(scaler, scaled_data, scaled_pitch, data, pitch, bpp, width, height);
	if (freeData)
		mem_free(data);

	return scaled;
}

FIBITMAP *fiScale2x(FIBITMAP *bitmap, int scaler, bool freeSource)
{
	byte *data;
	int pitch;

	data = fiGetData(bitmap, &pitch);
	FIBITMAP *scaled = fiScale2x(data, pitch, FreeImage_GetWidth(bitmap), FreeImage_GetHeight(bitmap), FreeImage_GetBPP(bitmap)/8, scaler, false);
	if (!scaled)
		return fiClone(bitmap);
	if (freeSource)
		fiFree(bitmap);
	return scaled;
}

/*
==========================================================================================

  BLUR AND SHARPEN

  picked from Developer's ImageLib

==========================================================================================
*/

// apply a custom filter matrix to bitmap
FIBITMAP *fiFilter(FIBITMAP *bitmap, double *m, double scale, double bias, int iteractions, bool removeSource)
{
    int	i, t, x, y, c, lastx, lasty, ofs[9];
	FIBITMAP *filtered, *filtered2, *temp;
	double n;

	if (!bitmap)
		return NULL;

	int width = FreeImage_GetWidth(bitmap);
	int height = FreeImage_GetHeight(bitmap);
	int bpp  = FreeImage_GetBPP(bitmap) / 8;
	int pitch = width * bpp;

	// check bitmap type
	if (bpp != 1 && bpp != 3 && bpp != 4)
	{
		Warning("fiFilter: only supported for 8, 24 or 32-bit bitmaps");
		return fiClone(bitmap);
	}

	// many iteractinos requires 2 buffers
	filtered = fiClone(bitmap);
	if (iteractions > 1)
	{
		if (removeSource)
			filtered2 = bitmap;
		else
			filtered2 = fiClone(bitmap);
	}
	
	// do the job
	bool in_allocated, out_allocated, out2_allocated;
	byte *in_data = fiGetUnalignedData(bitmap, &in_allocated, false);
	byte *out_data = fiGetUnalignedData(filtered, &out_allocated, false);
	byte *out2_data = fiGetUnalignedData(filtered2, &out2_allocated, false);
	byte *in = in_data;
	byte *out = out_data;
	byte *out2 = out2_data;
	for ( ; iteractions > 0; iteractions--)
	{
		lastx = width  - 1;
		lasty = height - 1;
		for (y = 1; y < lasty; y++)
		{

			for (x = 1; x < lastx; x++)
			{
				ofs[4] = ((y  ) * width + (x  )) * bpp;
				ofs[0] = ((y-1) * width + (x-1)) * bpp;
				ofs[1] = ((y-1) * width + (x  )) * bpp;
				ofs[2] = ((y-1) * width + (x+1)) * bpp;
				ofs[3] = ((y  ) * width + (x-1)) * bpp;
				ofs[5] = ((y  ) * width + (x+1)) * bpp;
				ofs[6] = ((y+1) * width + (x-1)) * bpp;
				ofs[7] = ((y+1) * width + (x  )) * bpp;
				ofs[8] = ((y+1) * width + (x-1)) * bpp;
				// sample 0 channel
				n = in[ofs[0]]*m[0] + in[ofs[1]]*m[1] + in[ofs[2]]*m[2] + in[ofs[3]]*m[3] + in[ofs[4]]*m[4] + in[ofs[5]]*m[5] + in[ofs[6]]*m[6] + in[ofs[7]]*m[7] + in[ofs[8]]*m[8];
				t = (unsigned int)fabs((n/scale) + bias);
				out[ofs[4]] = min(t, 255);
				// sample rest channels
				for (c = 1; c < bpp; c++)
				{
					n = in[ofs[0]+c]*m[0] + in[ofs[1]+c]*m[1] + in[ofs[2]+c]*m[2] + in[ofs[3]+c]*m[3] + in[ofs[4]+c]*m[4] + in[ofs[5]+c]*m[5] + in[ofs[6]+c]*m[6] + in[ofs[7]+c]*m[7] + in[ofs[8]+c]*m[8];
					t = (unsigned int)fabs(n/scale + bias);
					out[ofs[4]+c] = min(t, 255);
				}
			}
		}

		// copy 4 corners
		for (c = 0; c < bpp; c++)
		{
			out[c] = in[c];
			out[pitch - bpp + c] = in[pitch - bpp + c];
			out[(height - 1) * pitch + c] = in[(height - 1) * pitch + c];
			out[height * pitch - bpp + c] = in[height * pitch - bpp + c];
		}

		// if we only copy the edge pixels, then they receive no filtering, making them
		// look out of place after several passes of an image.  So we filter the edge
		// rows/columns, duplicating the edge pixels for one side of the "matrix"

		// first row
		for (x = 1; x < (width - 1); x++)
		{
			for (c = 0; c < bpp; c++)
			{
				n = in[(x-1)*bpp+c]*m[0] + in[x*bpp+c]*m[1] + in[(x+1)*bpp+c]*m[2] + in[(x-1)*bpp+c]*m[3] + in[x*bpp+c]*m[4] + in[(x+1)*bpp+c]*m[5] + in[(width+(x-1))*bpp+c]*m[6] + in[(width+(x))*bpp+c]*m[7] + in[(width+(x-1))*bpp+c]*m[8];
				t = (unsigned int)fabs(n/scale + bias);
				out[x*bpp+c] = min(t, 255);
			}
		}

		// last row
		y = (height - 1) * pitch;
		for (x = 1; x < (width - 1); x++)
		{
			for (c = 0; c < bpp; c++)
			{
				n = in[y-pitch+(x-1)*bpp+c]*m[0] + in[y-pitch+x*bpp+c]*m[1] + in[y-pitch+(x+1)*bpp+c]*m[2] + in[y+(x-1)*bpp+c]*m[3] + in[y+x*bpp+c]*m[4] + in[y+(x+1)*bpp+c]*m[5] + in[y+(x-1)*bpp+c]*m[6] + in[y+x*bpp+c]*m[7] + in[y+(x-1)*bpp+c]*m[8];
				t = (unsigned int)fabs(n/scale + bias);
				out[y+x*bpp+c] = min(t, 255);
			}
		}

		// left side
		for (i = 1, y = pitch; i < height-1; i++, y += pitch)
		{
			for (c = 0; c < bpp; c++)
			{
				n=in[y-pitch+c]*m[0] + in[y-pitch+bpp+c]*m[1] + in[y-pitch+2*bpp+c]*m[2] + in[y+c]*m[3] + in[y+bpp+c]*m[4] + in[y+2*bpp+c]*m[5] + in[y+pitch+c]*m[6] + in[y+pitch+bpp+c]*m[7] + in[y+pitch+2*bpp+c]*m[8];
				t = (unsigned int)fabs(n/scale + bias);
				out[y + c] = min(t, 255);
			}
		}

		// right side
		for (i = 1, y = pitch*2-bpp; i < height-1; i++, y += pitch)
		{
			for (c = 0; c < bpp; c++)
			{
				n = in[y-pitch+c]*m[0] + in[y-pitch+bpp+c]*m[1] + in[y-pitch+2*bpp+c]*m[2] + in[y+c]*m[3] + in[y+bpp+c]*m[4] + in[y+2*bpp+c]*m[5] + in[y+pitch+c]*m[6] + in[y+pitch+bpp+c]*m[7] + in[y+pitch+2*bpp+c]*m[8];
				t = (unsigned int)fabs(n/scale + bias);
				out[y+c] = min(t, 255);
			}
		}

		// swap buffers
		if (out == out_data)
		{
			in = out_data;
			out = out2_data;
		}
		else
		{
			in = out2_data;
			out = out_data;
		}
	}

	// remove source pic
	if (out == out_data)
	{
		temp = filtered2;
		fiStoreUnalignedData(temp, out2_data, width, height, bpp);
	}
	else
	{
		temp = filtered;
		fiStoreUnalignedData(temp, out_data, width, height, bpp);
	}
	if (temp == filtered)
	{
		if (filtered2 != bitmap)
		{
			fiFree(filtered2);
			if (removeSource)
				fiFree(bitmap);
		}
		else if (removeSource)
			fiFree(bitmap);
	}
	else
	{
		if (filtered != bitmap)
		{
			fiFree(filtered);
			if (removeSource)
				fiFree(bitmap);
		}
		else if (removeSource)
			fiFree(bitmap);
	}

	// clean up
	fiFreeUnalignedData(in_data, in_allocated);
	fiFreeUnalignedData(out_data, out_allocated);
	fiFreeUnalignedData(out2_data, out2_allocated);

	// return
	return temp;


}

// apply gaussian blur to bitmap
FIBITMAP *fiBlur(FIBITMAP *bitmap, int iteractions, bool removeSource)
{
	double k[9], scale = 0;
	int i;

	if (!bitmap)
		return NULL;

	// check bitmap type
	int bpp = FreeImage_GetBPP(bitmap) / 8;
	if (bpp != 1 && bpp != 3 && bpp != 4)
	{
		Warning("fiBlur: only supported for 8, 24 or 32-bit bitmaps");
		return fiClone(bitmap);
	}

	// create kernel
	k[0] = 1.0f; k[1] = 2.0f; k[2] = 1.0f; 
	k[3] = 2.0f; k[4] = 4.0f; k[5] = 2.0f; 
	k[6] = 1.0f; k[7] = 2.0f; k[8] = 1.0f;
	for (i = 0; i < 9; i++)
		scale += k[i];

	// blur
	return fiFilter(bitmap, k, scale, 0, iteractions, removeSource);
}

// apply sharpen
// factor < 1 blurs, > 1 sharpens
FIBITMAP *fiSharpen(FIBITMAP *bitmap, float factor, int iteractions, bool removeSource)
{
	byte *in, *out, *end, *data_blurred, *data_sharpened;
	int blurredpitch, sharpenedpitch, y, w, h;
	double k[9], scale = 0;

	// check bitmap type
	int bpp = FreeImage_GetBPP(bitmap) / 8;
	if (bpp != 1 && bpp != 3 && bpp != 4)
	{
		Warning("fiSharpen: only supported for 8, 24 or 32-bit bitmaps");
		return fiClone(bitmap);
	}

	// get blurred image
	k[0] = 1.0f; k[1] = 2.0f; k[2] = 1.0f; 
	k[3] = 2.0f; k[4] = 4.0f; k[5] = 2.0f; 
	k[6] = 1.0f; k[7] = 2.0f; k[8] = 1.0f;
	for (int i = 0; i < 9; i++)
		scale += k[i];
	FIBITMAP *blurred = fiFilter(bitmap, k, scale, 0, 2, false);

	// sharpen
	w = FreeImage_GetWidth(bitmap);
	h = FreeImage_GetHeight(bitmap);
	FIBITMAP *sharpened = fiClone(bitmap);

	float rf = 1 - factor;
	for ( ; iteractions > 0; iteractions--)
	{
		data_blurred = fiGetData(blurred, &blurredpitch);
		data_sharpened = fiGetData(sharpened, &sharpenedpitch);
		for (y = 0; y < h; y++)
		{
			in = data_blurred;
			end = in + w*bpp;
			out = data_sharpened;
			while(in < end)
			{
				*out = min(255, max(0, (int)(*in*rf + *out*factor)));
				out++;
				in++;
			}
			data_sharpened += sharpenedpitch;
			data_blurred += blurredpitch;
		}
	}
	fiFree(blurred);
	if (removeSource)
		fiFree(bitmap);

	return sharpened;
}

/*
==========================================================================================

  FIX TRANSPARENT PIXELS FOR ALPHA BLENDING

==========================================================================================
*/

// fill rgb data of transparent pixels from non-transparent neighbors to fix 'black aura' effect
FIBITMAP *fiFixTransparentPixels(FIBITMAP *bitmap)
{
	byte *data, *out, *pixel, *npix;
	int x, y, nc, nx, ny, w, h;
	float rgb[3], b, br;

	// check bitmap type
	if (FreeImage_GetBPP(bitmap) != 32)
	{
		Warning("fiSharpen: only supported 32-bit bitmaps");
		return fiClone(bitmap);
	}

	FIBITMAP *filled = fiClone(bitmap);
	// vortex: since BPP is 4, data is always properly aligned, so we dont need pitch
	data = fiGetData(bitmap, NULL);
	out = fiGetData(filled, NULL);
	w = FreeImage_GetWidth(bitmap);
	h = FreeImage_GetHeight(bitmap);
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			pixel = data + w*4*y + x*4;
			if (pixel[3] < 255)
			{
				// fill from neighbors
				nc = 0;
				rgb[0] = 0.0f;
				rgb[1] = 0.0f;
				rgb[2] = 0.0f;
				#define fill(_x,_y) nx = (_x < 0) ? (w - 1) : (_x >= w) ? 0 : _x; ny = (_y < 0) ? (h - 1) : (_y >= h) ? 0 : _y; npix = data + w*4*ny + nx*4; if (npix[3] > tex_binaryAlphaMin) { nc++; rgb[0] += (float)npix[0]; rgb[1] += (float)npix[1]; rgb[2] += (float)npix[2]; }
				fill(x-1,y-1)
				fill(x  ,y-1)
				fill(x+1,y-1)
				fill(x-1,y  )
				fill(x+1,y  )
				fill(x-1,y+1)
				fill(x  ,y+1)
				fill(x+1,y+1)
				#undef fill
				if (nc)
				{
					b = 1.0f - (float)pixel[3] / 255.0f;
					br = 1.0f - b;
					npix = out + w*4*y + x*4;
					*npix++ = (byte)min(255, (pixel[0]*br + (rgb[0]/nc)*b));
					*npix++ = (byte)min(255, (pixel[1]*br + (rgb[1]/nc)*b));
					*npix++ = (byte)min(255, (pixel[2]*br + (rgb[2]/nc)*b));
				}
			}
		}
	}
	return filled;
}