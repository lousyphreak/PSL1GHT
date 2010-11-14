#include <psl1ght/lv2.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <sysutil/video.h>
#include <rsx/gcm.h>
#include <rsx/reality.h>

#include <io/pad.h>

#include <psl1ght/lv2.h>

#include <cairo/cairo.h>
#include "draw.h"

#include <math.h>

#include <efb/efb.h>
#include <psl1ght/lv2.h>
#include <psl1ght/lv2/spu.h>
#include <lv2/spu.h>

gcmContextData *context; // Context to keep track of the RSX buffer.

VideoResolution res; // Screen Resolution

int currentBuffer = 0;
buffer *buffers[2]; // The buffer we will be drawing into.

efbData *efbD;
uint32_t *offscreenBuffers[2];
efbBuffer *efbBuffers[2];
u32 offWidth=128*1;
u32 offHeight=128*1;
//u32 offWidth=1920;
//u32 offHeight=1080;




void waitFlip() { // Block the PPU thread untill the previous flip operation has finished.
	while(gcmGetFlipStatus() != 0) 
		usleep(200);  // Sleep, to not stress the cpu.
	gcmResetFlipStatus();
}

// Flip a buffer onto the screen.
void flip(s32 buffer) {
	assert(gcmSetFlip(context, buffer) == 0);
	realityFlushBuffer(context);
	gcmSetWaitFlip(context); // Prevent the RSX from continuing until the flip has finished.
}

void makeBuffer(int id, int size) {
	buffer *buf = malloc(sizeof(buffer));
	buf->ptr = rsxMemAlign(16, size);
	assert(buf->ptr != NULL);

	assert(realityAddressToOffset(buf->ptr, &buf->offset) == 0);
	// Register the display buffer with the RSX
	assert(gcmSetDisplayBuffer(id, buf->offset, res.width * 4, res.width, res.height) == 0);
	
	buf->width = res.width;
	buf->height = res.height;
	buffers[id] = buf;
}

// Initilize everything. You can probally skip over this function.
void init_screen() {
	// Allocate a 1Mb buffer, alligned to a 1Mb boundary to be our shared IO memory with the RSX.
	void *host_addr = memalign(1024*1024, 1024*1024);
	assert(host_addr != NULL);

	// Initilise Reality, which sets up the command buffer and shared IO memory
	context = realityInit(0x10000, 1024*1024, host_addr); 
	assert(context != NULL);

	VideoState state;
	assert(videoGetState(0, 0, &state) == 0); // Get the state of the display
	assert(state.state == 0); // Make sure display is enabled

	// Get the current resolution
	assert(videoGetResolution(state.displayMode.resolution, &res) == 0);
	
	// Configure the buffer format to xRGB
	VideoConfiguration vconfig;
	memset(&vconfig, 0, sizeof(VideoConfiguration));
	vconfig.resolution = state.displayMode.resolution;
	vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
	vconfig.pitch = res.width * 4;

	assert(videoConfigure(0, &vconfig, NULL, 0) == 0);
	assert(videoGetState(0, 0, &state) == 0); 

	s32 buffer_size = 4 * res.width * res.height; // each pixel is 4 bytes
	printf("buffers will be 0x%x bytes\n", buffer_size);
	
	gcmSetFlipMode(GCM_FLIP_VSYNC); // Wait for VSYNC to flip

	// Allocate two buffers for the RSX to draw to the screen (double buffering)
	makeBuffer(0, buffer_size);
	makeBuffer(1, buffer_size);

	gcmResetFlipStatus();
	flip(1);
}

// Draw a single frame, do all your drawing/animation from in here.
void drawFrame(buffer *buffer, long frame) {
	cairo_t *cr;

	/* We are just going to attach cairo directly to the buffer in the rsx memory.
	 * If this gets too slow later with blending, we will need to create a buffer
         * on cell then copy the finished result accross.
	 */
	//cairo_surface_t *surface = cairo_image_surface_create_for_data((u8 *) buffer->ptr,
	//	CAIRO_FORMAT_RGB24, buffer->width, buffer->height, buffer->width * 4);
	cairo_surface_t *surface = cairo_image_surface_create_for_data((u8 *) buffer,
		CAIRO_FORMAT_RGB16_565, offWidth, offHeight, offWidth * 2);
	assert(surface != NULL);
	cr = cairo_create(surface);
	assert(cr != NULL);

	// Lets start by clearing everything
	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0); // White
	cairo_paint(cr);

	cairo_set_line_width(cr, 10.0); // 30 pixel wide line
	cairo_set_source_rgb (cr, 1.0, 0.5, 0.0); // Orange

	float angle = (frame % 600) / 100.0; // Rotation animaton, rotate once every 10 seconds.
	cairo_arc(cr, offWidth/2, offHeight/2, offHeight/3, angle*M_PI, (angle-0.3)*M_PI);
	cairo_stroke(cr); // Stroke the arc with our 30px wide orange line

	cairo_destroy(cr); // Realease Surface
	cairo_surface_finish(surface);
	cairo_surface_destroy(surface); // Flush and destroy the cairo surface
}

void init_efb()
{
	printf("sizeof(efbConfig)=%d\n",(u32)(u64)sizeof(efbConfig));
	printf("sizeof(efbBuffer)=%d\n",(u32)(u64)sizeof(efbBuffer));

	u32 buffersize=offWidth*offHeight*4;
	printf("alloc buffers: %d\n",buffersize);
	offscreenBuffers[0] = (uint32_t*)memalign(128,buffersize);
	printf("osb1: 0x%08X\n",(u32)(u64)offscreenBuffers[0]);
	offscreenBuffers[1] = (uint32_t*)memalign(128,buffersize);
	printf("osb2: 0x%08X\n",(u32)(u64)offscreenBuffers[1]);

	efbBuffers[0] = (efbBuffer*)memalign(128,sizeof(efbBuffer));
	printf("efb1: 0x%08X\n",(u32)(u64)efbBuffers[0]);
	efbBuffers[1] = (efbBuffer*)memalign(128,sizeof(efbBuffer));
	printf("efb2: 0x%08X\n",(u32)(u64)efbBuffers[1]);

	for(int i=0;i<2;i++)
	{
		printf("init buffer %d\n",i);
		efbBuffers[i]->framebufferAddress=(u32)(u64)offscreenBuffers[i];
		efbBuffers[i]->paletteAddress=0;
		efbBuffers[i]->height=offHeight;
		efbBuffers[i]->width=offWidth;

		efbBuffers[i]->bytesPerPixel=2;
		efbBuffers[i]->rBits=5;
		efbBuffers[i]->rShift=11;
		efbBuffers[i]->gBits=6;
		efbBuffers[i]->gShift=5;
		efbBuffers[i]->bBits=5;
		efbBuffers[i]->bShift=0;
/*
		efbBuffers[i]->bytesPerPixel=4;
		efbBuffers[i]->rBits=8;
		efbBuffers[i]->rShift=16;
		efbBuffers[i]->gBits=8;
		efbBuffers[i]->gShift=8;
		efbBuffers[i]->bBits=8;
		efbBuffers[i]->bShift=0;
*/
	}

	efbConfig *conf=memalign(128,sizeof(efbConfig));
	//conf->width=128*10;//buffers[0]->width/2;
	//conf->height=128*6;//buffers[0]->height/2;
	conf->width=buffers[0]->width;
	conf->height=buffers[0]->height;
	efbD=efbInit(conf);
	printf("~efbInit(0x%08X)\n",(u32)(u64)efbD);
	//free(conf);

	printf("efb init finished\n");
}

s32 main(s32 argc, const char* argv[])
{
	printf("init_screen()\n");
	init_screen();

	PadInfo padinfo;
	PadData paddata;
	int i;
	
	printf("Initializing 6 SPUs... ");
	printf("%08x\n", lv2SpuInitialize(6, 5));

	printf("ioPadInit()\n");
	ioPadInit(7);
	printf("init_efb()\n");
	init_efb();

	long frame = 0; // To keep track of how many frames we have rendered.
	
	// Ok, everything is setup. Now for the main loop.
	while(1){
		printf("frame\n");
		// Check the pads.
		ioPadGetInfo(&padinfo);
		for(i=0; i<MAX_PADS; i++){
			if(padinfo.status[i]){
				ioPadGetData(i, &paddata);
				
				if(paddata.BTN_CROSS){
					return 0;
				}
			}
			
		}

		printf("waitFlip\n");
		waitFlip(); // Wait for the last flip to finish, so we can draw to the old buffer

		printf("drawFrame\n");
		if(1)
		{
			//drawFrame(buffers[currentBuffer], frame); // Draw into the unused buffer6
			drawFrame((buffer*)offscreenBuffers[0], frame); // Draw into the unused buffer
		}
		else
		{
			for(int xy=0;xy<offWidth*offHeight;xy++)
			{
				if(currentBuffer)
					offscreenBuffers[0][xy]=xy*2;//%offWidth;
				else
					offscreenBuffers[0][xy]=xy*2;//%offWidth;
			}
		}
		printf("efbBlitToScreen\n");
		efbBlitToScreen(efbD, buffers[currentBuffer]->ptr,efbBuffers[0]);
		printf("flip\n");
		flip(currentBuffer); // Flip buffer onto screen
		printf("currentBuffer\n");
		currentBuffer = !currentBuffer; 
		frame++;
		//if(frame>4)
		//	break;
	}
	efbShutdown(efbD);
	return 0;
}

