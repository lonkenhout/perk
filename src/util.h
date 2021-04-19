#ifndef __PEARS_UTIL_H__
#define __PEARS_UTIL_H__


#include <getopt.h>

#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <time.h>
//#include <netinet/in.h>
//#include <sys/socket.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

enum REQUEST_TYPE {
	GET,
	PUT,
	EMPTY,
	MALFORMED
};

//#define PEARS_DEBUG (1)

#define SCALE_SEC	(1.0)
#define SCALE_MSEC	(1000.0)
#define SCALE_MCSEC	(1000000.0)

#define OK (1)
#define TOO_LONG (2)

/* max expected request length and max number of requests
 * sent 
 */
#define MAX_KEY_SIZE (20)
#define MAX_VAL_SIZE (180)
#define MAX_LINE_LEN (MAX_KEY_SIZE+MAX_VAL_SIZE)
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

int parse_get_request(char *request, 
				char *k, size_t k_sz);
int parse_put_request(char *request, 
				char *k, size_t k_sz,
				char *v, size_t v_sz);
int parse_request(char *request, 
				char *k, size_t k_sz,
				char *v, size_t v_sz);


int get_line(char *buff, size_t max);
int get_file_line(FILE *input_file, char *buff, size_t max);

int get_addr(char *dst, struct sockaddr *addr);
int get_addr_port(char *res, struct sockaddr *addr);
int addr_eq(struct sockaddr *addr1, struct sockaddr *addr2);

void get_time(struct timeval *t);
double compute_time(struct timeval start, struct timeval end, double scale);
void print_curr_time(void);
#endif



