#include "rdma.h"


struct ibv_context *init_ibv_dev(char *dev_name)
{
	struct ibv_context *ctx = NULL;
	int n_dev = 0;
	struct ibv_device** ibv_devs = ibv_get_device_list(&n_dev);
	if(dev_name != NULL) {
		for(int i = 0; i < n_dev; ++i) {
			if(strncmp(ibv_devs[i]->dev_name, dev_name, IBV_SYSFS_NAME_MAX-1)) {
				ctx = ibv_open_device(ibv_devs[i]);
				break;
			}
		}
	} else{
		for(int i = 0; i < n_dev; ++i) {
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
int init_server_dev(PEARS_SVR_CTX *psc)
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

int init_server_client_resources(PEARS_CLIENT_CONN *pc_conn)
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

	//TODO: check comp_vector arg
	//		signalling for CQs that arent for receiving metadata
	//		could be interesting spot for optimization
	// https://linux.die.net/man/3/ibv_create_cq
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

	bzero(&(pc_conn->qp_init_attr), sizeof(pc_conn->qp_init_attr));
	pc_conn->qp_init_attr.cap.max_recv_sge = MAX_SGE;
	pc_conn->qp_init_attr.cap.max_recv_wr = MAX_WR;
	pc_conn->qp_init_attr.cap.max_send_sge = MAX_SGE;
	pc_conn->qp_init_attr.cap.max_send_wr = MAX_WR;
	pc_conn->qp_init_attr.qp_type = IBV_QPT_RC;
	pc_conn->qp_init_attr.recv_cq = pc_conn->cq;
	pc_conn->qp_init_attr.send_cq = pc_conn->cq;
	ret = rdma_create_qp(pc_conn->cm_cid,
						pc_conn->pd,
						&(pc_conn->qp_init_attr));
	if(ret) {
		log_err("rdma_create_qp() failed");
		return -errno;
	}
	pc_conn->qp = pc_conn->cm_cid->qp;
	debug("queue pair created\n");
	return 0;
}

int wait_for_client_conn(PEARS_SVR_CTX *psc, PEARS_CLIENT_CONN *pc_conn)
{
	struct rdma_cm_event *cm_event = NULL;
	int ret = -1;
	
	debug("Waiting for client to connect\n");
	ret = rdma_cm_event_rcv(psc->cm_ec, RDMA_CM_EVENT_CONNECT_REQUEST, &cm_event);
	if(ret) {
		log_err("failed to retrieve cm event");
		return ret;
	}
	pc_conn->cm_cid = cm_event->id;
	
	ret = rdma_ack_cm_event(cm_event);
	if(ret) {
		log_err("failed to ACK cm event");
		return -errno;
	}
	debug("RDMA client connected\n");
	return 0;
}

int accept_client_conn(PEARS_SVR_CTX *psc, PEARS_CLIENT_CONN *pc_conn)
{
	struct rdma_conn_param 	conn_par;
	struct rdma_cm_event 	*cm_event = NULL;
	struct ibv_recv_wr		recv_wr, *bad_recv_wr = NULL;
	struct sockaddr_in		remote_sa;
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

	pc_conn->rec_sge.addr = (uint64_t) pc_conn->md->addr;
	pc_conn->rec_sge.length = pc_conn->md->length;
	pc_conn->rec_sge.lkey = pc_conn->md->lkey;

	bzero(&recv_wr, sizeof(recv_wr));
	recv_wr.sg_list = &(pc_conn->rec_sge);
	recv_wr.num_sge = 1;
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

	ret = rdma_cm_event_rcv(psc->cm_ec, RDMA_CM_EVENT_ESTABLISHED, &cm_event);
	if(ret) {
		log_err("failed to retrieve cm event");
		return ret;
	}

	ret = rdma_ack_cm_event(cm_event);
	if(ret) {
		log_err("failed to ACK cm event");
		return -errno;
	}

	memcpy(&(pc_conn->client_sa),
			rdma_get_peer_addr(pc_conn->cm_cid),
			sizeof(struct sockaddr_in));
	printf("Connection accepted: %s:%d\n",
			inet_ntoa(pc_conn->client_sa.sin_addr),
			ntohs(pc_conn->client_sa.sin_port));

	return 0;
}

int send_md_s2c(PEARS_CLIENT_CONN *pc_conn)
{
	struct ibv_send_wr		send_wr, *bad_send_wr = NULL;
	struct ibv_wc 			wc;
	int 					ret = -1;

	ret = retrieve_work_completion_events(pc_conn->io_cc, &wc, 1);
	if(ret != 1) {
		log_err("process_work_completion_events() failed");
		return ret;
	}

	show_rdma_buffer_attr(&(pc_conn->md_attr));

	/* receive information about buffer */
	printf("New buffer of length %u bytes requested\n", pc_conn->md_attr.length);
	pc_conn->server_buf = rdma_buffer_alloc(pc_conn->pd,
											pc_conn->md_attr.length,
											PERM_R_RW);
	if(!pc_conn->server_buf) {
		log_err("rdma_buffer_alloc() failed");
		return -ENOMEM;
	}

	pc_conn->server_md_attr.address = (uint64_t) pc_conn->server_buf->addr;
	pc_conn->server_md_attr.length = (uint32_t) pc_conn->server_buf->length;
	pc_conn->server_md_attr.stag.local_stag = (uint32_t) pc_conn->server_buf->lkey;

	pc_conn->server_md = rdma_buffer_register(pc_conn->pd,
											  &(pc_conn->server_md_attr),
											  sizeof(pc_conn->server_md_attr),
											  PERM_L_RW);
	if(!pc_conn->server_md) {
		log_err("rdma_buffer_register() failed");
		return -ENOMEM;
	}

	show_rdma_buffer_attr(&(pc_conn->server_md_attr));

	pc_conn->snd_sge.addr = (uint64_t) &(pc_conn->server_md_attr);
	pc_conn->snd_sge.length = sizeof(pc_conn->server_md_attr);
	pc_conn->snd_sge.lkey = pc_conn->server_md->lkey;

	bzero(&send_wr, sizeof(send_wr));
	send_wr.sg_list = &(pc_conn->snd_sge);
	send_wr.num_sge = 1;
	send_wr.opcode = IBV_WR_SEND;
	send_wr.send_flags = IBV_SEND_SIGNALED;
	ret = ibv_post_send(pc_conn->qp, &send_wr, &bad_send_wr);
	if(ret) {
		log_err("ibv_post_send() failed");
		return -errno;
	}

	ret = retrieve_work_completion_events(pc_conn->io_cc, &wc, 1);
	if(ret != 1) {
		log_err("retrieve_work_completion_events() failed");
		return ret;
	}
	debug("local buffer md sent to client\n");
	return 0;
}

int disconnect_client_conn(PEARS_SVR_CTX *psc, PEARS_CLIENT_CONN *pc_conn)
{
	struct rdma_cm_event *cme = NULL;
	int ret = -1;

	//TODO: should be done in handler function
	ret = rdma_cm_event_rcv(psc->cm_ec,
							RDMA_CM_EVENT_DISCONNECTED,
							&cme);
	if(ret) {
		log_err("rdma_cm_event_rcv() for disconnect failed");
		return ret;
	}

	ret = rdma_ack_cm_event(cme);
	if(ret) {
		log_err("rdma_ack_cm_event() failed");
		return -errno;
	}

	printf("buff: %s\n", (char*)(pc_conn->server_md_attr.address));

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

	rdma_buffer_free(pc_conn->server_buf);
	rdma_buffer_deregister(pc_conn->md);
	rdma_buffer_deregister(pc_conn->server_md);
	

	ret = ibv_dealloc_pd(pc_conn->pd);
	if(ret) {
		log_err("ibv_dealloc_pd() failed");
	}

	return 0;
}

int destroy_server_dev(PEARS_SVR_CTX *psc)
{
	
	//TODO: free any QP and ACK all related events before this function call
	int ret = rdma_destroy_id(psc->cm_sid);
	if(ret) {
		log_err("rdma_destroy_id() failed");
	}

	rdma_destroy_event_channel(psc->cm_ec);
}

/**
 * Client functions
 *
 **/
int init_client_dev(PEARS_CLT_CTX *pcc, struct sockaddr_in *svr_sa)
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

	bzero(&(pcc->qp_init_attr), sizeof(pcc->qp_init_attr));
	pcc->qp_init_attr.cap.max_recv_sge = MAX_SGE;
	pcc->qp_init_attr.cap.max_recv_wr = MAX_WR;
	pcc->qp_init_attr.cap.max_send_sge = MAX_SGE;
	pcc->qp_init_attr.cap.max_send_wr = MAX_WR;
	pcc->qp_init_attr.qp_type = IBV_QPT_RC;
	pcc->qp_init_attr.recv_cq = pcc->cq;
	pcc->qp_init_attr.send_cq = pcc->cq;
	ret = rdma_create_qp(pcc->cm_cid, pcc->pd, &(pcc->qp_init_attr));
	if(ret) {
		log_err("rdma_create_qp() failed");
		return -errno;
	}
	pcc->qp = pcc->cm_cid->qp;
	return 0;
}

int client_pre_post_recv_buffer(PEARS_CLT_CTX *pcc)
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

	pcc->rec_sge.addr = (uint64_t) pcc->server_md_mr->addr;
	pcc->rec_sge.length = (uint32_t) pcc->server_md_mr->length;
	pcc->rec_sge.lkey = (uint32_t) pcc->server_md_mr->lkey;
	bzero(&(rec_wr), sizeof(rec_wr));
	rec_wr.sg_list = &(pcc->rec_sge);
	rec_wr.num_sge = 1;
	ret = ibv_post_recv(pcc->qp, &rec_wr, &bad_rec_wr);
	if(ret) {
		log_err("ibv_post_recv() failed");
		return ret;
	}
	debug("Pre posted a recv work request\n");
	return 0;
}

int connect_to_server(PEARS_CLT_CTX *pcc)
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

int send_md_c2s(PEARS_CLT_CTX *pcc)
{
	struct ibv_send_wr 	snd_wr;
	struct ibv_send_wr	*bad_snd_wr = NULL;
	struct ibv_wc 		wc[2];
	int ret = -1;

	pcc->kvs_request_mr = rdma_buffer_register(pcc->pd,
											   pcc->kvs_request,
											   strlen(pcc->kvs_request),
											   PERM_R_RW);
	if(!pcc->kvs_request_mr) {
		log_err("rdma_buffer_register() failed");
		return -ENOMEM;
	}

	pcc->kvs_request_attr.address = (uint64_t) pcc->kvs_request_mr->addr;
	pcc->kvs_request_attr.length = pcc->kvs_request_mr->length;
	pcc->kvs_request_attr.stag.local_stag = pcc->kvs_request_mr->lkey;
	pcc->kvs_request_attr_mr = rdma_buffer_register(pcc->pd,
													&(pcc->kvs_request_attr),
													sizeof(pcc->kvs_request_attr),
													PERM_L_RW);
	if(!pcc->kvs_request_attr_mr) {
		log_err("rdma_buffer_register() failed");
		return -ENOMEM;
	}

	pcc->snd_sge.addr = (uint64_t) pcc->kvs_request_attr_mr->addr;
	pcc->snd_sge.length = (uint32_t) pcc->kvs_request_attr_mr->length;
	pcc->snd_sge.lkey = pcc->kvs_request_attr_mr->lkey;
	bzero(&snd_wr, sizeof(snd_wr));
	snd_wr.sg_list = &(pcc->snd_sge);
	snd_wr.num_sge = 1;
	snd_wr.opcode = IBV_WR_SEND;
	snd_wr.send_flags = IBV_SEND_SIGNALED;

	ret = ibv_post_send(pcc->qp, &snd_wr, &bad_snd_wr);
	if(ret) {
		log_err("ibv_post_send() failed");
		return -errno;
	}

	ret = retrieve_work_completion_events(pcc->comp_channel, wc, 2);
	if(ret != 2) {
		log_err("retrieve_work_completion_events() for 2 wc failed");
		return ret;
	}
	show_rdma_buffer_attr(&(pcc->kvs_request_attr));

	debug("Server buffer location and credentials received\n");
	return 0;
}

int rdma_write_c2s(PEARS_CLT_CTX *pcc)
{
	struct ibv_send_wr 	snd_wr;
	struct ibv_send_wr	*bad_snd_wr = NULL;
	struct ibv_wc 		wc;
	int 				ret = -1;

	pcc->remote_mr = rdma_buffer_register(pcc->pd,
										  pcc->remote,
										  strlen(pcc->kvs_request),
										  PERM_R_RW);
	if(!pcc->remote_mr) {
		log_err("rdma_buffer_register() failed");
		return -ENOMEM;
	}

	pcc->snd_sge.addr = (uint64_t) pcc->kvs_request_mr->addr;
	pcc->snd_sge.length = (uint32_t) pcc->kvs_request_mr->length;
	pcc->snd_sge.lkey = pcc->kvs_request_mr->lkey;
	bzero(&snd_wr, sizeof(snd_wr));
	snd_wr.sg_list = &(pcc->snd_sge);
	snd_wr.num_sge = 1;
	snd_wr.opcode = IBV_WR_RDMA_WRITE;
	snd_wr.send_flags = IBV_SEND_SIGNALED;
	snd_wr.wr.rdma.rkey = pcc->server_md_attr.stag.remote_stag;
	snd_wr.wr.rdma.remote_addr = pcc->server_md_attr.address;

	ret = ibv_post_send(pcc->qp, &snd_wr, &bad_snd_wr);
	if(ret) {
		log_err("ibv_post_send() failed");
		return -errno;
	}

	ret = retrieve_work_completion_events(pcc->comp_channel, &wc, 1);
	if(ret != 1) {
		log_err("retrieve_work_completion_events() failed");
		return ret;
	}
	debug("Buffer written to server\n");

	pcc->snd_sge.addr = (uint64_t) pcc->remote_mr->addr;
	pcc->snd_sge.length = (uint32_t) pcc->remote_mr->length;
	pcc->snd_sge.lkey = pcc->remote_mr->lkey;
	bzero(&snd_wr, sizeof(snd_wr));
	snd_wr.sg_list = &(pcc->snd_sge);
	snd_wr.num_sge = 1;
	snd_wr.opcode = IBV_WR_RDMA_READ;
	snd_wr.send_flags = IBV_SEND_SIGNALED;
	/* same remote data as we wrote to before */
	snd_wr.wr.rdma.rkey = pcc->server_md_attr.stag.remote_stag;
	snd_wr.wr.rdma.remote_addr = pcc->server_md_attr.address;

	ret = ibv_post_send(pcc->qp, &snd_wr, &bad_snd_wr);
	if(ret) {
		log_err("failed to read from remote address");
		return -errno;
	}

	ret = retrieve_work_completion_events(pcc->comp_channel, &wc, 1);
	if(ret != 1) {
		log_err("failed to retrieve work completion");
		return ret;
	}
	debug("Completed reading remote address\n");
	show_rdma_buffer_attr(&(pcc->server_md_attr));
	return 0;
}

int client_disconnect(PEARS_CLT_CTX *pcc)
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
	

	rdma_buffer_deregister(pcc->server_md_mr);
	rdma_buffer_deregister(pcc->kvs_request_attr_mr);
	rdma_buffer_deregister(pcc->kvs_request_mr);
	rdma_buffer_deregister(pcc->remote_mr);

	free(pcc->kvs_request);
	free(pcc->remote);

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
//TODO: maybe write an alternate that uses polling on 1 or more
//		completion channels using example on:
//			https://linux.die.net/man/3/ibv_get_cq_event
// notes section mentions amortizing acking WC events
int retrieve_work_completion_events(struct ibv_comp_channel *cc, struct ibv_wc *wc, int max_wc)
{
	struct ibv_cq *cq_p = NULL;
	void *ctx = NULL;
	int ret = -1, total_wc = 0;

	/* ack cq events regardless of status, important for cleanup later*/
	ret = ibv_get_cq_event(cc, &cq_p, &ctx);
	if(ret) {
		log_err("ibv_get_cq_event() failed");
		ibv_ack_cq_events(cq_p, 1);
		return -errno;
	}
	ibv_ack_cq_events(cq_p, 1);

	ret = ibv_req_notify_cq(cq_p, 0);
	if(ret) {
		log_err("ibv_req_notify_cq() failed");
		return -errno;
	}

	do {
		ret = ibv_poll_cq(cq_p, max_wc-total_wc, wc + total_wc);
		if(ret < 0) {
			log_err("ibv_poll_cq() failed");
			return -ret;
		}
		total_wc += ret;
	} while(total_wc < max_wc);

	ret = validate_wcs(wc, total_wc);
	if(ret) return ret;
	
	return 0;
}

int validate_wcs(struct ibv_wc *wc, int tot)
{
	int i;
	for(i = 0; i < tot; i++) {
		if(wc[i].status != IBV_WC_SUCCESS) {
			log_err("WC error: %s [OPCODE: %d]", 
				ibv_wc_status_str(wc[i].status),
				wc[i].opcode);
			return -(wc[i].status);
		}
	}
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
	int n_dev;
	struct ibv_device** ibv_devs = ibv_get_device_list(&n_dev);

	for(int i = 0; i < n_dev; ++i) {
		printf("[IBV_DEV] KIB dev: %s, Uverbs dev: %s, dev path=%s, class dev: %s\n", 
						ibv_devs[i]->name, 
						ibv_devs[i]->dev_name,
						ibv_devs[i]->dev_path,
						ibv_devs[i]->ibdev_path);
	}
}

/* Code acknowledgment: rping.c from librdmacm/examples */
int get_addr(char *dst, struct sockaddr *addr)
{
	struct addrinfo *res;
	int ret = -1;
	ret = getaddrinfo(dst, NULL, NULL, &res);
	if (ret) {
		log_err("getaddrinfo failed - invalid hostname or IP address\n");
		return ret;
	}
	memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
	freeaddrinfo(res);
	return ret;
}
