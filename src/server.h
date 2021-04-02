#ifndef __PEARS_SERVER_H__
#define __PEARS_SERVER_H__

#include <stdio.h>
#include <sys/epoll.h>

#include <glib.h>

#include "./util.h"
#include "./rdma.h"

#define DEFAULT_IPADDR	"127.0.0.1"
#define DEFAULT_PORT	(4944)


#define MAX_EVENTS (20)

#define POLL_CLIENT_CONNECT_FAILED		(-1)
#define POLL_CLIENT_DISCONNECT_FAILED	(-2)

#define POLL_CLIENT_CONNECT_SUCCESS 	(1)
#define POLL_CLIENT_DISCONNECT_SUCCESS	(2)


#endif
