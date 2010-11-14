#include "main.h"


efbConfig _config __attribute__((aligned(128)));

u32 spuReadVerify()
{
	u32 message=spu_readch(SPU_RdInMbox);
	spu_writech(SPU_WrOutMbox, message);

	return message;
}


void main()
{
	//init
	spu_writech(SPU_WrOutMbox, 0xB1176060);

	u16 running=1;

	while(running)
	{
		u32 command=spuReadVerify();

		switch(command)
		{
		case EFB_COMMAND_DRAW:
			draw();
			//send finished
			spu_writech(SPU_WrOutMbox, EFB_RESPONSE_DRAW_FINISHED);
			break;
		case EFB_COMMAND_UPDATE_CONFIG:
			check_config();
			//send finished
			spu_writech(SPU_WrOutMbox, EFB_RESPONSE_CONFIG_FINISHED);
			break;
		case EFB_COMMAND_QUIT:
			running=0;
			break;
		default:
			spu_writech(SPU_WrOutMbox, 0xFA110000|command);
		}
	}
	return;
}
