/* Server header */

#ifndef __PERK_RDMA_H__
#define __PERK_RDMA_H__

#include "./util.h"

#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <semaphore.h>

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

/* attribute from code example: https://github.com/animeshtrivedi/rdma-example */
struct __attribute((packed)) rdma_buffer_attr {
  uint64_t address;
  uint32_t length;
  union stag {
	  /* if we send, we call it local stags */
	  uint32_t local_stag;
	  /* if we receive, we call it remote stag */
	  uint32_t remote_stag;
  }stag;
};

/* rdma setup for client- or server-side */
enum RDMA_COMBINATION {
	RDMA_COMBO_WR,
	RDMA_COMBO_WRIMM,
	RDMA_COMBO_SD,
	RDMA_COMBO_RD,
};

/* structure for managing configuration internally */
struct verb_rdma_config{
	enum RDMA_COMBINATION		client;
	enum RDMA_COMBINATION		server;
};

/* standard rdma configurations, determines how memory is registered */
static enum RDMA_COMBINATION default_client_rdma_config = RDMA_COMBO_WR;
static enum RDMA_COMBINATION default_server_rdma_config = RDMA_COMBO_SD;

/* server-side resources */
typedef struct perk_server_context{
	struct sockaddr_in			server_sa;
	
	struct rdma_event_channel	*cm_ec;
	struct rdma_cm_id			*cm_sid;
	
	struct ibv_context 			*ctx;
	struct ibv_pd 				*pd;
	struct ibv_comp_channel		*comp_channel;
	struct ibv_cq				*cq;
	struct ibv_qp_init_attr 	qp_init_attr;
	
	struct verb_rdma_config		config;
	
	uint32_t			total_ops;
} PERK_SVR_CTX;

/* very bulky struct for managing all client-side resources */
typedef struct perk_client_context{
	int rcv_ps;
	uint32_t cid;

	struct sockaddr_in			client_sa;
	
	struct rdma_event_channel	*cm_ec;
	struct rdma_cm_id			*cm_cid;
	

	struct ibv_pd 				*pd;
	struct ibv_comp_channel		*comp_channel;
	struct ibv_cq				*cq;
	struct ibv_qp				*qp;
	struct ibv_qp_init_attr 	qp_init_attr;
	
	struct verb_rdma_config		config;

	/* local memory properties */
	char						*raw_request;
	struct ibv_mr				*kvs_request_mr;
	struct rdma_buffer_attr		kvs_request_attr;
	struct ibv_mr				*kvs_request_attr_mr;

	/* stores server memory properties */
	struct rdma_buffer_attr		server_md_attr;
	struct ibv_mr				*server_md_mr;

	char						*response;
	struct ibv_mr				*response_mr;

	/* extra memory for read requests */
	struct rdma_buffer_attr		server_rd_md_attr;
	struct ibv_mr				*server_rd_md_mr;

	/* work request related stuff for request */
	struct ibv_sge				request_sge;
	struct ibv_send_wr			request_wr;

	struct request				sd_request;
	struct request				sd_response;

	struct ibv_mr				*sd_request_mr;
	struct ibv_mr				*sd_response_mr;

	/* work request related stuff for response*/
	struct ibv_sge				rec_sge;
	struct ibv_sge				snd_sge;
	struct ibv_sge				wr_sge;
	struct ibv_sge				rd_sge;

	struct ibv_recv_wr 			recv_wr;
	struct ibv_send_wr			send_wr;
	struct ibv_send_wr 			wr_wr;
	struct ibv_send_wr			rd_wr;

	FILE 						*f_ptr;
	int 						using_file;
	int 						max_reqs;
} PERK_CLT_CTX;

/* another bulky struct for managing a single connection server-side */
typedef struct perk_client_conn{
	struct sockaddr_in			client_sa;
	struct sockaddr				*addr;
	struct rdma_cm_id			*cm_cid;

	struct ck_hash_table		*ct;
	uint32_t					ops;

	struct ibv_pd				*pd;
	struct ibv_qp				*qp;
	struct ibv_cq				*cq;
	struct ibv_comp_channel		*io_cc;
	struct ibv_qp_init_attr 	qp_init_attr;
	
	struct verb_rdma_config		config;

	struct ibv_mr				*md;
	struct rdma_buffer_attr		md_attr;

	struct ibv_mr				*server_buf;
	struct ibv_mr				*server_md;
	struct rdma_buffer_attr		server_md_attr;
	
	struct rdma_buffer_attr		server_rd_md_attr;
	struct ibv_mr				*server_rd_md_mr;

	struct ibv_mr				*imm_data;
	
	struct ibv_mr				*sd_request_mr;
	struct ibv_mr				*sd_response_mr;
	struct request				sd_request;
	struct request				sd_response;

	struct ibv_sge				rec_sge;
	struct ibv_sge				snd_sge;
	struct ibv_sge				wr_sge;
	struct ibv_sge				rd_sge;

	struct ibv_recv_wr 			recv_wr;
	struct ibv_send_wr			send_wr;
	struct ibv_send_wr 			wr_wr;
	struct ibv_send_wr			rd_wr;
} PERK_CLIENT_CONN;

/* client table */
#define MAX_CLIENTS (64)
typedef struct perk_client_collection{
	int 					active[MAX_CLIENTS];
	int 					established[MAX_CLIENTS];
	PERK_CLIENT_CONN 		clients[MAX_CLIENTS];
	pthread_t				threads[MAX_CLIENTS];
} PERK_CLIENT_COLL;



#define MAX_CQ_SIZE (256)
#define MAX_IMM_SIZE (4)

#define MAX_CLIENT_BACKLOG (8)
#define MAX_SGE (2)
#define MAX_WR (8)


#define REQ_NOTIFY_FILTER (0)

/* some permissions for the shared memory, local read is enabled by default */
#define PERM_L_RW (IBV_ACCESS_LOCAL_WRITE) 
#define PERM_R_W (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE)
#define PERM_R_RW (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ)
#define PERM_RA_W (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC)
#define PERM_RA_RW (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC)

/*** server side functions ***/
/* create event channel, cid, bind to the address in psc and start listening */
int init_server_dev(PERK_SVR_CTX *psc);

/* allocates protection domain, completion channel and queue, queue pair, registers memory with RNIC */
int init_server_client_resources(PERK_CLIENT_CONN *pc_conn);

/* accepts a clients connection */
int accept_client_conn(PERK_SVR_CTX *psc, PERK_CLIENT_CONN *pc_conn);

/* copies peer address and prints the address:ip pair of the newly connected client */
int finalize_client_conn(PERK_CLIENT_CONN *pc_conn);

/* cleans up resources for a client table entry */
int disconnect_client_conn(PERK_SVR_CTX *psc, PERK_CLIENT_CONN *pc_conn);

/* destroy any leftover server resources */
int destroy_server_dev(PERK_SVR_CTX *psc);

/* sends WRITE/READ memory to the client */
int send_md_s2c(PERK_CLIENT_CONN *pc_conn);

/* find free entry in the client table */
int client_coll_find_free(PERK_CLIENT_COLL *conns);

/* find client table entry for specific client */
int client_coll_find_conn(PERK_CLIENT_COLL *conns, struct sockaddr *addr);

/*** client side functions ***/
/* initialize event channel, cid, request initial connection to server */
int init_client_dev(PERK_CLT_CTX *pcc, struct sockaddr_in *svr_sa);

/* pre post recv for exchanging WRITE/READ memory information */
int client_pre_post_recv_buffer(PERK_CLT_CTX *pcc);

/* establish connection with server */
int connect_to_server(PERK_CLT_CTX *pcc);

/* send metadata of WRITEable memory to server and receive metadata of server-side memory */
int send_md_c2s(PERK_CLT_CTX *pcc);

/* disconnect from the server and clean up resources */
int client_disconnect(PERK_CLT_CTX *pcc);

/* shared functions */
/* prepare somme initial attributes for a standard queue pair */
void qp_init_attr_prepare(struct ibv_qp_init_attr *qp_attr, struct ibv_cq *cq);

/* setup a metadata struct with the given ibv memory and register the metadata with the RNIC */
struct ibv_mr *setup_md_attr(struct ibv_pd *pd, struct rdma_buffer_attr *attr, struct ibv_mr *mr);

/* prepare a recv work request */
void rdma_recv_wr_prepare(struct ibv_recv_wr *wr, struct ibv_sge *sg, struct ibv_mr *mr);

/* post a pre-prepared recv work request */
int rdma_post_recv_reuse(struct ibv_recv_wr *wr, struct ibv_qp *qp);

/* prepare a send work request */
void rdma_send_wr_prepare(struct ibv_send_wr *wr, struct ibv_sge *sg, struct ibv_mr *mr);

/* post a pre-prepared send work request */
int rdma_post_send_reuse(struct ibv_send_wr *wr, struct ibv_qp *qp);

/* prepare a write with immediate work request */
void rdma_write_imm_wr_prepare(struct ibv_send_wr *wr, struct ibv_sge *sg, struct ibv_mr *mr, struct rdma_buffer_attr r_attr);

/* prepare  a write work request */
void rdma_write_wr_prepare(struct ibv_send_wr *wr, struct ibv_sge *sg, struct ibv_mr *mr, struct rdma_buffer_attr r_attr);

/* prepare a read work request */
void rdma_read_wr_prepare(struct ibv_send_wr *wr, struct ibv_sge *sg, struct ibv_mr *mr, struct rdma_buffer_attr r_attr);

/* post a pre-prepared WRITE/READ/WRIMM work request */
int rdma_post_write_reuse(struct ibv_send_wr *wr, struct ibv_qp *qp);

/* clear the completion queue (necessary for freeing) */
int rdma_clear_cq(struct ibv_cq *cq);

/* try receiving a specific connection management event */
int rdma_cm_event_rcv(struct rdma_event_channel *ec,
					enum rdma_cm_event_type e_type,
					struct rdma_cm_event **cme);

/* try receiving any connection management event */
int rdma_cm_event_rcv_any(struct rdma_event_channel *ec,
					struct rdma_cm_event **cme);

/* spin on the completion queue until the specific number of work completions are found */
int rdma_spin_cq(struct ibv_cq *cq, struct ibv_wc *wc, int max_wc);

/* poll on the completion queue until the specific number of work completions are found with timeout */
int rdma_poll_cq(struct ibv_cq *cq, struct ibv_wc *wc, int max_wc, useconds_t timeout);

/* validate work completions */
int validate_wcs(struct ibv_wc *wc, int tot);

/* allocate and register a memory buffer */
struct ibv_mr* rdma_buffer_alloc(struct ibv_pd *pd, uint32_t len, enum ibv_access_flags perm);

/* register a memory buffer */
struct ibv_mr *rdma_buffer_register(struct ibv_pd *pd, void *addr, uint32_t len, enum ibv_access_flags perm);

/* deregister and free a memory buffer */
void rdma_buffer_free(struct ibv_mr *mr);

/* deregister a memory buffer */
void rdma_buffer_deregister(struct ibv_mr *mr);

/* set completion channel to non-blocking mode */
int set_comp_channel_non_block(struct ibv_comp_channel *io_cc);

/* extra stuff */
void print_ibv_devs(void);

/* util from other repos */
void show_rdma_buffer_attr(struct rdma_buffer_attr *attr);

#endif
