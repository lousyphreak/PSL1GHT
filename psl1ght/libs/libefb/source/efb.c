#include "efb_intern.h"
#include <efb/efb.h>

#include <psl1ght/lv2/spu.h>
#include <lv2/spu.h>
#include <malloc.h>
#include <assert.h>
#include <stdio.h>


//efbBuffer efbCreateBufferPalette2(u16 width, u16 height);
//efbBuffer efbCreateBufferPalette(u16 width, u16 height, void *frameBuffer, void *palette);

u32 spu;
u32 efbinitialized;
efbData *efbInit(efbConfig *config)
{
	printf("efbInit()\n");
	efbData *ret=malloc(1024/*sizeof(efbData)*/);
	printf("  efbdata: 0x%08X\n",(u32)(u64)ret);

	spu=uploadSPUProgram();
	printf("  spu: %d\n",spu);

	efbinitialized=startSPU(spu);
	printf("  initialized: %X\n",(u32)(u64)efbinitialized);

	// magig number sent from spu on startup :P
	assert(efbinitialized==0xB1176060);

	printf("  writing config: 0x%08X\n",(u32)(u64)config);
	efbUpdateConfig(ret,config);

	return ret;
}

void efbShutdown(efbData *efb)
{
	printf("Destroying SPU...\n");
	printf("%08x\n", lv2SpuRawDestroy(efb->spu));

	printf("Closing image...\n");
//	printf("%08x\n", sysSpuImageClose(&efb->image));

	efb->initialized=0;
	efb->spu=-1;
	free(efb);
}

void efbUpdateConfig(efbData *efb,efbConfig *config)
{
	printf("update config...\n");
	u32 ret= spuWriteVerify(spu,EFB_COMMAND_UPDATE_CONFIG);//framebuffer
	printf("config command sent: %d, config: 0x%08X\n",ret,(u32)(u64)config);
	spuWrite(spu,(u32)(u64)config);//framebuffer
	printf("waiting for accept...\n");
	u32 val=spuReadBlocking(spu);
	printf("answer: 0x%08X\n",val);
	assert(val==EFB_RESPONSE_CONFIG_FINISHED);
}

void efbBlitToScreen(efbData *efb, void *framebuffer, efbBuffer *buffer)
{
	printf("efbBlitToScreen\n");
	printf("  EFB_COMMAND_DRAW\n");
	u32 ret=spuWriteVerify(spu,EFB_COMMAND_DRAW);
	printf("  result: 0x%08X\n",ret);
	assert(ret==EFB_COMMAND_DRAW);
	printf("  write fb address: 0x%08X\n",(u32)(u64)framebuffer);
	spuWrite(spu,(u32)(u64)framebuffer);
	printf("  write buffer address: 0x%08X\n",(u32)(u64)buffer);
	spuWrite(spu,(u32)(u64)buffer);

	u32 result=-1;
	while(result!=EFB_RESPONSE_DRAW_FINISHED)
	{
		printf("  read result\n");
		//this should move to efbWaitForBlit
		result=spuReadBlocking(spu);
		printf("  result: 0x%08X\n",result);
	}
//	assert(val==EFB_RESPONSE_DRAW_FINISHED);
}

void efbWaitForBlit(efbData *efb)
{
}
