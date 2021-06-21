#ifndef __PERK_SERVER_H__
#define __PERK_SERVER_H__

#include <stdio.h>

#include "./util.h"
#include "./rdma.h"
#include "./workers.h"
#include "./bm.h"

struct kv_pair {
	char key[MAX_KEY_SIZE];
	char val[MAX_VAL_SIZE];
};

#define DEFAULT_IPADDR	"127.0.0.1"
#define DEFAULT_PORT	(4944)

#define MAX_EVENTS (20)
#define POLL_TIMEOUT (0)

#define POLL_CLIENT_CONNECT_FAILED		(-1)
#define POLL_CLIENT_DISCONNECT_FAILED	(-2)

#define POLL_CLIENT_CONNECT_SUCCESS 	(1)
#define POLL_CLIENT_DISCONNECT_SUCCESS	(2)
#define POLL_CLIENT_CONNECT_ESTABLISHED (3)

/* print help */
void print_usage(char *cmd);

/* parse server-side command line arguments */
int parse_opts(int argc, char **argv);

/* initialize hash table */
void perk_kv_init(void);

/* destroy hash table */
void perk_kv_destroy(void);

/* sets up basic resources for a single client i.e. protection domain, queue pairs,
 * completion queues, ibv_mr's etc. */
int process_connect_req(PERK_SVR_CTX *psc, PERK_CLIENT_CONN *pc_conn);

/* finalizes the connection and does any information exchange between server and client */
int process_established_req(PERK_SVR_CTX *psc, PERK_CLIENT_CONN *pc_conn);

/* cleans up a client's resources after it disconnected */
int process_disconnect_req(PERK_SVR_CTX *psc, PERK_CLIENT_CONN *pc_conn);

/* process a connection management event, if its a valid CM event, a client
 * may be added to conns (the client table), or a thread may be started for
 * a client that is now fully connected */
int process_cm_event(PERK_SVR_CTX *psc, PERK_CLIENT_COLL *conns);

/* this is the main event loop server side, it processes connection and dispatches threads */
void server(PERK_SVR_CTX *psc, PERK_CLIENT_COLL *conns);

#endif
