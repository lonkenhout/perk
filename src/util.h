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
	NOTHING = 0,
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
#define SCALE_NSEC	(1000000000.0)

#define OK (1)
#define TOO_LONG (2)

/* max expected request length and max number of requests
 * sent 
 */
#define MAX_KEY_SIZE (8)

#ifdef PERK_OVERRIDE_VALSIZE
#define MAX_VAL_SIZE (PERK_OVERRIDE_VALSIZE)

#else
#define MAX_VAL_SIZE (23)
#endif

#define MAX_LINES (1)

struct request{
	uint64_t rid;
	char key[MAX_KEY_SIZE];
	char val[MAX_VAL_SIZE];
	enum REQUEST_TYPE type; 
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


/* parse a get request from a raw string into a key buffer */
int parse_get_request(char *request, 
				char *k, size_t k_sz);

/* parse a put request from a raw string into key-value buffers */
int parse_put_request(char *request, 
				char *k, size_t k_sz,
				char *v, size_t v_sz);

/* check request type of raw buffer and parse the request based on that */
int parse_request(char *request, 
				char *k, size_t k_sz,
				char *v, size_t v_sz);

/* get a line from stdin */
int get_line(char *buff);

/* get a line from input_file and place it in *buff */
int get_file_line(FILE *input_file, char **buff);

/* convert readable ip address to sockaddr */
int get_addr(char *dst, struct sockaddr *addr);

/* convert sockaddr to readable ip address */
int get_addr_port(char *res, struct sockaddr *addr);

/* checks if two sockaddr's are equal, returns 0 if equal, 1 otherwise */
int addr_eq(struct sockaddr *addr1, struct sockaddr *addr2);

/* store wallclocktime in t */
void get_time(struct timeval *t);

/* print the time difference between two wallclock measurements in microseconds */
void print_time_diff(char *msg, struct timeval t_s, struct timeval t_e);

/* print the time difference between two wallclock measurements in nanoseconds */
void print_time_diff_nano(char *msg, struct timeval t_s, struct timeval t_e);

/* print the number of operations per second given two wallclock measurements */
void print_ops_per_sec(uint64_t ops, struct timeval t_s, struct timeval t_e);

/* increment integer pointed to by c */
void incr_num(uint64_t *c);

/* convert req type enum to string */
char *req_type_str(enum REQUEST_TYPE type);

/* print a request */
void print_request(struct request req, struct request res);

/* compute the time between start and end on the given scale */
double compute_time(struct timeval start, struct timeval end, double scale);

/* debug function used for printing the current time */
void print_curr_time(void);

/* macro for printing requests that can be enabled through environment variables
 * passed to cmake when generating build files */
#ifdef PERK_PRINT_REQUESTS
#define print_req(req, res) print_request(req, res);
#else
#define print_req(req, res)
#endif

#endif



