#ifndef __PEARS_SERVER_H__
#define __PEARS_SERVER_H__

#include <stdio.h>
//#include <sys/epoll.h>


#include <glib.h>

#include "./util.h"
#include "./rdma.h"

#define MAX_KV_ENTRIES (10000)

struct kv_pair {
	char key[MAX_KEY_SIZE];
	char val[MAX_VAL_SIZE];
};

typedef struct kv_cache {
	struct kv_pair 	pairs[MAX_KV_ENTRIES];
	int 			count;
} KV_CACHE;

#define DEFAULT_IPADDR	"127.0.0.1"
#define DEFAULT_PORT	(4944)


#define MAX_EVENTS (20)
#define POLL_TIMEOUT (0)

#define POLL_CLIENT_CONNECT_FAILED		(-1)
#define POLL_CLIENT_DISCONNECT_FAILED	(-2)

#define POLL_CLIENT_CONNECT_SUCCESS 	(1)
#define POLL_CLIENT_DISCONNECT_SUCCESS	(2)


#endif
