#include "main.h"


void check_config()
{
	u32 newconfig=spu_readch(SPU_RdInMbox);

	int tag = 1, tag_mask = 1<<tag;

	mfc_get(&_config, newconfig, sizeof(_config),tag,0,0);
	mfc_write_tag_mask(tag_mask);
	mfc_read_tag_status_all();
}
