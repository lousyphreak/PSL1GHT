#pragma once

#include <psl1ght/types.h>

//http://pastie.org/1293476

EXTERN_BEGIN

// conversion stretching, you name it
#define EFB_MODE_DEFAULT 0
// straight blit (no stretching, no conversion)
#define EFB_MODE_BLIT 1

// no stretching
#define EFB_STRETCH_NONE 0
// stretch to multiples only
#define EFB_STRETCH_MULTIPLE 1
// stretch to full size
#define EFB_STRETCH_FULL 2

// default box filter
#define EFB_FILTER_BOX 0

/*
 * struct containing infos about the buffer
 * NEED TO BE 16 BYTE ALIGNED
 *
 * palette format:
 *   bytesPerPixel=1
 *   shift and bits values ignored
 *
 * rgb format:
 *   565 RGB:
 *     bytesPerPixel=2
 *     rShift=11
 *     gShift=5
 *     bShift=0
 *     rBits=5
 *     gBits=6
 *     bBits=5
 */
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

typedef struct
{
	u32 initialized;
	u32 spu;
} efbData;

efbData *efbInit(efbConfig *config);
void efbShutdown(efbData *efb);
void efbUpdateConfig(efbData *efb,efbConfig *config);

void efbBlitToScreen(efbData *efb, void *framebuffer, efbBuffer *buffer);
void efbWaitForBlit(efbData *efb);

EXTERN_END
