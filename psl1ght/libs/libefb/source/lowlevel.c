#include "spu.bin.h"
#include "efb_intern.h"
#include <efb/efb.h>

#include <psl1ght/lv2/spu.h>
#include <lv2/spu.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>

void spuWrite(u32 spu,u32 val)
{
	lv2SpuRawWriteProblemStorage(spu,SPU_In_MBox,val );
}

u32 spuWriteVerify(u32 spu,u32 val)
{
	spuWrite(spu,val);

	return spuReadBlocking(spu);
}

u32 spuRead(u32 spu)
{
	while (!(lv2SpuRawReadProblemStorage(spu, 0x4014) & 0xFF)) {
		asm volatile("eieio" ::);
	}
	u32 ret=lv2SpuRawReadProblemStorage(spu, SPU_Out_MBox);
	//printf("SPU mailbox return value: 0x%08x\n", ret);

	return ret;
}

u32 spuReadBlocking(u32 spu)
{
	while (!(lv2SpuRawReadProblemStorage(spu, 0x4014) & 0xFF)) {
		asm volatile("eieio" ::);
	}
	u32 ret=lv2SpuRawReadProblemStorage(spu, SPU_Out_MBox);
	//printf("SPU mailbox return value: 0x%08x\n", ret);

	return ret;
}

u32 uploadSPUProgram()
{
	u32 spu=3;
	u32 entry = 0;
	u32 segmentcount = 0;
	sysSpuSegment* segments;
	sysSpuImage image;

	printf("Initializing raw SPU... ");
	printf("%08x\n", lv2SpuRawCreate(&spu, NULL));

	printf("Getting ELF information... ");
	printf("%08x\n", sysSpuElfGetInformation(spu_bin, &entry, &segmentcount));
	printf("\tEntry Point: %08x\n\tSegment Count: %08x\n", entry, segmentcount);

	size_t segmentsize = sizeof(sysSpuSegment) * segmentcount;
	segments = (sysSpuSegment*)malloc(segmentsize);
	memset(segments, 0, segmentsize);

	printf("Getting ELF segments... ");
	printf("%08x\n", sysSpuElfGetSegments(spu_bin, segments, segmentcount));

	printf("Loading ELF image... ");
	printf("%08x\n", sysSpuImageImport(&image, spu_bin, 0));

/*	image.type = SYS_SPU_IMAGE_TYPE_USER;
	image.entryPoint = entry;
	image.segments = (u32)(u64)segments;
	image.segmentCount = segmentcount; */

	printf("Loading image into SPU... ");
	printf("%08x\n", sysSpuRawImageLoad(spu, &image));

	return spu;
}

u32 startSPU(u32 spu)
{
	printf("Running SPU...\n");
	lv2SpuRawWriteProblemStorage(spu, SPU_RunCntl, 1);

	printf("Waiting for SPU to start...\n");
	u32 ret=spuReadBlocking(spu);

	printf("SPU running!\n");

	return ret;
}
