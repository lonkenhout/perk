#ifndef __PEARS_SERVER_H__
#define __PEARS_SERVER_H__

#include <stdio.h>

#include <glib.h>

#include "./util.h"
#include "./rdma.h"
#include "./workers.h"

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

void *worker(void *args);

void pears_kv_init(void);
void pears_kv_destroy(void);
void pears_kv_insert(void *key, void *val);
char *pears_kv_get(void *key);

#endif
