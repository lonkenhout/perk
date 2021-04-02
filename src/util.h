#ifndef __PEARS_UTIL_H__
#define __PEARS_UTIL_H__


#include <getopt.h>

#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
//#include <netinet/in.h>
//#include <sys/socket.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define PEARS_DEBUG 1

/* debugging macros */
#define get_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(msg, ...) fprintf(stderr, "%s:%d:%s(): \n\terror (errno %d: %s): " msg "\n",\
				__FILE__, __LINE__, __func__, errno, get_errno(), ##__VA_ARGS__)


#ifdef PEARS_DEBUG
#define debug(msg, ...) fprintf(stdout, "[DEBUG] " msg, ##__VA_ARGS__);
#else
#define debug(msg, ...)
#endif



#endif



