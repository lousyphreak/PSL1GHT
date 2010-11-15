#include "main.h"


u32 convert(efbBuffer *inBuffer, u32 color)
{
	u16 r = ((color >> inBuffer->rShift) << (8-inBuffer->rBits)) & 0xFF;
	u16 g = ((color >> inBuffer->gShift) << (8-inBuffer->gBits)) & 0xFF;
	u16 b = ((color >> inBuffer->bShift) << (8-inBuffer->bBits)) & 0xFF;

	return (r<<16|g<<8|b);
}

u32 convertColor(efbBuffer *inBuffer, void *cacheLine, u16 x)
{
	u32 color;
	switch(inBuffer->bytesPerPixel)
	{
	case 1:
		color=((u32*)inBuffer->paletteAddress)[((u8*)cacheLine)[x]];
		break;
	case 2:
		color=((u16*)cacheLine)[x];
		break;
	case 3:
		color=((u8*)cacheLine)[x];
		break;
	case 4:
		color=((u32*)cacheLine)[x];
		break;
	default:
		color=0;
	}
	return convert(inBuffer, color);
}
