#include "workers.h"

int handle_request(struct request *request, struct request *response)
{
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
			response->type = RESPONSE_ERR;
			ret = -1;
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
		response->type = RESPONSE_OK;
		strcpy(response->key, request->key);
		strcpy(response->val, val);
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
			break;
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
		case RDMA_COMBO_SD:
			ret = rdma_spin_cq(pcc->cq, &wc, 1);
	        if(ret != 1) log_err("retrieve_work_completion_events() failed");
			ret ^= 1;
			break;
		default:
			break;
	}
	return ret;
}

int send_response(PEARS_CLIENT_CONN *pcc)
{
	int ret = -1;
	struct ibv_wc wc;
	switch(pcc->config.server) {
		case RDMA_COMBO_SD:
			ret = rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
	        if(ret) log_err("rdma_post_send() failed");
	        
	        ret = rdma_spin_cq(pcc->cq, &wc, 1);
	        if(ret != 1) log_err("retrieve_work_completion_events() failed");
			ret ^= 1;
			break;
		default:
			break;
	}
	return ret;
}

int prep_next_iter(PEARS_CLIENT_CONN *pcc)
{
	int ret = -1;
	switch(pcc->config.server) {
		case RDMA_COMBO_SD:
			ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
	        if(ret) log_err("rdma_post_recv() failed");
			break;
		default:
			break;
	}
	return ret;
}

void *worker_wr_sd(void *args)
{
	int res, sem_val = 0;
	char k[MAX_LINE_LEN] = {0,};
	char v[MAX_LINE_LEN] = {0,};
	memset(k, 0, sizeof(k));
	memset(v, 0, sizeof(v));

	struct timeval s, e;

	PEARS_CLIENT_CONN *pcc = (PEARS_CLIENT_CONN *)args;
	while(1){
		char *buf = (char*)pcc->server_buf->addr;
		res = parse_request(buf, 
							k, MAX_KEY_SIZE, 
							v, MAX_VAL_SIZE);
		if(res == GET) {
			debug("Get request received: {%s}\n", k);
			struct ibv_wc wc;
			/* get request */
			char *val = pears_kv_get(k);
			if(val == NULL) {
				val = "EMPTY";
			}

			/* clean up and send response */
			memset(pcc->server_buf->addr, 0, pcc->server_buf->length);
			strcpy((char*)pcc->response_mr->addr, val);
			res = rdma_post_send(pcc->response_mr, pcc->qp);
			if(res) {
				log_err("rdma_post_send() failed");
				return NULL;
			}

			/* wait for completion */
			debug("Sent result, waiting for completion\n");
			res = rdma_spin_cq(pcc->cq, &wc, 1);
			if(res != 1) {
				log_err("retrieve_work_completion_events() failed");
				return NULL;
			}
			memset(pcc->response_mr->addr, 0 , pcc->response_mr->length);
			debug("Completed send\n");
			pcc->ops++;
		} else if(res == PUT) {
			debug("Put request received: {%s:%s}\n", k, v);
			struct ibv_wc wc;
			/* put request */
			pears_kv_insert(k, v);
			
			/* cleanup and send response */
			memset(pcc->server_buf->addr, 0, pcc->server_buf->length);
			strcpy((char*)pcc->response_mr->addr, "INSERTED");
			res = rdma_post_send(pcc->response_mr, pcc->qp);
			if(res) {
				log_err("rdma_post_send() failed");
				return NULL;
			}

			/* wait for completion */
			debug("Sent result, waiting for completion\n");
			res = rdma_spin_cq(pcc->cq, &wc, 1);
			if(res != 1) {
				log_err("retrieve_work_completion_events() failed");
				return NULL;
			}
			memset(pcc->response_mr->addr, 0 , pcc->response_mr->length);
			pcc->ops++;

		} else if(res == EXIT) {
			struct ibv_wc wc;
			strcpy((char*)pcc->response_mr->addr, "OK");
			
			res = rdma_post_send(pcc->response_mr, pcc->qp);

			if(res) {
				log_err("rdma_post_send() failed");
				return NULL;
			}
			debug("Sent result, waiting for completion\n");
			res = rdma_spin_cq(pcc->cq, &wc, 1);
			if(res != 1) {
				log_err("retrieve_work_completion_events() failed");
				return NULL;
			}
			break;
		}
	}
	return NULL;
}

void *worker_wrimm_sd(void *args)
{
	int res, sem_val = 0;
	char k[MAX_LINE_LEN] = {0,};
	char v[MAX_LINE_LEN] = {0,};
	memset(k, 0, sizeof(k));
	memset(v, 0, sizeof(v));

	struct timeval s, e;
	


	//rdma_post_send_reuse(pcc->send_wr, pcc->qp);

	PEARS_CLIENT_CONN *pcc = (PEARS_CLIENT_CONN *)args;

	rdma_send_wr_prepare(&(pcc->send_wr), &(pcc->snd_sge), pcc->response_mr);

	while(1){
		struct ibv_wc wc;
		/* we expect the completion of write with IMM */
		res = rdma_spin_cq(pcc->cq, &wc, 1);
		if(res != 1) {
			log_err("retrieve_work_completion_events() failed");
			return NULL;
		}
		/* prepost recv for next request */
		res = rdma_post_recv(pcc->imm_data, pcc->qp);
		if(res) {
			log_err("failed to ACK cm event");
			return NULL;
		}
		char *buf = (char*)pcc->server_buf->addr;
		res = parse_request(buf, 
							k, MAX_KEY_SIZE, 
							v, MAX_VAL_SIZE);
		
		
		if(res == GET) {
			debug("Get request received: {%s}\n", k);
			/* get request */
			char *val = pears_kv_get(k);
			if(val == NULL) {
				val = "EMPTY";
			}

			/* clean up and send response */
			memset(pcc->server_buf->addr, 0, pcc->server_buf->length);
			strcpy((char*)pcc->response_mr->addr, val);
			//res = rdma_post_send(pcc->response_mr, pcc->qp);
			rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
			if(res) {
				log_err("rdma_post_send() failed");
				return NULL;
			}

			/* wait for completion */
			debug("Sent result, waiting for completion\n");
			res = rdma_spin_cq(pcc->cq, &wc, 1);
			if(res != 1) {
				log_err("retrieve_work_completion_events() failed");
				return NULL;
			}
			memset(pcc->response_mr->addr, 0 , pcc->response_mr->length);
			debug("Completed send\n");
			pcc->ops++;
		} else if(res == PUT) {
			debug("Put request received: {%s:%s}\n", k, v);
			/* put request */
			pears_kv_insert(k, v);
			
			/* cleanup and send response */
			memset(pcc->server_buf->addr, 0, pcc->server_buf->length);
			strcpy((char*)pcc->response_mr->addr, "INSERTED");
			//res = rdma_post_send(pcc->response_mr, pcc->qp);
			rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
			if(res) {
				log_err("rdma_post_send() failed");
				return NULL;
			}

			/* wait for completion */
			debug("Sent result, waiting for completion\n");
			res = rdma_spin_cq(pcc->cq, &wc, 1);
			if(res != 1) {
				log_err("retrieve_work_completion_events() failed");
				return NULL;
			}
			memset(pcc->response_mr->addr, 0 , pcc->response_mr->length);
			pcc->ops++;

		} else if(res == EXIT) {
			struct ibv_wc wc;
			strcpy((char*)pcc->response_mr->addr, "OK");
			
			res = rdma_post_send(pcc->response_mr, pcc->qp);

			if(res) {
				log_err("rdma_post_send() failed");
				return NULL;
			}
			debug("Sent result, waiting for completion\n");
			res = rdma_spin_cq(pcc->cq, &wc, 1);
			if(res != 1) {
				log_err("retrieve_work_completion_events() failed");
				return NULL;
			}
			break;
		}
		//rdma_clear_cq(pcc->cq);
	}
	return NULL;
}

void *worker(void *args)
{
	int ret, req;
	struct timeval s, e;
	double time;
	PEARS_CLIENT_CONN *pcc = (PEARS_CLIENT_CONN *)args;
	
	pcc->sd_response.type = RESPONSE_OK;
	//TODO: move to rdma.h
	pcc->sd_response_mr = rdma_buffer_register(pcc->pd, &(pcc->sd_response), sizeof(pcc->sd_response), PERM_L_RW);
	
	ret = prepare_request_server(pcc);
	if(ret) return NULL;
	ret = prepare_response_server(pcc);
	if(ret) return NULL;

	struct ibv_wc wc;
	while(1) {
		printf("receiving request\n");
		ret = recv_request(pcc);
		if(ret) return NULL;

		printf("handling request\n");
		req = handle_request(&(pcc->sd_request), &(pcc->sd_response));
		
		printf("sending reesponse\n");
		ret = send_response(pcc);
		if(ret) return NULL;

		printf("prepping next iter\n");
		ret = prep_next_iter(pcc);
		if(ret) return NULL;

		if(req == 1) break;
		pcc->ops++;
	}
	return NULL;
}

void *worker_sd_sd(void *args)
{
	int res, sem_val = 0;
	char k[MAX_LINE_LEN] = {0,};
	char v[MAX_LINE_LEN] = {0,};
	memset(k, 0, sizeof(k));
	memset(v, 0, sizeof(v));
	//char *k = NULL, *v = NULL;

	struct timeval s, e;
	PEARS_CLIENT_CONN *pcc = (PEARS_CLIENT_CONN *)args;

	
	pcc->sd_response.type = RESPONSE_OK;
	
	pcc->sd_response_mr = rdma_buffer_register(pcc->pd, &(pcc->sd_response), sizeof(pcc->sd_response), PERM_L_RW);


	rdma_send_wr_prepare(&(pcc->send_wr), &(pcc->snd_sge), pcc->sd_response_mr);
	res = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
	if(res) {
		log_err("rdma_post_recv() failed");
		return NULL;
	}
	
	debug("worker starting polling\n");
	struct timeval start, end;
	double time;
	int req;
	while(1){
		struct ibv_wc wc;
		// we expect the completion of write with IMM
		res = rdma_spin_cq(pcc->cq, &wc, 1);
		if(res != 1) {
			log_err("retrieve_work_completion_events() failed");
			return NULL;
		}
		// check the type of the request
		req = handle_request(&(pcc->sd_request), &(pcc->sd_response));
	
		debug("Handled request\n");	
		res = rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
		if(res) {
			log_err("rdma_post_send() failed");
			return NULL;
		}

		// wait for completion
		debug("Sent result, waiting for completion\n");
		res = rdma_spin_cq(pcc->cq, &wc, 1);
		if(res != 1) {
			log_err("retrieve_work_completion_events() failed");
			return NULL;
		}
		res = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		if(res) {
			log_err("rdma_post_recv() failed");
			return NULL;
		}
		debug("Done spinning, moving on\n");
		
		if(req == 1) {
			break;
		}
		pcc->ops++;

	}
	return NULL;
}

void *worker_wr_wr(void *args)
{
	PEARS_CLIENT_CONN *pcc = (PEARS_CLIENT_CONN *)args;
	int res, sem_val = 0;
	char k[MAX_LINE_LEN] = {0,};
	char v[MAX_LINE_LEN] = {0,};
	memset(k, 0, sizeof(k));
	memset(v, 0, sizeof(v));

	struct timeval s, e;

	rdma_write_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->response_mr, pcc->md_attr);

	while(1){
		char *buf = (char*)pcc->server_buf->addr;
		res = parse_request(buf, 
							k, MAX_KEY_SIZE, 
							v, MAX_VAL_SIZE);
		if(res == GET) {
			debug("Get request received: {%s}\n", k);
			struct ibv_wc wc;
			/* get request */
			char *val = pears_kv_get(k);
			if(val == NULL) {
				val = "EMPTY";
			}

			/* clean up and send response */
			memset(pcc->server_buf->addr, 0, pcc->server_buf->length);
			strcpy((char*)pcc->response_mr->addr, val);
			/* write to the server */
			res = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
			if(res) {
				log_err("rdma_write_c2s() failed");
				return NULL;
			}

			/* wait for completion */
			debug("Sent result, waiting for completion\n");
			res = rdma_spin_cq(pcc->cq, &wc, 1);
			if(res != 1) {
				log_err("retrieve_work_completion_events() failed");
				return NULL;
			}
			memset(pcc->response_mr->addr, 0 , pcc->response_mr->length);
			debug("Completed send\n");
			pcc->ops++;
		} else if(res == PUT) {
			debug("Put request received: {%s:%s}\n", k, v);
			struct ibv_wc wc;
			/* put request */
			pears_kv_insert(k, v);
			
			/* cleanup and send response */
			memset(pcc->server_buf->addr, 0, pcc->server_buf->length);
			strcpy((char*)pcc->response_mr->addr, "INSERTED");
			res = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
			if(res) {
				log_err("rdma_write_c2s() failed");
				return NULL;
			}

			/* wait for completion */
			debug("Sent result, waiting for completion\n");
			res = rdma_spin_cq(pcc->cq, &wc, 1);
			if(res != 1) {
				log_err("retrieve_work_completion_events() failed");
				return NULL;
			}
			memset(pcc->response_mr->addr, 0 , pcc->response_mr->length);
			pcc->ops++;

		} else if(res == EXIT) {
			struct ibv_wc wc;
			strcpy((char*)pcc->response_mr->addr, "OK");
			
			res = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
			if(res) {
				log_err("rdma_write_c2s() failed");
				return NULL;
			}
			debug("Sent result, waiting for completion\n");
			res = rdma_spin_cq(pcc->cq, &wc, 1);
			if(res != 1) {
				log_err("retrieve_work_completion_events() failed");
				return NULL;
			}
			break;
		}
	}
	return NULL;
}

void *worker_wr_rd(void *args)
{
	PEARS_CLIENT_CONN *pcc = (PEARS_CLIENT_CONN *)args;
	int res, sem_val = 0;
	char k[MAX_LINE_LEN] = {0,};
	char v[MAX_LINE_LEN] = {0,};
	memset(k, 0, sizeof(k));
	memset(v, 0, sizeof(v));

	struct timeval s, e;

	rdma_write_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->response_mr, pcc->md_attr);

	while(1){
		char *buf = (char*)pcc->server_buf->addr;
		res = parse_request(buf, 
							k, MAX_KEY_SIZE, 
							v, MAX_VAL_SIZE);
		if(res == GET) {
			debug("Get request received: {%s}\n", k);
			struct ibv_wc wc;
			/* get request */
			char *val = pears_kv_get(k);
			if(val == NULL) {
				val = "EMPTY";
			}

			/* clean up and set the response, then continue as normal */
			memset(pcc->server_buf->addr, 0, pcc->server_buf->length);
			strcpy(buf, "R:");
			strcat(buf, val);
			debug("set result to %s\n", buf);
			//strcpy((char*)pcc->response_mr->addr, val);
			debug("Set response, now leaving\n");
			pcc->ops++;
		} else if(res == PUT) {
			debug("Put request received: {%s:%s}\n", k, v);
			struct ibv_wc wc;
			/* put request */
			pears_kv_insert(k, v);
			
			/* cleanup and send response */
			memset(pcc->server_buf->addr, 0, pcc->server_buf->length);
			//strcpy((char*)pcc->response_mr->addr, "INSERTED");
			strcpy(buf, "R:INSERTED");
			debug("Set response, now leaving\n");
			pcc->ops++;

		} else if(res == EXIT) {
			struct ibv_wc wc;
			//strcpy((char*)pcc->response_mr->addr, "OK");
			strcpy(buf, "OK");
			/*wait a little bit so the client can read the response, then break and exit */
			usleep(1000);	
			break;
		}
	}
	return NULL;
}


