#ifndef __PERK_CLIENT_H__
#define __PERK_CLIENT_H__

#include "util.h"
#include "client.h"
#include "rdma.h"
#include "bm.h"

int prep_request(PERK_CLT_CTX *pcc, struct request *request);
int validate_request(int req, char *input);

int client(PERK_CLT_CTX *pcc);

int client_wr_wr(PERK_CLT_CTX *pcc);
int client_wr_sd(PERK_CLT_CTX *pcc);
int client_sd_sd(PERK_CLT_CTX *pcc);
int client_wrimm_sd(PERK_CLT_CTX *pcc);

#endif
