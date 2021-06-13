#include "rdma.h"


struct ibv_context *init_ibv_dev(char *dev_name)
{
	struct ibv_context *ctx = NULL;
	int n_dev = 0, i;
	struct ibv_device** ibv_devs = ibv_get_device_list(&n_dev);
	if(dev_name != NULL) {
		for(i = 0; i < n_dev; ++i) {
			if(strncmp(ibv_devs[i]->dev_name, dev_name, IBV_SYSFS_NAME_MAX-1)) {
				ctx = ibv_open_device(ibv_devs[i]);
				break;
			}
		}
	} else{
		for(i = 0; i < n_dev; ++i) {
			ctx = ibv_open_device(ibv_devs[i]);
			if(ctx != NULL) break;
		}
	}
	ibv_free_device_list(ibv_devs);
	return ctx;
}

/**
 * Server functions
 *
 **/
int init_server_dev(PERK_SVR_CTX *psc)
{
	if(psc == NULL) {
		log_err("cant create server context");
		return 1;
	}

	int ret = -1;
	
	psc->cm_ec = rdma_create_event_channel();
	if(!psc->cm_ec) {
		log_err("rdma_create_event_channel() failed");
		return -errno;
	}
	debug("cm event channel created\n");
	
	ret = rdma_create_id(psc->cm_ec, &(psc->cm_sid), NULL, RDMA_PS_TCP);
	if(ret) {
		log_err("rdma_create_id() failed");
		return -errno;
	}
	debug("rdma id created\n");
	
	ret = rdma_bind_addr(psc->cm_sid, (struct sockaddr*)&(psc->server_sa));
	if(ret) {
		log_err("rdma_bind_addr() failed");
		return -errno;
	}
	debug("rdma id bound to ip\n");
	
	ret = rdma_listen(psc->cm_sid, MAX_CLIENT_BACKLOG);
	if(ret && errno == EADDRINUSE){
		debug("address already being listened on, attempting to listen anyways\n");
	} else if(ret) {
		log_err("rdma_listen() failed");
		return -errno;
	}
	printf("Server is listening on %s:%d\n", 
			inet_ntoa(psc->server_sa.sin_addr),
			ntohs(psc->server_sa.sin_port));
	
	return 0;
}

int init_server_client_resources(PERK_CLIENT_CONN *pc_conn)
{
	int ret = -1;
	if(!pc_conn->cm_cid) {
		errno = EINVAL;
		log_err("cm cid is NULL");
		return -EINVAL;
	}

	pc_conn->pd = ibv_alloc_pd(pc_conn->cm_cid->verbs);
	if(!pc_conn->cm_cid) {
		log_err("ibv_alloc_pd() failed");
		return -errno;
	}

	debug("protection domain allocated for %s:%d\n",
			inet_ntoa(pc_conn->client_sa.sin_addr),
			ntohs(pc_conn->client_sa.sin_port));

	pc_conn->io_cc = ibv_create_comp_channel(pc_conn->cm_cid->verbs);
	if(!pc_conn->io_cc) {
		log_err("ibv_create_comp_channel() failed");
		return -errno;
	}
	debug("completion channel created\n");

	pc_conn->cq = ibv_create_cq(pc_conn->cm_cid->verbs,
								MAX_CQ_SIZE,
								NULL,
								pc_conn->io_cc,
								0);
	if(!pc_conn->cq) {
		log_err("ibv_create_cq() failed");
		return -errno;
	}

	ret = ibv_req_notify_cq(pc_conn->cq, 0);
	if(ret) {
		log_err("ibv_req_notify_cq() failed");
		return -errno;
	}

	qp_init_attr_prepare(&(pc_conn->qp_init_attr), pc_conn->cq);
	ret = rdma_create_qp(pc_conn->cm_cid,
						pc_conn->pd,
						&(pc_conn->qp_init_attr));
	if(ret) {
		log_err("rdma_create_qp() failed");
		return -errno;
	}
	pc_conn->qp = pc_conn->cm_cid->qp;
	debug("queue pair created\n");

	if(pc_conn->config.server == RDMA_COMBO_SD) {
		pc_conn->sd_response_mr = rdma_buffer_register(pc_conn->pd,
														&(pc_conn->sd_response),
														sizeof(pc_conn->sd_response), 
														PERM_L_RW);
	} else {
		pc_conn->sd_response_mr = rdma_buffer_register(pc_conn->pd,
														&(pc_conn->sd_response),
														sizeof(pc_conn->sd_response), 
														PERM_R_RW);
	}
	return 0;
}


int accept_client_conn(PERK_SVR_CTX *psc, PERK_CLIENT_CONN *pc_conn)
{
	struct rdma_conn_param 	conn_par;
	struct ibv_recv_wr		recv_wr, *bad_recv_wr = NULL;
	int						ret = 0;

	if(!pc_conn->cm_cid || !pc_conn->qp) {
		log_err("cid and qp not setup correctly");
		return -EINVAL;
	}

	pc_conn->md = rdma_buffer_register(pc_conn->pd,
										&(pc_conn->md_attr), 
										sizeof(pc_conn->md_attr),
										PERM_L_RW);
	if(ret) {
		log_err("rdma_buffer_register() failed");
		return -ENOMEM;
	}

	
	rdma_recv_wr_prepare(&recv_wr, &(pc_conn->rec_sge), pc_conn->md);
	debug("posting recv\n");
	print_curr_time();
	ret = ibv_post_recv(pc_conn->qp, &recv_wr, &bad_recv_wr);
	if(ret) {
		log_err("bad work request");
		return ret;
	}
	debug("recv work request posted\n");

	memset(&conn_par, 0, sizeof(conn_par));
	conn_par.initiator_depth = 3;
	conn_par.responder_resources = 3;

	ret = rdma_accept(pc_conn->cm_cid, &conn_par);
	if(ret) {
		log_err("rdma_accept() failed");
		return -errno;
	}
	debug("accepted connection\n");
	return 0;
}

int finalize_client_conn(PERK_CLIENT_CONN *pc_conn)
{	
	memcpy(&(pc_conn->client_sa),
			rdma_get_peer_addr(pc_conn->cm_cid),
			sizeof(struct sockaddr_in));
	printf("Connection accepted: %s:%d\n",
			inet_ntoa(pc_conn->client_sa.sin_addr),
			ntohs(pc_conn->client_sa.sin_port));

	return 0;
}

int send_md_s2c(PERK_CLIENT_CONN *pc_conn)
{
	struct ibv_send_wr		send_wr, *bad_send_wr = NULL;
	struct ibv_wc 			wc;
	int 					ret = -1;

	debug("Rdma polling for recv completion\n");
	print_curr_time();
	ret = rdma_poll_cq(pc_conn->cq, &wc, 1, 100);
	if(ret != 1) {
		log_err("rdma_poll_cq() failed");
		return ret;
	}
	
	/* struct request memory */
	if(pc_conn->config.client == RDMA_COMBO_SD) {
		pc_conn->sd_request_mr = rdma_buffer_register(pc_conn->pd, &(pc_conn->sd_request), sizeof(pc_conn->sd_request), PERM_L_RW);
		if(!pc_conn->sd_request_mr) {
			log_err("ionfnonfoianeoidfna");
			return -errno;
		}
		/* prepost receive on that memory in case of sends*/
		rdma_recv_wr_prepare(&(pc_conn->recv_wr), &(pc_conn->rec_sge), pc_conn->sd_request_mr);
		ret = rdma_post_recv_reuse(&(pc_conn->recv_wr), pc_conn->qp);
		if(ret) {
			log_err("rdma_post_recv() failed");
			return 1;
		}
	} else {
		pc_conn->sd_request_mr = rdma_buffer_register(pc_conn->pd, &(pc_conn->sd_request), sizeof(pc_conn->sd_request), PERM_R_RW);
        if(!pc_conn->sd_request_mr) {
            log_err("failed to register request memory");
            return -errno;
        }
	}


	/* put the request struct into the attribute struct to send to client */
	pc_conn->server_md = setup_md_attr(pc_conn->pd, &(pc_conn->server_md_attr), pc_conn->sd_request_mr);
	if(!pc_conn->server_md) {
		log_err("rdma_buffer_register() failed");
		return -ENOMEM;
	}

	/* prepost a recv for write with IMM/send */
	//TODO: setup up better system to check if this is necessary
	pc_conn->imm_data = rdma_buffer_alloc(pc_conn->pd, MAX_IMM_SIZE, PERM_L_RW);
	if(pc_conn->config.client == RDMA_COMBO_WRIMM){
		ret = rdma_post_recv(pc_conn->imm_data, pc_conn->qp);
		if(ret) {
			log_err("failed to ACK cm event");
			return -errno;
		}
	}


	rdma_send_wr_prepare(&send_wr, &(pc_conn->snd_sge), pc_conn->server_md);
	debug("posting send\n");
	ret = ibv_post_send(pc_conn->qp, &send_wr, &bad_send_wr);
	if(ret) {
		log_err("ibv_post_send() failed");
		return -errno;
	}

	//TODO: remove this code
	/* setup attributes to send to client*/
	pc_conn->server_rd_md_mr = setup_md_attr(pc_conn->pd, &(pc_conn->server_rd_md_attr), pc_conn->sd_response_mr);
	if(!pc_conn->server_rd_md_mr) {
			log_err("rdma_buffer_register() failed");
			        return -ENOMEM;
	}
	/* and send that one as well */
	ret = rdma_post_send(pc_conn->server_rd_md_mr, pc_conn->qp);
	if(ret) return ret;

	print_curr_time();
	ret = rdma_poll_cq(pc_conn->cq, &wc, 2, 100);
	if(ret != 2) {
		log_err("process_work_completion_events() failed");
		return ret;
	}

	debug("local buffer md sent to client\n");
	return 0;
}

int disconnect_client_conn(PERK_SVR_CTX *psc, PERK_CLIENT_CONN *pc_conn)
{
	int ret = -1;

	rdma_destroy_qp(pc_conn->cm_cid);
	ret = rdma_destroy_id(pc_conn->cm_cid);
	if(ret) {
		log_err("rdma_destroy_id() failed");
	}

	ret = ibv_destroy_cq(pc_conn->cq);
	if(ret) {
		log_err("ibv_destroy_cq() failed");
	}

	ret = ibv_destroy_comp_channel(pc_conn->io_cc);
	if(ret) {
		log_err("ibv_destroy_comp_channel() failed");
	}

	if(pc_conn->server_buf != NULL) rdma_buffer_free(pc_conn->server_buf);
	//if(pc_conn->response_mr != NULL)rdma_buffer_free(pc_conn->response_mr);
	if(pc_conn->imm_data != NULL)rdma_buffer_free(pc_conn->imm_data);
	if(pc_conn->md != NULL) rdma_buffer_deregister(pc_conn->md);
	if(pc_conn->server_md != NULL) rdma_buffer_deregister(pc_conn->server_md);
	if(pc_conn->server_rd_md_mr != NULL) rdma_buffer_deregister(pc_conn->server_rd_md_mr);
	if(pc_conn->sd_response_mr != NULL) rdma_buffer_deregister(pc_conn->sd_response_mr);
	if(pc_conn->sd_request_mr != NULL) rdma_buffer_deregister(pc_conn->sd_request_mr);

	ret = ibv_dealloc_pd(pc_conn->pd);
	if(ret) {
		log_err("ibv_dealloc_pd() failed");
	}

	return 0;
}

int destroy_server_dev(PERK_SVR_CTX *psc)
{
	
	//TODO: free any QP and ACK all related events before this function call
	int ret = rdma_destroy_id(psc->cm_sid);
	if(ret) {
		log_err("rdma_destroy_id() failed");
	}

	rdma_destroy_event_channel(psc->cm_ec);
}

int client_coll_find_free(PERK_CLIENT_COLL *conns)
{
	int i;
	for(i = 0; i < MAX_CLIENTS; ++i) {
		if(!conns->active[i]) break;
	}
	return i;
}

int client_coll_find_conn(PERK_CLIENT_COLL *conns, struct sockaddr *addr)
{
	int i;
	for(i = 0; i < MAX_CLIENTS; ++i) {
		if(addr_eq(addr, (struct sockaddr *)&(conns->clients[i].cm_cid->route.addr.dst_addr))) {
			break;
		}
	}
	return i;
}

/**
 * Client functions
 *
 **/
int init_client_dev(PERK_CLT_CTX *pcc, struct sockaddr_in *svr_sa)
{
	struct rdma_cm_event *cme = NULL;
	int ret = -1;

	pcc->cm_ec = rdma_create_event_channel();
	if(!pcc->cm_ec) {
		log_err("rdma_create_event_channel() failed");
		return -errno;
	}
	debug("Client event channel created\n");

	ret = rdma_create_id(pcc->cm_ec, &(pcc->cm_cid), NULL, RDMA_PS_TCP);
	if(ret) {
		log_err("rdma_create_id() failed");
		return -errno;
	}

	ret = rdma_resolve_addr(pcc->cm_cid, NULL, (struct sockaddr*) &(pcc->client_sa), 2000);
	if(ret) {
		log_err("rdma_reslove_addr() failed");
		return -errno;
	}
	debug("Waiting for event\n");

	ret = rdma_cm_event_rcv(pcc->cm_ec, RDMA_CM_EVENT_ADDR_RESOLVED, &cme);
	if(ret) {
		log_err("rdma_cm_event_rcv() failed");
		return ret;
	}

	ret = rdma_ack_cm_event(cme);
	if(ret) {
		log_err("rdma_ack_cm_event() failed");
		return -errno;
	}

	ret = rdma_resolve_route(pcc->cm_cid, 2000);
	if(ret) {
		log_err("rdma_resolve_route() failed");
		return -errno;
	}

	ret = rdma_cm_event_rcv(pcc->cm_ec, RDMA_CM_EVENT_ROUTE_RESOLVED, &cme);
	if(ret) {
		log_err("rdma_cm_event_rcv() failed");
		return ret;
	}

	ret = rdma_ack_cm_event(cme);
	if(ret) {
		log_err("rdma_ack_cm_event() failed");
		return -errno;
	}

	debug("device used: %s\n", ibv_get_device_name(pcc->cm_cid->verbs->device));

	pcc->pd = ibv_alloc_pd(pcc->cm_cid->verbs);
	if(!pcc->pd) {
		log_err("ibv_alloc_pd() failed");
		return -errno;
	}

	pcc->comp_channel = ibv_create_comp_channel(pcc->cm_cid->verbs);
	if(!pcc->comp_channel) {
		log_err("ibv_create_comp_channel() failed");
		return -errno;
	}

	pcc->cq = ibv_create_cq(pcc->cm_cid->verbs, MAX_CQ_SIZE, NULL, pcc->comp_channel, 0);
	if(!pcc->cq) {
		log_err("ibv_create_cq failed");
		return -errno;
	}

	ret = ibv_req_notify_cq(pcc->cq, 0);
	if(ret) {
		log_err("ibv_req_notify_cq() failed");
		return -errno;
	}

	qp_init_attr_prepare(&(pcc->qp_init_attr), pcc->cq);
	ret = rdma_create_qp(pcc->cm_cid, pcc->pd, &(pcc->qp_init_attr));
	if(ret) {
		log_err("rdma_create_qp() failed");
		return -errno;
	}
	pcc->qp = pcc->cm_cid->qp;
	return 0;
}

int client_pre_post_recv_buffer(PERK_CLT_CTX *pcc)
{
	struct ibv_recv_wr 	rec_wr;
	struct ibv_recv_wr	*bad_rec_wr = NULL;
	int 				ret = -1;

	pcc->server_md_mr = rdma_buffer_register(pcc->pd,
										  &(pcc->server_md_attr),
										  sizeof(pcc->server_md_attr),
										  PERM_L_RW);
	if(!pcc->server_md_mr) {
		log_err("rdma_buffer_register() failed");
		return -ENOMEM;
	}

	rdma_recv_wr_prepare(&rec_wr, &(pcc->rec_sge), pcc->server_md_mr);
	debug("pre posting recv\n");
	print_curr_time();
	/* post receive for memory attributes sent back by server */
	ret = ibv_post_recv(pcc->qp, &rec_wr, &bad_rec_wr);
	if(ret) {
		log_err("ibv_post_recv() failed");
		return ret;
	}

	/* setup memory to store READable memory */
	pcc->server_rd_md_mr = rdma_buffer_register(pcc->pd,
												&(pcc->server_rd_md_attr),
												sizeof(pcc->server_rd_md_attr),
												PERM_L_RW);
	if(!pcc->server_rd_md_mr) {
		log_err("rdma_buffer_register() failed");
		return -ENOMEM;
	}

	ret = rdma_post_recv(pcc->server_rd_md_mr, pcc->qp);
	if(ret) return ret;


	debug("Pre posted a recv work request\n");
	return 0;
}

int connect_to_server(PERK_CLT_CTX *pcc)
{
	struct rdma_conn_param conn_param;
	struct rdma_cm_event *cme = NULL;
	int ret = -1;

	bzero(&conn_param, sizeof(conn_param));
	conn_param.initiator_depth = 3;
	conn_param.responder_resources = 3;
	conn_param.retry_count = 3;
	ret = rdma_connect(pcc->cm_cid, &conn_param);
	if(ret) {
		log_err("rdma_connect() failed");
		return -errno;
	}

	debug("Waiting for connection to be established\n");
	ret = rdma_cm_event_rcv(pcc->cm_ec, RDMA_CM_EVENT_ESTABLISHED, &cme);
	if(ret) {
		log_err("rdma_cm_event_rcv() failed");
		return ret;
	}

	ret = rdma_ack_cm_event(cme);
	if(ret) {
		log_err("rdma_ack_cm_event() failed");
		return -errno;
	}

	debug("Client is now connected\n");
	return 0;
}

int send_md_c2s(PERK_CLT_CTX *pcc)
{
	struct ibv_send_wr 	snd_wr;
	struct ibv_send_wr	*bad_snd_wr = NULL;
	struct ibv_wc 		wc[2];
	int ret = -1;
	debug("Registering rdma buffers\n");
	/* register request memory, this will be used for client->server */
	if(pcc->config.client == RDMA_COMBO_SD) {	
		pcc->sd_request_mr = rdma_buffer_register(pcc->pd, &(pcc->sd_request), sizeof(pcc->sd_request), PERM_L_RW);
		if(!pcc->sd_request_mr) {
			log_err("failed to register memory\n");
		}
	} else {
		pcc->sd_request_mr = rdma_buffer_register(pcc->pd, &(pcc->sd_request), sizeof(pcc->sd_request), PERM_R_RW);
		if(!pcc->sd_request_mr) {
			log_err("failed to register memory\n");
		}
	}
	/* register response memory, will be used for server->client, or READ client<-server */
	if(pcc->config.server == RDMA_COMBO_SD) {
		pcc->sd_response_mr = rdma_buffer_register(pcc->pd, &(pcc->sd_response), sizeof(pcc->sd_response), PERM_L_RW);
		if(!pcc->sd_response_mr) {
			log_err("failed ot register response memory\n");
		}
		rdma_recv_wr_prepare(&(pcc->recv_wr), &(pcc->rec_sge), pcc->sd_response_mr);
		ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		if(ret) {
			log_err("rdma_post_recv() failed");
			return ret;
		}
	} else {
		pcc->sd_response_mr = rdma_buffer_register(pcc->pd, &(pcc->sd_response), sizeof(pcc->sd_response), PERM_R_RW);
		if(!pcc->sd_response_mr) {
			log_err("failed ot register response memory\n");
		}
		
	}

	/* store request memory in attribute to send to the server for registration */
	pcc->kvs_request_attr_mr = setup_md_attr(pcc->pd, &(pcc->kvs_request_attr), pcc->sd_response_mr);
	if(!pcc->kvs_request_attr_mr) {
		log_err("rdma_buffer_register() failed");
		return -ENOMEM;
	}

	rdma_send_wr_prepare(&snd_wr, &(pcc->snd_sge), pcc->kvs_request_attr_mr);

	/* send the memory we will use for WRITEs to the server */
	debug("Posting send\n");
	print_curr_time();
	ret = ibv_post_send(pcc->qp, &snd_wr, &bad_snd_wr);
	if(ret) {
		log_err("ibv_post_send() failed");
		return -errno;
	}

	debug("Waiting for completion of recv and send\n");

	ret = rdma_poll_cq(pcc->cq, wc, 3, 100);
	if(ret != 3) {
		log_err("rdma_poll_cq() failed");
		return ret;
	}


	debug("Server buffer location and credentials received\n");
	return 0;
}


int client_disconnect(PERK_CLT_CTX *pcc)
{
	struct rdma_cm_event *cme = NULL;
	int ret = -1;

	ret = rdma_disconnect(pcc->cm_cid);
	if(ret) {
		log_err("rdma_disconnect() failed");
	}

	ret = rdma_cm_event_rcv(pcc->cm_ec, RDMA_CM_EVENT_DISCONNECTED, &cme);
	if(ret) {
		log_err("rdma_cm_event_rcv() failed");
	}

	ret = rdma_ack_cm_event(cme);
	if(ret) {
		log_err("rdma_ack_cm_event() failed");
	}

	rdma_destroy_qp(pcc->cm_cid);
	ret = rdma_destroy_id(pcc->cm_cid);
	if(ret) {
		log_err("rdma_destory_id() failed");
	}

	ret = ibv_destroy_cq(pcc->cq);
	if(ret) {
		log_err("rdma_destroy_cq() failed");
	}

	ret = ibv_destroy_comp_channel(pcc->comp_channel);
	if(ret) {
		log_err("ibv_destroy_comp_channel() failed");
	}
	
	if(pcc->server_md_mr != NULL) rdma_buffer_deregister(pcc->server_md_mr);
	if(pcc->kvs_request_attr_mr != NULL) rdma_buffer_deregister(pcc->kvs_request_attr_mr);
	if(pcc->kvs_request_mr != NULL) rdma_buffer_deregister(pcc->kvs_request_mr);
	if(pcc->response_mr != NULL) rdma_buffer_deregister(pcc->response_mr);
	if(pcc->server_rd_md_mr != NULL) rdma_buffer_deregister(pcc->server_rd_md_mr);

	if(pcc->sd_request_mr != NULL) rdma_buffer_deregister(pcc->sd_request_mr);
	if(pcc->sd_response_mr != NULL) rdma_buffer_deregister(pcc->sd_response_mr);

	ret = ibv_dealloc_pd(pcc->pd);
	if(ret) {
		log_err("ibv_dealloc_pd() failed");
	}

	rdma_destroy_event_channel(pcc->cm_ec);

	return 0;
}

/**
 * Shared functions
 *
 **/
void qp_init_attr_prepare(struct ibv_qp_init_attr *qp_attr, struct ibv_cq *cq)
{
	memset(qp_attr, 0, sizeof(*qp_attr));
    qp_attr->cap.max_recv_sge = MAX_SGE;
    qp_attr->cap.max_recv_wr = MAX_WR;
    qp_attr->cap.max_send_sge = MAX_SGE;
    qp_attr->cap.max_send_wr = MAX_WR;
    qp_attr->qp_type = IBV_QPT_RC;
    qp_attr->recv_cq = cq;
    qp_attr->send_cq = cq;
}

struct ibv_mr *setup_md_attr(struct ibv_pd *pd, struct rdma_buffer_attr *attr, struct ibv_mr *mr)
{
	struct ibv_mr *attr_mr;
	/* setup the server_side memory attributes with create buffer */
	attr->address = (uint64_t) mr->addr;
	attr->length = (uint32_t) mr->length;
	attr->stag.local_stag = (uint32_t) mr->lkey;

	/* register the attributes structure as sendable memory */
	return rdma_buffer_register(pd, attr, sizeof(*attr), PERM_L_RW);
}


void rdma_write_wr_prepare(struct ibv_send_wr *wr, 
	struct ibv_sge *sg, struct ibv_mr *mr, struct rdma_buffer_attr r_attr)
{
	sg->addr = (uint64_t) mr->addr;
	sg->length = (uint32_t) mr->length;
	sg->lkey = mr->lkey;
	bzero(wr, sizeof(*wr));
	wr->sg_list = sg;
	wr->num_sge = 1;
	wr->opcode = IBV_WR_RDMA_WRITE;
	wr->send_flags = IBV_SEND_SIGNALED;
	wr->wr.rdma.rkey = r_attr.stag.remote_stag;
	wr->wr.rdma.remote_addr = r_attr.address;
}

void rdma_read_wr_prepare(struct ibv_send_wr *wr, 
	struct ibv_sge *sg, struct ibv_mr *mr, struct rdma_buffer_attr r_attr)
{
	sg->addr = (uint64_t) mr->addr;
	sg->length = (uint32_t) mr->length;
	sg->lkey = mr->lkey;
	bzero(wr, sizeof(*wr));
	wr->sg_list = sg;
	wr->num_sge = 1;
	wr->opcode = IBV_WR_RDMA_READ;
	wr->send_flags = IBV_SEND_SIGNALED;
	wr->wr.rdma.rkey = r_attr.stag.remote_stag;
	wr->wr.rdma.remote_addr = r_attr.address;
}

void rdma_write_imm_wr_prepare(struct ibv_send_wr *wr, 
	struct ibv_sge *sg, struct ibv_mr *mr, struct rdma_buffer_attr r_attr)
{
	sg->addr = (uint64_t) mr->addr;
	sg->length = (uint32_t) mr->length;
	sg->lkey = mr->lkey;
	bzero(wr, sizeof(*wr));
	wr->sg_list = sg;
	wr->num_sge = 1;
	wr->opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
	wr->send_flags = IBV_SEND_SIGNALED;
	wr->wr.rdma.rkey = r_attr.stag.remote_stag;
	wr->wr.rdma.remote_addr = r_attr.address;
}

int rdma_post_write_reuse(struct ibv_send_wr *wr, struct ibv_qp *qp)
{
	struct ibv_send_wr	*bad_snd_wr = NULL;
	if(ibv_post_send(qp, wr, &bad_snd_wr)) {
		log_err("ibv_post_send() failed");
		return -errno;
	}
	return 0;
}



int rdma_post_recv(struct ibv_mr *mr, struct ibv_qp *qp)
{
	struct ibv_sge sg;
	struct ibv_recv_wr wr;
	struct ibv_recv_wr *bad_wr = NULL;
	memset(&sg, 0, sizeof(sg));
	sg.addr = (uint64_t)mr->addr;
	sg.length = (uint32_t)mr->length;
	sg.lkey = (uint32_t)mr->lkey;

	memset(&wr, 0, sizeof(wr));
	wr.sg_list = &sg;
	wr.num_sge = 1;
	if(ibv_post_recv(qp, &wr, &bad_wr)) {
		log_err("ibv_post_recv() failed");
		return -errno;
	}
	return 0;
}

void rdma_recv_wr_prepare(struct ibv_recv_wr *wr, struct ibv_sge *sg, struct ibv_mr *mr)
{
	memset(sg, 0, sizeof(*sg));
	sg->addr = (uint64_t)mr->addr;
	sg->length = (uint32_t)mr->length;
	sg->lkey = (uint32_t)mr->lkey;

	memset(wr, 0, sizeof(*wr));
	wr->sg_list = sg;
	wr->num_sge = 1;
}

int rdma_post_recv_reuse(struct ibv_recv_wr *wr, struct ibv_qp *qp)
{
	struct ibv_recv_wr *bad_wr = NULL;
	int ret;
	if((ret = ibv_post_recv(qp, wr, &bad_wr))) {
		log_err("ibv_post_recv() failed");
		return ret;
	}
	return 0;
}

int rdma_post_send(struct ibv_mr *mr, struct ibv_qp *qp)
{
	struct ibv_sge sg;
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr = NULL;

	memset(&sg, 0, sizeof(sg));
	sg.addr = (uint64_t) mr->addr;
	sg.length = (uint32_t)mr->length;
	sg.lkey = (uint32_t)mr->lkey;
	memset(&wr, 0, sizeof(wr));
	wr.sg_list = &sg;
	wr.num_sge = 1;
	wr.opcode = IBV_WR_SEND;
	wr.send_flags = IBV_SEND_SIGNALED;
	if(ibv_post_send(qp, &wr, &bad_wr)) {
		log_err("ibv_post_send() failed");
		return -errno;
	}
	return 0;
}

void rdma_send_wr_prepare(struct ibv_send_wr *wr, struct ibv_sge *sg, struct ibv_mr *mr)
{
	memset(sg, 0, sizeof(*sg));
	sg->addr = (uint64_t) mr->addr;
	sg->length = (uint32_t)mr->length;
	sg->lkey = (uint32_t)mr->lkey;
	memset(wr, 0, sizeof(*wr));
	wr->sg_list = sg;
	wr->num_sge = 1;
	wr->opcode = IBV_WR_SEND;
	wr->send_flags = IBV_SEND_SIGNALED;
}

int rdma_post_send_reuse(struct ibv_send_wr *wr, struct ibv_qp *qp)
{
	struct ibv_send_wr *bad_wr = NULL;
	int ret = ibv_post_send(qp, wr, &bad_wr);
	if(ret) {
		log_err("ibv_post_send() failed");
		return -errno;
	}
	return 0;
}

/* poll on cq directly using timeout */
int rdma_poll_cq(struct ibv_cq *cq,
				struct ibv_wc *wc, 
				int max_wc, 
				useconds_t timeout)
{
	int ret = -1, total_wc = 0;
	debug("polling for %d wcs\n", max_wc);
	do {
		/* check for work completions */
		ret = ibv_poll_cq(cq, max_wc - total_wc, wc + total_wc);
		if(ret < 0) {
			log_err("ibv_poll_cq() failed");
			return -ret;
		}
		total_wc += ret;


		if(total_wc < max_wc && timeout > 0) {
			usleep(timeout);
		}
	} while(total_wc < max_wc);

	if(total_wc > 0) {
		ret = validate_wcs(wc, total_wc);
		if(ret) return ret;
	}

	return total_wc;
}

/* poll on cq, with no timeout, i.e. spinning */
int rdma_spin_cq(struct ibv_cq *cq,
				struct ibv_wc *wc, 
				int max_wc)
{
	int ret = -1, total_wc = 0;
	debug("polling for %d wcs\n", max_wc);
	do {
		/* check for work completions */
		ret = ibv_poll_cq(cq, max_wc - total_wc, wc + total_wc);
		if(ret < 0) {
			log_err("ibv_poll_cq() failed");
			return -ret;
		}
		total_wc += ret;
		//debug("polled, %d wc total\n", total_wc);
	} while(total_wc < max_wc);

	ret = validate_wcs(wc, total_wc);
	if(ret) return ret;

	return total_wc;
}

int rdma_clear_cq(struct ibv_cq *cq) 
{
	int ret = -1;
	struct ibv_wc wc[MAX_CQ_SIZE];
	
	do {
		memset(wc, 0, sizeof(wc));
		ret = ibv_poll_cq(cq, MAX_CQ_SIZE, wc);
		if(ret == 0) break;
		if(ret < 0) {
			log_err("ibv_poll_cq() failed");
			return -1;
		}
		ret = validate_wcs(wc, ret);
		if(ret) return ret;
	} while(1);
	return ret;
}

int validate_wcs(struct ibv_wc *wc, int tot)
{
	int i;
	for(i = 0; i < tot; i++) {
		if(wc[i].status != IBV_WC_SUCCESS) {
			log_err("WC error: %s [OPCODE: %d]", 
				ibv_wc_status_str(wc[i].status),
				wc[i].opcode);
			switch(wc[i].opcode) {
				case IBV_WC_SEND:
					debug("SEND FAILED\n");
					break;
				case IBV_WC_RECV:
					debug("RECV FAILED\n");
					break;
				default:
					break;
			}
			return -(wc[i].status);
		}
	}
	return 0;
}

int rdma_cm_event_rcv(struct rdma_event_channel *ec,
					enum rdma_cm_event_type e_type,
					struct rdma_cm_event **cme)
{
	int ret = -1;
	ret = rdma_get_cm_event(ec, cme);
	if(ret) {
		log_err("failed to retrieve cm event");
		return -errno;
	}

	if((*cme)->status != 0) {
		log_err("event has non zero status: %d", (*cme)->status);
		rdma_ack_cm_event(*cme);
		return -((*cme)->status);
	}
	
	if((*cme)->event != e_type) {
		log_err("unexpected event received: %s, expected: %s", 
				rdma_event_str((*cme)->event), 
				rdma_event_str(e_type));
		rdma_ack_cm_event(*cme);
		return 1;
	}
	debug("Received %s\n", rdma_event_str(e_type));
	return 0;
}

int rdma_cm_event_rcv_any(struct rdma_event_channel *ec,
					struct rdma_cm_event **cme)
{
	int ret = -1;
	ret = rdma_get_cm_event(ec, cme);
	if(ret) {
		log_err("failed to retrieve cm event");
		return -errno;
	}
	
	if((*cme)->status != 0) {
		log_err("event has non zero status: %d", (*cme)->status);
		rdma_ack_cm_event(*cme);
		return -((*cme)->status);
	}
	
	debug("Received %s\n", rdma_event_str((*cme)->event));
	return 0;
}

struct ibv_mr* rdma_buffer_alloc(struct ibv_pd *pd, uint32_t len, enum ibv_access_flags perm)
{
	struct ibv_mr *mr = NULL;
	if(!pd) {
		log_err("Protection domain is null");
		return NULL;
	}

	void *buf = calloc(1, len);
	if(!buf) {
		log_err("calloc() failed");
		return NULL;
	}

	mr = rdma_buffer_register(pd, buf, len, perm);
	if(!mr){
		free(buf);
		buf = NULL;
	}
	return mr;
}

struct ibv_mr *rdma_buffer_register(struct ibv_pd *pd, void *addr, uint32_t len, enum ibv_access_flags perm)
{
	struct ibv_mr *mr = NULL;
	if(!pd) {
		log_err("protection domain is null");
		return NULL;
	}

	mr = ibv_reg_mr(pd, addr, len, perm);
	if(!mr) {
		log_err("ibv_reg_mr() failed");
		return NULL;
	}
	return mr;
}

void rdma_buffer_free(struct ibv_mr *mr)
{
	if(!mr) {
		log_err("mr is null, cant free");
		return;
	}
	void *to_free = mr->addr;
	rdma_buffer_deregister(mr);
	debug("Buffer freed\n");
	free(to_free);
}

void rdma_buffer_deregister(struct ibv_mr *mr)
{
	if(!mr) {
		log_err("mr is null, cant free");
		return;
	}
	ibv_dereg_mr(mr);
}


int set_comp_channel_non_block(struct ibv_comp_channel *io_cc)
{
	int flags, ret;
	flags = fcntl(io_cc->fd, F_GETFL);
	if(flags == -1) {
		log_err("getting flagsd failed");
		return -errno;
	}
	ret = fcntl(io_cc->fd, F_SETFL, flags | O_NONBLOCK);
	if(ret < 0) {
		log_err("modifying completion channel flags failed");
		return -errno;
	}
	return 0;
}

void show_rdma_buffer_attr(struct rdma_buffer_attr *attr){
	if(!attr){
		log_err("Passed attr is NULL");
		return;
	}
	printf("---------------------------------------------------------\n");
	printf("buffer attr, addr: %p , len: %u , stag : 0x%x \n", 
			(void*) attr->address, 
			(unsigned int) attr->length,
			attr->stag.local_stag);
	printf("---------------------------------------------------------\n");
}

void print_ibv_devs()
{
	int n_dev, i;
	struct ibv_device** ibv_devs = ibv_get_device_list(&n_dev);
	
	for(i = 0; i < n_dev; ++i) {
		printf("[IBV_DEV] KIB dev: %s, Uverbs dev: %s, dev path=%s, class dev: %s\n", 
						ibv_devs[i]->name, 
						ibv_devs[i]->dev_name,
						ibv_devs[i]->dev_path,
						ibv_devs[i]->ibdev_path);
	}
}


