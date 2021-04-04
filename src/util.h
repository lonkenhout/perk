#ifndef __PEARS_UTIL_H__
#define __PEARS_UTIL_H__


#include <getopt.h>

#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
//#include <netinet/in.h>
//#include <sys/socket.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#define PEARS_DEBUG (1)

#define OK (1)
#define TOO_LONG (2)

/* max expected request length and max number of requests
 * sent 
 */
#define MAX_LINE_LEN (200)
#define MAX_LINES (1)

/* debugging macros */
#define get_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(msg, ...) fprintf(stderr, "%s:%d:%s(): \n\terror (errno %d: %s): " msg "\n",\
				__FILE__, __LINE__, __func__, errno, get_errno(), ##__VA_ARGS__)


#ifdef PEARS_DEBUG
#define debug(msg, ...) fprintf(stdout, "[DEBUG] " msg, ##__VA_ARGS__);
#else
#define debug(msg, ...)
#endif

int get_line(char *buff, size_t max);
int get_file_line(FILE *input_file, char *buff, size_t max);

int get_addr(char *dst, struct sockaddr *addr);
int get_addr_port(char *res, struct sockaddr *addr);
int addr_eq(struct sockaddr *addr1, struct sockaddr *addr2);

#endif



