#pragma once

#include <spu_intrinsics.h>
#include <spu_mfcio.h>

typedef unsigned char      u8;
typedef   signed char      s8;
typedef unsigned short     u16;
typedef   signed short     s16;
typedef unsigned int       u32;
typedef   signed int       s32;
typedef unsigned long long u64;
typedef   signed long long s64;

#define TAG_READ 1
#define TAG_WRITE 2

#define EFB_COMMAND_DRAW  1
#define EFB_COMMAND_UPDATE_CONFIG 2
#define EFB_COMMAND_QUIT 3

#define EFB_RESPONSE_DRAW_FINISHED 0x11102221
#define EFB_RESPONSE_CONFIG_FINISHED 2

typedef struct
{
	// pointer to ram buffer
	u32 framebufferAddress;
	// pointer to palette, ALWAYS 32bit RGBX 256 entries (NULL if unused)
	u32 paletteAddress;
	// size
	u16 width;
	u16 height;
	u16 bytesPerPixel;
	// how much the colors are shifted
	u16 rShift;
	u16 gShift;
	u16 bShift;
	// how much bits each color uses
	u16 rBits;
	u16 gBits;
	u16 bBits;


	u16 padding[3];
	//remember to stay pow2 size
} efbBuffer;

typedef struct
{
	// blitting mode
	u16 mode;
	// 0 to stretch to full, 1 to keep aspect ratio
	u16 keepAspect;
	// EFB_STRETCH_*
	u16 stretchX;
	// EFB_STRETCH_*
	u16 stretchY;
	// filter to use, note that some override stretchX&Y
	u16 filter;
	// screen (tv) resolution
	u16 width;
	u16 height;

	u16 padding;
} efbConfig;


extern efbConfig _config;

void draw();
u32 spuReadVerify();
void check_config();
u32 convertColor(efbBuffer *inBuffer, void *cacheLine, u16 x);

