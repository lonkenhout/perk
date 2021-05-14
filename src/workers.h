#ifndef __PEARS_WORKERS_H__
#define __PEARS_WORKERS_H__

#include "util.h"
#include "rdma.h"
#include "server.h"


int handle_request(PEARS_CLIENT_CONN *pcc, struct request *request, struct request *response);
void handle_request_get(struct request *request, struct request *response);
void handle_request_put(struct request *request, struct request *response);
void handle_request_exit(struct request *request, struct request *response);

void *worker(void *args);

void *worker_wr_sd(void *args);
void *worker_wrimm_sd(void *args);
void *worker_sd_sd(void *args);
void *worker_wr_wr(void *args);
void *worker_wr_rd(void *args);

#endif
