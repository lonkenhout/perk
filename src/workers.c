#include "workers.h"


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

void *worker_sd_sd(void *args)
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
	debug("worker starting polling\n");
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
		char *buf = (char*)pcc->imm_data->addr;
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

