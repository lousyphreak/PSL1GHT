#pragma once

#include <psl1ght/types.h>

#define SPU_Out_Intr_MBox 0x4000
#define SPU_Out_MBox      0x4004
#define SPU_In_MBox       0x400C
#define SPU_MBox_Status   0x4014
#define SPU_RunCntl       0x401C
#define SPU_Status        0x4024

#define DMA_MEMALIGN 16

#define EFB_COMMAND_DRAW  0
#define EFB_COMMAND_UPDATE_CONFIG 1

#define EFB_RESPONSE_DRAW_FINISHED 1
#define EFB_RESPONSE_CONFIG_FINISHED 2

void spuWrite(u32 spu,u32 val);
u32 spuWriteVerify(u32 spu,u32 val);
u32 spuRead(u32 spu);
u32 spuReadBlocking(u32 spu);
u32 uploadSPUProgram();
u32 startSPU(u32 spu);
