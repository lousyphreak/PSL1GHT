#include "efb_intern.h"
#include <efb/efb.h>

#include <psl1ght/lv2/spu.h>
#include <lv2/spu.h>
#include <malloc.h>
#include <assert.h>
#include <stdio.h>


//efbBuffer efbCreateBufferPalette2(u16 width, u16 height);
//efbBuffer efbCreateBufferPalette(u16 width, u16 height, void *frameBuffer, void *palette);


efbData *efbInit(efbConfig *config)
{
	printf("efbInit()\n");
	efbData *ret=malloc(sizeof(efbData));
	printf("  efbdata: 0x%08X\n",(u32)(u64)ret);

	ret->spu=uploadSPUProgram();
	printf("  spu: %d\n",ret->spu);

	ret->initialized=startSPU(ret->spu);
	printf("  initialized: %X\n",(u32)(u64)ret->initialized);

	// magig number sent from spu on startup :P
	assert(ret->initialized==0xB1176060);

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
	u32 ret= spuWriteVerify(efb->spu,EFB_COMMAND_UPDATE_CONFIG);//framebuffer
	printf("config command sent: %d\n",ret);
	spuWrite(efb->spu,(u32)(u64)config);//framebuffer
	printf("waiting for accept...\n");
	u32 val=spuReadBlocking(efb->spu);
	printf("answer: 0x%08X\n",val);
	assert(val==EFB_RESPONSE_CONFIG_FINISHED);
}

void efbBlitToScreen(efbData *efb, void *framebuffer, efbBuffer *buffer)
{
	spuWriteVerify(efb->spu,EFB_COMMAND_DRAW);
	spuWrite(efb->spu,(u32)(u64)framebuffer);
	spuWrite(efb->spu,(u32)(u64)buffer);

	//this should move to efbWaitForBlit
	u32 val=spuReadBlocking(efb->spu);
	assert(val==EFB_RESPONSE_DRAW_FINISHED);
}

void efbWaitForBlit(efbData *efb)
{
}
