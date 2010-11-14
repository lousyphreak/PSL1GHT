#pragma once

#include <psl1ght/types.h>

#define DMA_MEMALIGN 16

#define EFB_COMMAND_DRAW  1
#define EFB_COMMAND_UPDATE_CONFIG 2
#define EFB_COMMAND_QUIT 3

#define EFB_RESPONSE_DRAW_FINISHED 0x11102221
#define EFB_RESPONSE_CONFIG_FINISHED 2

void spuWrite(u32 spu,u32 val);
u32 spuWriteVerify(u32 spu,u32 val);
u32 spuRead(u32 spu);
u32 spuReadBlocking(u32 spu);
u32 uploadSPUProgram();
u32 startSPU(u32 spu);
