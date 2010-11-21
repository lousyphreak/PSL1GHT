#include "main.h"


// size of ringbuffers (u32)
#define BUFFER_SEGMENT_SIZE (1024*2)
#define BUFFER_SEGMENT_SIZE_BYTES (BUFFER_SEGMENT_SIZE*4)
// buffer segments
#define BUFFER_SIZE (BUFFER_SEGMENT_SIZE*4)

#define BUFFER_IN_SEGMENTS 4
#define BUFFER_OUT_SEGMENTS 2

#define BUFFER_IN_SIZE (BUFFER_SEGMENT_SIZE*BUFFER_IN_SEGMENTS)
#define BUFFER_OUT_SIZE (BUFFER_SEGMENT_SIZE*BUFFER_OUT_SEGMENTS)

// in buffer struct
efbBuffer inBuffer __attribute__((aligned(128)));

// ring buffer for converted source data
u32 inBuffer_conv[BUFFER_IN_SIZE] __attribute__((aligned(128)));

// ring buffer for target data
u32 outBuffer[BUFFER_OUT_SIZE] __attribute__((aligned(128)));

// buffer for palette (if exists)
u32 inPalette[256] __attribute__((aligned(128)));

// read buffer for source data
u32 inBuffer_raw[BUFFER_SEGMENT_SIZE*2] __attribute__((aligned(128)));




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



// last segment of source DMA started
u32 srcDMA=-1;
// last segment of source DMA finished and converted
u32 srcCONV=0;

/*
 * src: address of source buffer
 * begin: begin of required data (pixel)
 * end: end of required data (pixel)
 * totalsize: total size of inbuffer
 * bytes: bytes per pixel
 */
void bufferSource(void *src, u32 end, u32 totalSize, u32 inSegmentBytes)
{
	//u32 endSegment=end;

	// skip if enough buffer ready, __builtin_expect for branch prediction
	//if(__builtin_expect ((srcBuffered>=ring_end), 1))
	if(srcCONV>end)
		return;


	// if the inbuffer is smaller than what we want...
	//if(__builtin_expect ((startRead+BUFFERSIZE_IN>=totalSize), 0))
	while(srcCONV<end)
	{
		u32 tag=(srcCONV/BUFFER_SEGMENT_SIZE)%2;
		u32 tagMask=1<<(2-tag);
		u8 *target=&((u8*)inBuffer_raw)[tag*BUFFER_SEGMENT_SIZE_BYTES];
		u8 *srcDMAPtr=src+(inBuffer.bytesPerPixel*srcDMA);

		if(srcDMA<totalSize)
		{
			if(srcDMA+BUFFER_SEGMENT_SIZE>=totalSize)
			{
				// how much is left in the buffer
				u32 restread=totalSize-srcDMA;
				u32 rr;
				// pad to allow dma (dont care for reading too much)
				for(rr=32;rr<=restread;rr*=2);
				// dma
				mfc_get(target,(u64)(u32)srcDMAPtr,rr*inBuffer.bytesPerPixel,tag+1,0,0);
			}
			else // all fine, read
			{
				// dma
				mfc_get(target,(u64)(u32)srcDMAPtr,inSegmentBytes,tag+1,0,0);
			}
			srcDMA+=BUFFER_SEGMENT_SIZE;

			if(srcDMA==srcCONV+BUFFER_SEGMENT_SIZE)
				continue;
		}
		// wait on dma finished
		mfc_write_tag_mask(tagMask);
		mfc_read_tag_status_all();

		//nth buffer
		//u16 ring_segment=(startRead/BUFFER_SEGMENT_LEN_PIXEL)%4;

		// color conversion and write to inBufferData
		u8 *srcConvPtr=&((u8*)inBuffer_raw)[(1-tag)*BUFFER_SEGMENT_SIZE_BYTES];
		u32 srcRead=srcCONV%BUFFER_IN_SIZE;

		for(int j=0;j<BUFFER_SEGMENT_SIZE;j++)
		{
			inBuffer_conv[srcRead] = convertColor(&inBuffer,srcConvPtr,j);
			//srcCONV++;
			srcRead++;
		}
		srcCONV+=BUFFER_SEGMENT_SIZE;
		//startRead=srcBuffered;
	}

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
	u32 tag=10;

	//write out in 128 byte blocks
	if(targetBuffered+BUFFER_SEGMENT_SIZE<=end)
	{
		u32 targetPixel=targetBuffered%BUFFER_OUT_SIZE;
		//u16 ring_segment=(targetBuffered/BUFFER_SEGMENT_LEN_PIXEL)%4;
		mfc_put(&outBuffer[targetPixel],((u64)(u32)target)+targetBuffered*4,BUFFER_SEGMENT_SIZE_BYTES,tag,0,0);
		// step on
		targetBuffered+=BUFFER_SEGMENT_SIZE;

		// wait for finish of the dma
		mfc_write_tag_mask(1<<tag);
		mfc_read_tag_status_all();
	}


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
	srcDMA=0;
	srcCONV=0;
	targetBuffered=0;

	// source and target size in pixels...
	u32 sourceSize=inBuffer.width*inBuffer.height;
	u32 targetSize=_config.width*_config.height;

	u32 inSegmentBytes=BUFFER_SEGMENT_SIZE*inBuffer.bytesPerPixel;

	// loop all assigned pixels
	for(u32 targetPos=startLine*_config.width;targetPos<endLine*_config.width;targetPos++)
	{
		//rebuild source and target x/y for current pixel
		u16 targetX=targetPos%_config.width;
		u16 targetY=targetPos/_config.width;
		u16 sourceX=targetX*widthRatio;
		u16 sourceY=targetY*heightRatio;

		u32 sourcePos=sourceX+sourceY*inBuffer.width;


/** /
		spu_writech(SPU_WrOutMbox, 0xFFFFFFFF);
		spu_writech(SPU_WrOutMbox, targetX);//0x0000001C
		spu_writech(SPU_WrOutMbox, targetY);//0x0000003A
		spu_writech(SPU_WrOutMbox, targetPos);//0x00001D1C
		spu_writech(SPU_WrOutMbox, MAKE_RING_ADDRESS(targetPos));//0x00001D1C
		spu_writech(SPU_WrOutMbox, GET_RING_BUFFER_NUM(sourcePos));//0x00000001
		spu_writech(SPU_WrOutMbox, (u32)(u64)getInBufferRaw(CLAMP_RING_BUFFER_NUM(srcBuffered)));//0x00009480
/ **/

		//u16 target_ring_segment=(targetPos/BUFFER_SEGMENT_LEN_PIXEL)%4;
		//u16 source_ring_segment=(sourcePos/BUFFER_SEGMENT_LEN_PIXEL)%4;
		// make sure buffer is filled
		bufferSource((void*)inBuffer.framebufferAddress,sourcePos,sourceSize,inSegmentBytes);

		u32 srcIndex=sourcePos%BUFFER_IN_SIZE;
		u32 targetIndex=targetPos%BUFFER_OUT_SIZE;


		// move data into the outbuffer
		outBuffer[targetIndex]=inBuffer_conv[srcIndex];
/*		vector int *buf=(vector int *)&outBuffer[targetIndex];
		vector int data1=spu_splats(0xdd0000);
		vector int data2=spu_splats(0x00dd00);
		vector int data3=spu_splats(0x0000dd);
		vector int data4=spu_splats(0x000000);
		buf[0]=data1;
		buf[1]=data2;
		buf[2]=data3;
		buf[3]=data4;
*/
		// write to the framebuffer
		bufferTarget((void*)targetAddress,targetPos,targetSize);
	}
}

