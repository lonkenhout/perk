#include "workers.h"

int handle_request(PERK_CLIENT_CONN *pcc, struct request *request, struct request *response)
{
	/* if the client reads the result from server memory, it should be stored in the request memory */
	switch(pcc->config.server) {
		case RDMA_COMBO_RD:
			response = request;
			break;
		default:
			break;
	}
	int ret = 0;
	switch(request->type) {
		case GET:
			debug("Doing get request\n");
			handle_request_get(pcc, request, response);
			break;
		case PUT:
			debug("Doing put request\n");
			handle_request_put(pcc, request, response);
			break;
		case EXIT:
			debug("Doing exit request\n");
			handle_request_exit(request, response);
			ret = 1;
			break;
		default:
			ret = -1;
			break;
	}
	switch(pcc->config.server) {
        case RDMA_COMBO_RD:
            break;
        default:
			request->type = EMPTY;
            break;
    }
	return ret;
}

void handle_request_get(PERK_CLIENT_CONN *pcc, struct request *request, struct request *response)
{
	
	//char *val = pears_kv_get(request->key);
	//int ret = pears_kv_get(request->key, response->val);
	int ret = ck_hash_table_get(pcc->ct, request->key, response->val);
	if(ret) {
		response->type = RESPONSE_EMPTY;
	} else {
		if(request != response) strcpy(response->key, request->key);
		//strcpy(response->val, val);
		response->type = RESPONSE_OK;
	}
}

void handle_request_put(PERK_CLIENT_CONN *pcc, struct request *request, struct request *response)
{
	//pears_kv_insert(request->key, request->val);
	ck_hash_table_insert(pcc->ct, request->key, request->val);
	//TODO: setup way to properly check for failure
	response->type = RESPONSE_OK;
}

void handle_request_exit(struct request *request, struct request *response)
{
	response->type = EXIT_OK;
}

int prepare_response_server(PERK_CLIENT_CONN *pcc)
{
	int ret = 0;
	switch(pcc->config.server) {
		case RDMA_COMBO_SD:
			rdma_send_wr_prepare(&(pcc->send_wr), &(pcc->snd_sge), pcc->sd_response_mr);
			break;
		case RDMA_COMBO_WR:
			rdma_write_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->sd_response_mr, pcc->md_attr);
		default:
			break;
	}
	return ret;
}

int prepare_request_server(PERK_CLIENT_CONN *pcc)
{
	int ret = 0;
	switch(pcc->config.client) {
		case RDMA_COMBO_SD:
			ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		    if(ret) log_err("rdma_post_recv() failed");
			break;
		case RDMA_COMBO_WRIMM:
			ret = rdma_post_recv(pcc->imm_data, pcc->qp);
			if(ret) log_err("failed to ACK cm event");
		default:
			break;
	}
	return ret;
}

int recv_request(PERK_CLIENT_CONN *pcc)
{
	int ret = -1;
	struct ibv_wc wc;
	switch(pcc->config.client) {
		case RDMA_COMBO_SD: case RDMA_COMBO_WRIMM:
			ret = rdma_spin_cq(pcc->cq, &wc, 1);
	        if(ret != 1) log_err("retrieve_work_completion_events() failed");
			ret ^= 1;
			break;
		case RDMA_COMBO_WR:
			while(pcc->sd_request.type == EMPTY || pcc->sd_request.type == RESPONSE_OK || pcc->sd_request.type == RESPONSE_EMPTY);
			ret = 0;
		default:
			break;
	}
	return ret;
}

int send_response(PERK_CLIENT_CONN *pcc)
{
	int ret = -1;
	struct ibv_wc wc;
	switch(pcc->config.client) {
		case RDMA_COMBO_WR:
			if(pcc->config.server != RDMA_COMBO_RD) pcc->sd_request.type = EMPTY;
			break;
		default:
			break;
	}
	switch(pcc->config.server) {
		case RDMA_COMBO_SD:
			ret = rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
	        if(ret) log_err("rdma_post_send() failed");
	        
	        ret = rdma_spin_cq(pcc->cq, &wc, 1);
	        if(ret != 1) {
				log_err("retrieve_work_completion_events() failed, %d", ret);
			}
			ret ^= 1;
			break;
		case RDMA_COMBO_WR:
			ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
            if(ret) log_err("rdma_write_c2s() failed");

			ret = rdma_spin_cq(pcc->cq, &wc, 1);
            if(ret != 1) log_err("retrieve_work_completion_events() failed");
            ret ^= 1;
		case RDMA_COMBO_RD:
			ret = 0;
			break;
		default:
			break;
	}
	return ret;
}

int prep_next_iter(PERK_CLIENT_CONN *pcc)
{
	int ret = -1;
	switch(pcc->config.client) {
		case RDMA_COMBO_SD:
			ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
	        if(ret) log_err("rdma_post_recv() failed");
			break;
		case RDMA_COMBO_WR:
			ret = 0;
			break;
		case RDMA_COMBO_WRIMM:
			ret = rdma_post_recv(pcc->imm_data, pcc->qp);
			if(ret) log_err("rdma_post_recv() failed");
			break;
		default:
			break;
	}
	return ret;
}

void *worker(void *args)
{
	int ret, req;
	struct timeval s, e;
	double time;
	PERK_CLIENT_CONN *pcc = (PERK_CLIENT_CONN *)args;
	
	pcc->sd_response.type = RESPONSE_OK;
	ret = prepare_request_server(pcc);
	if(ret) return NULL;
	ret = prepare_response_server(pcc);
	if(ret) return NULL;


	struct ibv_wc wc;
	long last_rid = -1;
	int retry_count = 0;
	while(1) {
		ret = recv_request(pcc);
		if(pcc->ops == 0) printf("%lu: recv\n", pcc);
		if(ret) return NULL;
		if (last_rid == pcc->sd_request.rid){
			continue;
		} 
		last_rid = pcc->sd_request.rid;
		
		req = handle_request(pcc, &(pcc->sd_request), &(pcc->sd_response));
		ret = send_response(pcc);
		if(ret) return NULL;

		ret = prep_next_iter(pcc);
		if(ret) return NULL;
		
		if(req == 1) break;
		pcc->ops++;
	}
	return NULL;
}

