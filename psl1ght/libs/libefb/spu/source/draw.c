#include "main.h"

// number of u32
#define BUFFERSIZE 2048
//#define OUT_BUFFERNUM  4


volatile efbBuffer inBuffer __attribute__((aligned(128)));

volatile u32 outBuffer1[BUFFERSIZE] __attribute__((aligned(128)));
volatile u32 outBuffer2[BUFFERSIZE] __attribute__((aligned(128)));
volatile u32 outBuffer3[BUFFERSIZE] __attribute__((aligned(128)));
volatile u32 outBuffer4[BUFFERSIZE] __attribute__((aligned(128)));

volatile u32 inBuffer1[BUFFERSIZE] __attribute__((aligned(128)));
volatile u32 inBuffer2[BUFFERSIZE] __attribute__((aligned(128)));
volatile u32 inBuffer3[BUFFERSIZE] __attribute__((aligned(128)));
volatile u32 inBuffer4[BUFFERSIZE] __attribute__((aligned(128)));

#define SRC_PREBUFFER 1
#define SRC_PREBUFFER_NEED 1
#define TARGET_PREBUFFER 1
#define TARGET_PREBUFFER_NEED 1

void bufferSource(void *src, u16 y, u16 lineWidth);
void bufferTarget(void *target, u16 y, u16 lineWidth);

u16 srcBuffered=0;

volatile u32 *getInBuffer(u16 which)
{
	if(which==0)
		return inBuffer1;
	else if(which==1)
		return inBuffer2;
	else if(which==2)
		return inBuffer3;
	else if(which==3)
		return inBuffer4;
	return 0;
}

volatile u32 *getOutBuffer(u16 which)
{
	if(which==0)
		return outBuffer1;
	else if(which==1)
		return outBuffer2;
	else if(which==2)
		return outBuffer3;
	else if(which==3)
		return outBuffer4;
	return 0;
}

void bufferSource(void *src, u16 y, u16 lineWidth)
{
	u32 tag=1;
	volatile u32 *buffer=inBuffer1;

	mfc_get(buffer,(u64)(u32)src+lineWidth*y*4,lineWidth*4,tag,0,0);
	mfc_write_tag_mask(1<<tag);
	mfc_read_tag_status_all();

	return;
/*
	//spu_writech(SPU_WrOutMbox, 0x1234);
	for(u16 i=0;i<SRC_PREBUFFER;i++)
	{
		u16 ty=y+i;
		u16 tag=ty%4+1;
		if(srcBuffered<ty)
		{
			volatile u32 *buffer=getInBuffer(tag-1);

			mfc_get(buffer,(u64)(u32)src+lineWidth*ty,lineWidth,tag,0,0);
		}

		if(i<SRC_PREBUFFER_NEED)
		{
			mfc_write_tag_mask(1<<tag);
			mfc_read_tag_status_all();
		}
	}*/
}
u16 targetBuffered=0;

void bufferTarget(void *target, u16 y, u16 lineWidth)
{
	u32 tag=1;
	volatile u32 *buffer=outBuffer1;

	mfc_put(buffer,((u64)(u32)target)+lineWidth*y*4,lineWidth*4,tag,0,0);
	mfc_write_tag_mask(1<<tag);
	mfc_read_tag_status_all();

	return;
/*
	for(u16 i=0;i<SRC_PREBUFFER;i++)
	{
		u16 ty=y+i;
		u16 tag=ty%4+6;
		if(targetBuffered<ty)
		{
			volatile u32 *buffer=getOutBuffer(tag-1);

			mfc_put(buffer,(u64)(u32)target+lineWidth*ty,lineWidth,tag,0,0);
		}

		if(i<SRC_PREBUFFER_NEED)
		{
			mfc_write_tag_mask(1<<tag);
			mfc_read_tag_status_all();
		}
	}*/
}

/*
 * DMA:
 * # Naturally aligned transfer sizes of 1, 2, 4, or 8 bytes and multiples of 16 bytes.
 * # Maximum transfer size of 16 KB.
 * # Peak performance is achieved when both the EA and LSA are 128-byte aligned and the size of the transfer is a multiple of 128 bytes.
 */

void draw()
{
	u32 targetAddress=spu_readch(SPU_RdInMbox);
	u32 sourceAddress=spu_readch(SPU_RdInMbox);

	mfc_get(&inBuffer, sourceAddress, sizeof(inBuffer),1,0,0);
	mfc_write_tag_mask(1<<1);
	mfc_read_tag_status_all();

	spu_writech(SPU_WrOutMbox, inBuffer.width);
	spu_writech(SPU_WrOutMbox, inBuffer.height);
	spu_writech(SPU_WrOutMbox, _config.width);
	spu_writech(SPU_WrOutMbox, _config.height);
/*
	_config.height=128;
	_config.width=128*2;
	inBuffer.height=128;
	inBuffer.width=128*2;

	spu_writech(SPU_WrOutMbox, inBuffer.width);
	spu_writech(SPU_WrOutMbox, inBuffer.height);
	spu_writech(SPU_WrOutMbox, _config.width);
	spu_writech(SPU_WrOutMbox, _config.height);
*/
	float widthRatio=inBuffer.width/(float)_config.width;
	float heightRatio=inBuffer.height/(float)_config.height;

	spu_writech(SPU_WrOutMbox, (u32)(widthRatio*10000));
	spu_writech(SPU_WrOutMbox, (u32)(heightRatio*10000));

	for(u16 y=0;y<_config.height;y++)
	{
		//spu_writech(SPU_WrOutMbox, 0x1100000|y);
		u16 srcY=y*heightRatio;
		//spu_writech(SPU_WrOutMbox, 0x1200000|srcY);
		bufferSource((void*)inBuffer.framebufferAddress,srcY,inBuffer.width);
		//spu_writech(SPU_WrOutMbox, (u32)y);
		//spu_writech(SPU_WrOutMbox, (u32)srcY);

		for(u16 x=0;x<_config.width;x++)
		{
			u16 srcX=x*widthRatio;

			volatile u32 *dstBuffer=outBuffer1;//getOutBuffer(y);
			volatile u32 *srcBuffer=inBuffer1;//getInBuffer(srcY);

			dstBuffer[x]=srcBuffer[srcX];
		}
		bufferTarget((void*)targetAddress,y,_config.width);
	}
}

