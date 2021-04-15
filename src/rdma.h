/* Server header */

#ifndef __PEARS_RDMA_H__
#define __PEARS_RDMA_H__

#include "./util.h"

#include <stdio.h>
#include <stdlib.h>

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

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

typedef struct pears_server_context{
	struct sockaddr_in			server_sa;
	
	struct rdma_event_channel	*cm_ec;
	struct rdma_cm_id			*cm_sid;
	
	struct ibv_context 			*ctx;
	struct ibv_pd 				*pd;
	struct ibv_comp_channel		*comp_channel;
	struct ibv_cq				*cq;
	struct ibv_qp_init_attr 	qp_init_attr;
	
} PEARS_SVR_CTX;

typedef struct pears_client_context{
	struct sockaddr_in			client_sa;
	
	struct rdma_event_channel	*cm_ec;
	struct rdma_cm_id			*cm_cid;
	

	struct ibv_pd 				*pd;
	struct ibv_comp_channel		*comp_channel;
	struct ibv_cq				*cq;
	struct ibv_qp				*qp;
	struct ibv_qp_init_attr 	qp_init_attr;
	
	/* local memory properties */
	char						*kvs_request;
	struct ibv_mr				*kvs_request_mr;
	struct rdma_buffer_attr		kvs_request_attr;
	struct ibv_mr				*kvs_request_attr_mr;

	/* stores server memory properties */
	struct rdma_buffer_attr		server_md_attr;
	struct ibv_mr				*server_md_mr;

	char						*response;
	struct ibv_mr				*response_mr;

	/* request util */
	struct ibv_sge				rec_sge;
	struct ibv_sge				snd_sge;
} PEARS_CLT_CTX;

typedef struct pears_client_conn{
	struct sockaddr_in			client_sa;
	struct sockaddr				*addr;
	struct rdma_cm_id			*cm_cid;

	struct ibv_pd				*pd;
	struct ibv_qp				*qp;
	struct ibv_cq				*cq;
	struct ibv_comp_channel		*io_cc;
	struct ibv_qp_init_attr 	qp_init_attr;

	struct ibv_mr				*md;
	struct rdma_buffer_attr		md_attr;
	struct ibv_sge				rec_sge;
	struct ibv_sge				snd_sge;

	struct ibv_mr				*server_buf;
	struct ibv_mr				*server_md;
	struct rdma_buffer_attr		server_md_attr;

	struct ibv_mr				*response_mr;
} PEARS_CLIENT_CONN;


#define MAX_CLIENTS (8)
typedef struct pears_client_collection{
	int active[MAX_CLIENTS];
	PEARS_CLIENT_CONN clients[MAX_CLIENTS];
} PEARS_CLIENT_COLL;



#define MAX_CQ_SIZE (256)

#define MAX_CLIENT_BACKLOG (8)
#define MAX_SGE (2)
#define MAX_WR (8)


#define REQ_NOTIFY_FILTER (0)

/* some permissions for the shared mem, local read is enabled by default */
#define PERM_L_RW (IBV_ACCESS_LOCAL_WRITE) 
#define PERM_R_W (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE)
#define PERM_R_RW (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ)
#define PERM_RA_W (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC)
#define PERM_RA_RW (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC)

/* server side functions */
int init_server_dev(PEARS_SVR_CTX *psc);
int wait_for_client_conn(PEARS_SVR_CTX *psc, PEARS_CLIENT_CONN *pc_conn);
int init_server_client_resources(PEARS_CLIENT_CONN *pc_conn);
int accept_client_conn(PEARS_SVR_CTX *psc, PEARS_CLIENT_CONN *pc_conn);
int disconnect_client_conn(PEARS_SVR_CTX *psc, PEARS_CLIENT_CONN *pc_conn);
int destroy_server_dev(PEARS_SVR_CTX *psc);
int send_md_s2c(PEARS_CLIENT_CONN *pc_conn);

int client_coll_find_free(PEARS_CLIENT_COLL *conns);
int client_coll_find_conn(PEARS_CLIENT_COLL *conns, struct sockaddr *addr);

/* client side functions */
int init_client_dev(PEARS_CLT_CTX *pcc, struct sockaddr_in *svr_sa);
int client_pre_post_recv_buffer(PEARS_CLT_CTX *pcc);
int connect_to_server(PEARS_CLT_CTX *pcc);
int send_md_c2s(PEARS_CLT_CTX *pcc);
int rdma_write_c2s(PEARS_CLT_CTX *pcc);
int rdma_write_c2s_non_block(PEARS_CLT_CTX *pcc);
int client_disconnect(PEARS_CLT_CTX *pcc);

/* shared functions */
int rdma_post_recv(struct ibv_mr *mr, struct ibv_qp *qp);
int rdma_post_send(struct ibv_mr *mr, struct ibv_qp *qp);

int rdma_clear_cq(struct ibv_cq *cq);

struct ibv_mr *rdma_buffer_register(struct ibv_pd *pd, void *addr, uint32_t length, enum ibv_access_flags perm);

int rdma_cm_event_rcv(struct rdma_event_channel *ec,
					enum rdma_cm_event_type e_type,
					struct rdma_cm_event **cme);
int rdma_cm_event_rcv_any(struct rdma_event_channel *ec,
					struct rdma_cm_event **cme);
int retrieve_work_completion_events(struct ibv_comp_channel *cc, struct ibv_wc *wc, int max_wc);
int validate_wcs(struct ibv_wc *wc, int tot);
struct ibv_mr* rdma_buffer_alloc(struct ibv_pd *pd, uint32_t len, enum ibv_access_flags perm);
struct ibv_mr *rdma_buffer_register(struct ibv_pd *pd, void *addr, uint32_t len, enum ibv_access_flags perm);
void rdma_buffer_free(struct ibv_mr *mr);
void rdma_buffer_deregister(struct ibv_mr *mr);

int set_comp_channel_non_block(struct ibv_comp_channel *io_cc);

/* extra stuff */
void print_ibv_devs(void);

/* util from other repos */
void show_rdma_buffer_attr(struct rdma_buffer_attr *attr);

#endif
