#ifndef __PERK_WORKERS_H__
#define __PERK_WORKERS_H__

#include "util.h"
#include "rdma.h"
#include "server.h"

/* process a received request */
int handle_request(PERK_CLIENT_CONN *pcc, struct request *request, struct request *response);

/* process a get request -> retrieve value from hash table */
void handle_request_get(PERK_CLIENT_CONN *pcc, struct request *request, struct request *response);

/* process a put request -> insert pair into hash table */
void handle_request_put(PERK_CLIENT_CONN *pcc, struct request *request, struct request *response);

/* process exit request */
void handle_request_exit(struct request *request, struct request *response);

/* prepares a work request for server->client depending on the server configuration */
int prepare_response_server(PERK_CLIENT_CONN *pcc);

/* prepares a work request for client->server depending on the client configuration */
int prepare_request_server(PERK_CLIENT_CONN *pcc);

/* receive a single request from the client -> wait until a request is available */
int recv_request(PERK_CLIENT_CONN *pcc);

/* send a single response to the client */
int send_response(PERK_CLIENT_CONN *pcc);

/* prepares the next iteration: prepost RECVs in case of WRIMM or SEND on the client-side */
int prep_next_iter(PERK_CLIENT_CONN *pcc);

/* the is the function passed to pthread, communicates with the client until an exit request
 * is sent, then it exits */
void *worker(void *args);


#endif
