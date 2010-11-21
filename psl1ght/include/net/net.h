#pragma once

#include <psl1ght/types.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <stdlib.h>

#define libnet_errno *_net_errno_loc()

#define net_initialize_network() ({ \
	static char __net_buffer[128 * 1024]; \
	net_init_param_t __net_param; \
	__net_param.memory = (u32)(u64)&__net_buffer; \
	__net_param.memory_size = sizeof(__net_buffer); \
	__net_param.flags = 0; \
	net_initialize_network_ex(&__net_param); \
})

typedef struct net_init_param {
	u32 memory;
	s32 memory_size;
	s32 flags;
} net_init_param_t;

typedef struct net_sockinfo {
	s32 s;
	s32 proto;
	s32 recv_queue_len;
	s32 send_queue_len;
	u32 local_adr;
	s32 local_port;
	u32 remote_adr;
	s32 remote_port;
	s32 state;
} net_sockinfo_t;

EXTERN_BEGIN

s32* _net_errno_loc(void);


s32 net_finalize_network();
s32 net_get_sockinfo(s32 s, net_sockinfo_t* p, s32 n);
s32 net_initialize_network_ex(net_init_param_t* param);
s32 net_show_ifconfig();
s32 net_show_nameserver();
s32 net_show_route();

EXTERN_END
