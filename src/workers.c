#include "workers.h"

int handle_request(PEARS_CLIENT_CONN *pcc, struct request *request, struct request *response)
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
			handle_request_get(request, response);
			break;
		case PUT:
			debug("Doing put request\n");
			handle_request_put(request, response);
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

void handle_request_get(struct request *request, struct request *response)
{
	
	char *val = pears_kv_get(request->key);
	if(val == NULL) {
		response->type = RESPONSE_EMPTY;
	} else {
		if(request != response) strcpy(response->key, request->key);
		strcpy(response->val, val);
		response->type = RESPONSE_OK;
	}
}

void handle_request_put(struct request *request, struct request *response)
{
	pears_kv_insert(request->key, request->val);
	//TODO: setup way to properly check for failure
	if(1) {
		response->type = RESPONSE_OK;
	}
}

void handle_request_exit(struct request *request, struct request *response)
{
	response->type = EXIT_OK;
}

int prepare_response_server(PEARS_CLIENT_CONN *pcc)
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

int prepare_request_server(PEARS_CLIENT_CONN *pcc)
{
	int ret = 0;
	switch(pcc->config.client) {
		case RDMA_COMBO_SD:
			ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		    if(ret) log_err("rdma_post_recv() failed");
			pcc->sd_request.type = EMPTY;
			break;
		case RDMA_COMBO_WRIMM:
			ret = rdma_post_recv(pcc->imm_data, pcc->qp);
			if(ret) log_err("failed to ACK cm event");
		default:
			break;
	}
	return ret;
}

int recv_request(PEARS_CLIENT_CONN *pcc)
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

int send_response(PEARS_CLIENT_CONN *pcc)
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

int prep_next_iter(PEARS_CLIENT_CONN *pcc)
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
	PEARS_CLIENT_CONN *pcc = (PEARS_CLIENT_CONN *)args;
	
	pcc->sd_response.type = RESPONSE_OK;
	
	ret = prepare_request_server(pcc);
	if(ret) return NULL;
	ret = prepare_response_server(pcc);
	if(ret) return NULL;

	struct ibv_wc wc;
	while(1) {
		//printf("receiving request\n");
		ret = recv_request(pcc);
		if(ret) return NULL;

		//printf("handling request\n");
		req = handle_request(pcc, &(pcc->sd_request), &(pcc->sd_response));
		
		//printf("sending reesponse\n");
		ret = send_response(pcc);
		if(ret) return NULL;

		//printf("prepping next iter\n");
		ret = prep_next_iter(pcc);
		if(ret) return NULL;
		
		if(req == 1) break;
		pcc->ops++;
	}
	return NULL;
}

