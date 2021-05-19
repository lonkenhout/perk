#ifndef __PERK_UTIL_H__
#define __PERK_UTIL_H__


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
#include <pthread.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>


enum __attribute__ ((__packed__)) REQUEST_TYPE {
	GET,
	PUT,
	RESPONSE_OK,
	RESPONSE_EMPTY,
	RESPONSE_ERR,
	EMPTY,
	EXIT,
	EXIT_OK,
	MALFORMED
};


#define SCALE_SEC	(1.0)
#define SCALE_MSEC	(1000.0)
#define SCALE_MCSEC	(1000000.0)

#define OK (1)
#define TOO_LONG (2)

/* max expected request length and max number of requests
 * sent 
 */
#define MAX_KEY_SIZE (8)
#define MAX_VAL_SIZE (21)
#define MAX_LINES (1)

struct request{
	enum REQUEST_TYPE type; 
	char key[MAX_KEY_SIZE];
	char val[MAX_VAL_SIZE];
};

/* debugging macros */
#define get_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(msg, ...) fprintf(stderr, "%s:%d:%s(): \n\terror (errno %d: %s): " msg "\n",\
				__FILE__, __LINE__, __func__, errno, get_errno(), ##__VA_ARGS__)


#ifdef PERK_DEBUG
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


int get_line(char *buff);
int get_file_line(FILE *input_file, char *buff);

int get_addr(char *dst, struct sockaddr *addr);
int get_addr_port(char *res, struct sockaddr *addr);
int addr_eq(struct sockaddr *addr1, struct sockaddr *addr2);

void get_time(struct timeval *t);
void print_time_diff(char *msg, struct timeval t_s, struct timeval t_e);
void print_ops_per_sec(uint64_t ops, struct timeval t_s, struct timeval t_e);

char *req_type_str(enum REQUEST_TYPE type);
void print_request(struct request req, struct request res);
double compute_time(struct timeval start, struct timeval end, double scale);
void print_curr_time(void);

#ifdef PERK_PRINT_REQUESTS
#define print_req(req, res) print_request(req, res);
#else
#define print_req(req, res)
#endif

#endif



