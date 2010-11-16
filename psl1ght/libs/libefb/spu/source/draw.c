#include "main.h"

// size if inBuffer (u8)
#define BUFFERSIZE_IN (1024*2*4)
#define NUM_IN_BUFFERS 4

// ring buffer for source data
u8 inBuffer_raw0[BUFFERSIZE_IN] __attribute__((aligned(128)));
u8 inBuffer_raw1[BUFFERSIZE_IN] __attribute__((aligned(128)));
u8 inBuffer_raw2[BUFFERSIZE_IN] __attribute__((aligned(128)));
u8 inBuffer_raw3[BUFFERSIZE_IN] __attribute__((aligned(128)));

// size of ringbuffers (u32)
#define BUFFERSIZE (1024*8)
// buffer segments
#define BUFFER_SEGMENT_LEN (BUFFERSIZE/4)

// in buffer struct
efbBuffer inBuffer __attribute__((aligned(128)));

// ring buffer for converted source data
u32 inBufferData[BUFFERSIZE] __attribute__((aligned(128)));

// ring buffer for target data
u32 outBuffer[BUFFERSIZE] __attribute__((aligned(128)));

// buffer for palette (if exists)
u32 inPalette[256] __attribute__((aligned(128)));

#define MAKE_RING_ADDRESS(x) (x%BUFFERSIZE)

#define MAKE_RING_ADDRESS_IN(x) (x%BUFFERSIZE_IN)
#define GET_RING_BUFFER_NUM(x) (x/BUFFERSIZE_IN)
#define CLAMP_RING_BUFFER_NUM(x) (x%NUM_IN_BUFFERS)
__attribute__((always_inline)) u8 *getInBufferRaw(u16 which)
{
	if(which==0)
		return inBuffer_raw0;
	if(which==1)
		return inBuffer_raw1;
	if(which==2)
		return inBuffer_raw2;
	return inBuffer_raw3;
}



/*
 * DMA:
 * # Naturally aligned transfer sizes of 1, 2, 4, or 8 bytes and multiples of 16 bytes.
 * # Maximum transfer size of 16 KB.
 * # Peak performance is achieved when both the EA and LSA are 128-byte aligned and the size of the transfer is a multiple of 128 bytes.
 */

/*
 * __builtin_expect has the prototype
 *
 *  	long int __builtin_expect (long int exp, long int val)
 *
 * and it means that almost certainly the value of `exp' is `val'.  This
 * probability value will then be used in condition expressions.
 *
 */



// last byte of source already buffered
u32 srcBuffered=0;

/*
 * src: address of source buffer
 * begin: begin of required data (bytes)
 * end: end of required data (bytes)
 * totalsize: total size of inbuffer
 * bytes: bytes per pixel
 */
void bufferSource(void *src/*, u32 begin*/, u32 end, u32 totalSize)
{
	//u16 ring_begin=GET_RING_BUFFER_NUM(begin);
	u16 ring_end=GET_RING_BUFFER_NUM(end);

	// skip if enough buffer ready, __builtin_expect for branch prediction
	if(__builtin_expect ((srcBuffered>=ring_end), 1))
		return;


	u32 tag=1;
	u32 tag_mask=1<<1;

	u16 i;
	// read all requested buffers
	for(i=srcBuffered;i<=ring_end;i++)
	{
		// get the end where this read would lead to
		u32 startRead=BUFFERSIZE_IN*i;

		u8 *buffer=getInBufferRaw(CLAMP_RING_BUFFER_NUM(i));
		void *srcp=src+startRead;

		// if the inbuffer is smaller than what we want...
		//if(__builtin_expect ((startRead+BUFFERSIZE_IN>=totalSize), 0))
		if(startRead+BUFFERSIZE_IN>=totalSize)
		{
			// how much is left in the buffer
			u32 restread=totalSize-startRead;
			u32 rr;
			// pad to allow dma (dont care for reading too much)
			for(rr=32;rr<=restread;rr*=2);
			// dma
			mfc_get(buffer,(u64)(u32)srcp,BUFFERSIZE_IN,tag,0,0);
		}
		else // all fine, read
		{
			// dma
			mfc_get(buffer,(u64)(u32)srcp,BUFFERSIZE_IN,tag,0,0);
		}

		srcBuffered=i;
	}

	// wait on dma finished
	mfc_write_tag_mask(tag_mask);
	mfc_read_tag_status_all();

	// color conversion and write to inBufferData
	for(i=srcBuffered;i<=ring_end;i++)
		inBufferData[MAKE_RING_ADDRESS(i)] = 0xFF00FF;//convertColor(&inBuffer,getInBufferRaw(CLAMP_RING_BUFFER_NUM(i)),1);
	return;
}
// last byte of target already written
u32 targetBuffered=0;

/*
 * target: framebuffer
 * end: may flush up to (u32)
 * totalsize: total size of framebuffer (u32)
 */
void bufferTarget(void *target, u32 end, u32 totalSize)
{
	u32 tag=2;

	//write out in 128 byte blocks
	if(targetBuffered+128<=end*4)
	{
		mfc_put(&outBuffer[MAKE_RING_ADDRESS(targetBuffered/4)],((u64)(u32)target)+targetBuffered,128,tag,0,0);
		// step on
		targetBuffered+=128;
	}

	// wait for finish of the dma
	mfc_write_tag_mask(1<<tag);
	mfc_read_tag_status_all();

	return;
}


void draw()
{
	// fetch parameters from mailbox
	u32 targetAddress=spu_readch(SPU_RdInMbox);
	u32 sourceAddress=spu_readch(SPU_RdInMbox);
	u32 startLine=spu_readch(SPU_RdInMbox);
	u32 endLine=spu_readch(SPU_RdInMbox);

	// dma in the buffer struct
	mfc_get(&inBuffer, sourceAddress, sizeof(inBuffer),1,0,0);
	mfc_write_tag_mask(1<<1);
	mfc_read_tag_status_all();

	// do we have a palette?
	if(inBuffer.paletteAddress!=0)
	{
		// dma in the palette and wait for it
		mfc_get(&inPalette, inBuffer.paletteAddress, sizeof(u32)*256,1,0,0);
		mfc_write_tag_mask(1<<1);
		mfc_read_tag_status_all();
		// update address to local buffer
		inBuffer.paletteAddress=(u32)inPalette;
	}

	// calculate ratios for scaling
	float widthRatio=inBuffer.width/(float)_config.width;
	float heightRatio=inBuffer.height/(float)_config.height;

	// reset buffered status
	srcBuffered=0;
	targetBuffered=0;

	// line pitch, source is calculated in bytes...
	u16 sourcePitch=inBuffer.width*inBuffer.bytesPerPixel;
	u32 sourceSize=inBuffer.width*inBuffer.height*inBuffer.bytesPerPixel;

	// ... target in pixels
	u16 targetPitch=_config.width;
	u32 targetSize=_config.width*_config.height;

	// loop all assigned pixels
	for(u32 targetPos=startLine*targetPitch;targetPos<endLine*targetPitch;targetPos++)
	{
		//rebuild source and target x/y for current pixel
		u16 targetX=targetPos%targetPitch;
		u16 targetY=targetPos/targetPitch;
		u16 sourceX=targetX*widthRatio;
		u16 sourceY=targetY*heightRatio;

		u32 sourcePos=sourceX*inBuffer.bytesPerPixel+sourceY*sourcePitch;


/** /
		spu_writech(SPU_WrOutMbox, 0xFFFFFFFF);
		spu_writech(SPU_WrOutMbox, targetX);//0x0000001C
		spu_writech(SPU_WrOutMbox, targetY);//0x0000003A
		spu_writech(SPU_WrOutMbox, targetPos);//0x00001D1C
		spu_writech(SPU_WrOutMbox, MAKE_RING_ADDRESS(targetPos));//0x00001D1C
		spu_writech(SPU_WrOutMbox, GET_RING_BUFFER_NUM(sourcePos));//0x00000001
		spu_writech(SPU_WrOutMbox, (u32)(u64)getInBufferRaw(CLAMP_RING_BUFFER_NUM(srcBuffered)));//0x00009480
/**/

		// make sure buffer is filled
		bufferSource((void*)inBuffer.framebufferAddress,sourcePos,sourceSize);

		// move data into the outbuffer
		outBuffer[MAKE_RING_ADDRESS(targetPos)]=inBufferData[MAKE_RING_ADDRESS(sourcePos)];

		// write to the framebuffer
		bufferTarget((void*)targetAddress,targetPos,targetSize);
	}
}

