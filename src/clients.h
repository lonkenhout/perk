#ifndef __PERK_CLIENT_H__
#define __PERK_CLIENT_H__

#include "util.h"
#include "client.h"
#include "rdma.h"
#include "bm.h"

/* prepares a work request for client->server depending on the type of communication */
int prepare_request_side(PERK_CLT_CTX *pcc);

/* prepares potential work request server->client depending on the type of communication */
int prepare_response_side(PERK_CLT_CTX *pcc);

/* reads a request from stdin/file and places it in request,
 * alternatively if neither of these is used, a default request is copied
 * into the struct */
int prep_request(PERK_CLT_CTX *pcc, struct request *request);

/* send a single request based on the type of communication */
int send_request(PERK_CLT_CTX *pcc);

/* receive a response from the server (i.e. wait until the response is complete), 
 * if using READ to retrieve the response, this function will perform READs until
 * the response has been READ successfully from the server's memory */
int recv_response(PERK_CLT_CTX *pcc);

/* sets relevant memory to fresh values based on the configuration */
int prep_next_iter(PERK_CLT_CTX *pcc);

/* validate that a read request is a PUT, GET or EXIT request */
int validate_request(int req, char *input);

/* this function actually performs all the above functionality, 
 * it runs in a loop, sending requests and receiving responses until pcc->max_reqs is reached */
int client(PERK_CLT_CTX *pcc);

#endif
