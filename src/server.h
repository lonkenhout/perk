#ifndef __PERK_SERVER_H__
#define __PERK_SERVER_H__

#include <stdio.h>

#include "./util.h"
#include "./rdma.h"
#include "./workers.h"
#include "./bm.h"

struct kv_pair {
	char key[MAX_KEY_SIZE];
	char val[MAX_VAL_SIZE];
};

#define DEFAULT_IPADDR	"127.0.0.1"
#define DEFAULT_PORT	(4944)

#define MAX_EVENTS (20)
#define POLL_TIMEOUT (0)

#define POLL_CLIENT_CONNECT_FAILED		(-1)
#define POLL_CLIENT_DISCONNECT_FAILED	(-2)

#define POLL_CLIENT_CONNECT_SUCCESS 	(1)
#define POLL_CLIENT_DISCONNECT_SUCCESS	(2)
#define POLL_CLIENT_CONNECT_ESTABLISHED (3)


void perk_kv_init(void);
void perk_kv_destroy(void);

#endif
