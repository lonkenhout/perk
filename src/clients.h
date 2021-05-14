#ifndef __PEARS_CLIENT_H__
#define __PEARS_CLIENT_H__

#include "util.h"
#include "client.h"
#include "rdma.h"
#include "bm.h"

int prep_request(PEARS_CLT_CTX *pcc, struct request *request);
int validate_request(int req, char *input);

int client(PEARS_CLT_CTX *pcc);

int client_wr_wr(PEARS_CLT_CTX *pcc);
int client_wr_sd(PEARS_CLT_CTX *pcc);
int client_sd_sd(PEARS_CLT_CTX *pcc);
int client_wrimm_sd(PEARS_CLT_CTX *pcc);

#endif
