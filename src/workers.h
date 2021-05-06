#ifndef __PEARS_WORKERS_H__
#define __PEARS_WORKERS_H__

#include "util.h"
#include "rdma.h"
#include "server.h"

void *worker_wr_sd(void *args);
void *worker_wrimm_sd(void *args);
void *worker_sd_sd(void *args);
void *worker_wr_wr(void *args);
void *worker_wr_rd(void *args);

#endif
