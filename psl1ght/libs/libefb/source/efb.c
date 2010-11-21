#include "efb_intern.h"
#include <efb/efb.h>

#include <psl1ght/lv2/spu.h>
#include <lv2/spu.h>
#include <malloc.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


//efbBuffer efbCreateBufferPalette2(u16 width, u16 height);
//efbBuffer efbCreateBufferPalette(u16 width, u16 height, void *frameBuffer, void *palette);

efbData *efbInit(u32 numSpus, efbConfig *config)
{
	printf("efbInit()\n");

	//some checks
	assert("efbConfig not power of 2" && ((sizeof(efbConfig)&(sizeof(efbConfig)-1))==0));
	assert("efbBuffer not power of 2" && ((sizeof(efbBuffer)&(sizeof(efbBuffer)-1))==0));
	assert(numSpus>0 && numSpus<6);

	efbData *ret=malloc(sizeof(efbData));
	printf("  efbdata: 0x%08X\n",(u32)(u64)ret);

	u32 i;
	for(i=0;i<numSpus;i++)
	{
		ret->spudata[i].spu=uploadSPUProgram();
		printf("  spu: %d\n",ret->spudata[i].spu);

		ret->spudata[i].initialized=startSPU(ret->spudata[i].spu);
		printf("  initialized: %X\n",(u32)(u64)ret->spudata[i].initialized);

		// magig number sent from spu on startup :P
		assert(ret->spudata[i].initialized==0xB1176060);
		ret->spudata[i].status=EFB_SPU_STATUS_IDLE;
	}
	ret->numSpus=numSpus;

	printf("  writing config: 0x%08X\n",(u32)(u64)config);
	efbUpdateConfig(ret,config);

	return ret;
}

void efbShutdown(efbData *efb)
{
	printf("sending quit...\n");

	u32 i;
	for(i=0;i<efb->numSpus;i++)
	{
		finishOp(i);
		u32 ret= spuWriteVerify(efb->spudata[i].spu,EFB_COMMAND_QUIT);//framebuffer
		printf("quit command sent: %d\n",ret);

		printf("Destroying SPU...\n");
		printf("%08x\n", lv2SpuRawDestroy(efb->spudata[i].spu));

		printf("Closing image...\n");
//		printf("%08x\n", sysSpuImageClose(&efb->image));

		efb->spudata[i].initialized=0;
		efb->spudata[i].spu=-1;
		efb->spudata[i].status=EFB_SPU_STATUS_INACTIVE;
	}

	free(efb);
}

void efbUpdateConfig(efbData *efb,efbConfig *config)
{
	u32 i;
	for(i=0;i<efb->numSpus;i++)
	{
		finishOp(i);
		printf("update config...\n");
		u32 ret= spuWriteVerify(efb->spudata[i].spu,EFB_COMMAND_UPDATE_CONFIG);//framebuffer
		printf("config command sent: %d, config: 0x%08X\n",ret,(u32)(u64)config);
		spuWrite(efb->spudata[i].spu,(u32)(u64)config);//framebuffer
		printf("waiting for accept...\n");
		u32 val=spuReadBlocking(efb->spudata[i].spu);
		printf("answer: 0x%08X\n",val);
		assert(val==EFB_RESPONSE_CONFIG_FINISHED);
	}
	efb->currentConfig=*config;
}

void efbBlitToScreen(efbData *efb, void *framebuffer, efbBuffer *buffer)
{
	u32 partsize=efb->currentConfig.height/efb->numSpus;
	u32 startLine=0;

	u32 i;
	for(i=0;i<efb->numSpus;i++)
	{
		finishOp(i);
		printf("efbBlitToScreen\n");
		//printf("  EFB_COMMAND_DRAW\n");
		u32 ret=spuWriteVerify(efb->spudata[i].spu,EFB_COMMAND_DRAW);
		//printf("  inform spu: %d\n",efb->spudata[i].spu);
		assert(ret==EFB_COMMAND_DRAW);
		//printf("  write fb address: 0x%08X\n",(u32)(u64)framebuffer);
		spuWrite(efb->spudata[i].spu,(u32)(u64)framebuffer);
		//printf("  write buffer address: 0x%08X\n",(u32)(u64)buffer);
		spuWrite(efb->spudata[i].spu,(u32)(u64)buffer);
		spuWrite(efb->spudata[i].spu,startLine);
		startLine+=partsize;
		if(i==efb->numSpus-1)
			startLine=efb->currentConfig.height;
		spuWrite(efb->spudata[i].spu,startLine/** /-partsize/2*/);
		efb->spudata[i].status=EFB_SPU_STATUS_DRAWING;
	}
	//	assert(val==EFB_RESPONSE_DRAW_FINISHED);

	//efbWaitForBlit(efb);
}

void efbWaitForBlit(efbData *efb)
{
	u32 i;
	for(i=0;i<efb->numSpus;i++)
	{
		if(efb->spudata[i].status!=EFB_SPU_STATUS_DRAWING)
			break;

		u32 result=-1;
		u32 numWaits=0;
		while(result!=EFB_RESPONSE_DRAW_FINISHED)
		{
			printf("  read result\n");
			//this should move to efbWaitForBlit
			result=spuReadBlocking(efb->spudata[i].spu);
			printf("  result: 0x%08X\n",result);
			numWaits++;
			if(numWaits>20)
				exit(0);
		}
		efb->spudata[i].status=EFB_SPU_STATUS_IDLE;
	}
}

void finishOp(u32 spu)
{
}
