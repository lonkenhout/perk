
#include "clients.h"


int prep_request(PEARS_CLT_CTX *pcc, struct request *request)
{
	int req = -1, ret = -1;
	if(pcc->using_file) {
		get_input(&(pcc->kvs_request));
		
		req = parse_request(pcc->kvs_request, 
							request->key, MAX_KEY_SIZE,
							request->val, MAX_VAL_SIZE);
		ret = validate_request(req, pcc->kvs_request);
	} else {
		request->type = GET;
		strcpy(request->key, "key123");
		ret = 0;
	}
	return ret;
}

int validate_request(int req, char *input)
{
	int ret = 0;
	switch(req) {
		case PUT: case GET: case EXIT:
			break;
		default:
			fprintf(stderr, "error: request couldnt be validated, check input format:\n\t%s\n", input);
			ret = 1;
	}
	return ret;
}



int client_wr_wr(PEARS_CLT_CTX *pcc)
{
	int ret = -1;
	/* set the CQ to nonblock */
	set_comp_channel_non_block(pcc->comp_channel);

	if(pcc->using_file) {
		get_input(&(pcc->kvs_request), MAX_LINES);
	} else {
		strcpy(pcc->kvs_request, "G:key123");
	}

	/* prepare read and write */
	rdma_recv_wr_prepare(&(pcc->recv_wr), &(pcc->rec_sge), pcc->response_mr);
	rdma_write_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->kvs_request_mr, pcc->server_md_attr);


	int count = 0;
	/* do the same request 10 million times */
	while(count < pcc->max_reqs) {
		/* post receive, we always expect a response */
		/*ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		if(ret) {
			log_err("rdma_post_recv() failed");
			return 1;
		}*/

		/* write to the server */
		ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
		if(ret) {
			log_err("rdma_write_c2s() failed");
			return 1;
		}

		struct ibv_wc wc;
			
		debug("Waiting for completion \n");

		ret = rdma_spin_cq(pcc->cq, &wc, 1);
		if(ret != 1) {
			log_err("rdma_poll_cq() failed");
			exit(1);
		}

		/* spin while response hasnt been send back yet */
		while(pcc->response[0] == 0);

		debug("%s:%s\n", pcc->kvs_request, pcc->response);

		memset(pcc->response, 0, sizeof(pcc->response_mr->length));
		count++;
	}

	/* send exit to server */
	strcpy(pcc->kvs_request, "E");
	/* post receive, we always expect a response */
	ret = rdma_post_recv(pcc->response_mr, pcc->qp);
	if(ret) {
		log_err("rdma_post_recv() failed");
		return 1;
	}

	/* write to the server */
	ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
	if(ret) {
		log_err("rdma_write_c2s() failed");
		return 1;
	}

	struct ibv_wc wc;
		
	debug("Waiting for completion \n");

	ret = rdma_spin_cq(pcc->cq, &wc, 1);
	if(ret != 1) {
		log_err("rdma_poll_cq() failed");
		exit(1);
	}

	return 0;
}

int client_wr_sd(PEARS_CLT_CTX *pcc)
{
	int ret = -1;
	/* set the CQ to nonblock */
	set_comp_channel_non_block(pcc->comp_channel);

	if(pcc->using_file) {
		get_input(&(pcc->kvs_request), MAX_LINES);
	} else {
		strcpy(pcc->kvs_request, "G:key123");
	}

	/* prepare read and write */
	rdma_recv_wr_prepare(&(pcc->recv_wr), &(pcc->rec_sge), pcc->response_mr);
	rdma_write_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->kvs_request_mr, pcc->server_md_attr);


	int count = 0;
	/* do the same request 10 million times */
	while(count < pcc->max_reqs) {
		/* post receive, we always expect a response */
		ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		if(ret) {
			log_err("rdma_post_recv() failed");
			return 1;
		}

		/* write to the server */
		ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
		if(ret) {
			log_err("rdma_write_c2s() failed");
			return 1;
		}

		struct ibv_wc wc;
			
		debug("Waiting for completion \n");

		ret = rdma_spin_cq(pcc->cq, &wc, 2);
		if(ret != 2) {
			log_err("rdma_poll_cq() failed");
			exit(1);
		}
		debug("%s:%s\n", pcc->kvs_request, pcc->response);

		memset(pcc->response, 0, sizeof(pcc->response_mr->length));
		count++;
	}

	/* send exit to server */
	strcpy(pcc->kvs_request, "E");
	/* post receive, we always expect a response */
	ret = rdma_post_recv(pcc->response_mr, pcc->qp);
	if(ret) {
		log_err("rdma_post_recv() failed");
		return 1;
	}

	/* write to the server */
	ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
	if(ret) {
		log_err("rdma_write_c2s() failed");
		return 1;
	}

	struct ibv_wc wc;
		
	debug("Waiting for completion \n");

	ret = rdma_spin_cq(pcc->cq, &wc, 1);
	if(ret != 1) {
		log_err("rdma_poll_cq() failed");
		exit(1);
	}

	return 0;
}

int client_wrimm_sd(PEARS_CLT_CTX *pcc)
{
	int ret = -1;
	/* set the CQ to nonblock */
	set_comp_channel_non_block(pcc->comp_channel);

	if(pcc->using_file) {
		get_input(&(pcc->kvs_request), MAX_LINES);
	} else {
		strcpy(pcc->kvs_request, "G:key123");
	}

	rdma_recv_wr_prepare(&(pcc->recv_wr), &(pcc->rec_sge), pcc->response_mr);
	
	//rdma_send_wr_prepare(pcc->send_wr, pcc->snd_sge, pcc->kvs_request_mr);
	//rdma_post_send_reuse(pcc->send_wr, pcc->qp);
	//rdma_write_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->kvs_request_mr, pcc->server_md_attr);
	rdma_write_imm_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->kvs_request_mr, pcc->server_md_attr);

	int count = 0;
	/* do the same request max_reqs times */
	while(count < pcc->max_reqs) {
		struct ibv_wc wc;
		/* post receive, we always expect a response */
		//ret = rdma_post_recv(pcc->response_mr, pcc->qp);
		ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		if(ret) {
			log_err("rdma_post_recv() failed");
			return 1;
		}

		/* write to the server */
		//ret = rdma_write_c2s_non_block(pcc);
		ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
		if(ret) {
			log_err("rdma_write_c2s() failed");
			return 1;
		}

		ret = rdma_spin_cq(pcc->cq, &wc, 2);
		if(ret != 2) {
			log_err("rdma_poll_cq() failed");
			exit(1);
		}
			
		debug("Waiting for completion \n");

		/*ret = rdma_spin_cq(pcc->cq, &wc, 1);
		if(ret != 1) {
			log_err("rdma_poll_cq() failed");
			exit(1);
		}*/
		debug("%s:%s\n", pcc->kvs_request, pcc->response);

		memset(pcc->response, 0, sizeof(pcc->response_mr->length));
		count++;
	}

	/* send exit to server */
	strcpy(pcc->kvs_request, "E");
	/* post receive, we always expect a response */
	ret = rdma_post_recv(pcc->response_mr, pcc->qp);
	debug("writing exit to server\n");
	if(ret) {
		log_err("rdma_post_recv() failed");
		return 1;
	}

	/* write to the server */
	//ret = rdma_write_c2s_non_block(pcc);
	ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
	if(ret) {
		log_err("rdma_write_c2s() failed");
		return 1;
	}

	struct ibv_wc wc;
		
	debug("Waiting for completion \n");

	ret = rdma_spin_cq(pcc->cq, &wc, 1);
	if(ret != 1) {
		log_err("rdma_poll_cq() failed");
		exit(1);
	}

	return 0;
}

int client(PEARS_CLT_CTX *pcc)
{
	int ret = -1;
	
	pcc->sd_request_mr = rdma_buffer_register(pcc->pd, &(pcc->sd_request), sizeof(pcc->sd_request), PERM_L_RW);
	pcc->sd_response_mr = rdma_buffer_register(pcc->pd, &(pcc->sd_response), sizeof(pcc->sd_response), PERM_L_RW);

	rdma_send_wr_prepare(&(pcc->send_wr), &(pcc->snd_sge), pcc->sd_request_mr);
	rdma_recv_wr_prepare(&(pcc->recv_wr), &(pcc->rec_sge), pcc->sd_response_mr);

	struct timeval start;
	struct timeval end, t_s, t_e;
	//usleep(500);
	
	ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
	if(ret) {
		log_err("rdma_post_recv() failed");
		return 1;
	}

	int count = 0;
	printf("doing %d requests\n", pcc->max_reqs);
	/* do the same request max_reqs times */
	get_time(&start);
	while(count < pcc->max_reqs) {
		ret = prep_request(pcc, &(pcc->sd_request));
		if(ret) {
			fprintf(stderr, "error, request couldnt be sent\n");
			exit(1);
		}

		//get_time(&t_s);
		struct ibv_wc wc;
		//printf("Sending request: %s\n", pcc->sd_request.key);

		
		/* write to the server */
		ret = rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
		if(ret) {
			log_err("rdma_write_c2s() failed");
			return 1;
		}
		ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		if(ret) {
			log_err("rdma_post_recv() failed");
			return 1;
		}
		
		ret = rdma_spin_cq(pcc->cq, &wc, 2);
		if(ret != 2) {
			log_err("rdma_poll_cq() failed");
			exit(1);
		}
		//printf("Got response: %s:%s\n", pcc->sd_response.key, pcc->sd_response.val);
			
		debug("Waiting for completion \n");

		count++;
	}
	get_time(&end);
	double time = compute_time(start, end, SCALE_MSEC);
	printf("== processed %d requests in %.0f ms\n",
	                pcc->max_reqs, time);
	printf("== ops/s: %.1f\n", pcc->max_reqs/(time/1000));
	/* send exit to server */
	//strcpy(pcc->kvs_request, "E");
	pcc->sd_request.type = EXIT;
	
	/* post receive, we always expect a response */
	ret = rdma_post_recv(pcc->response_mr, pcc->qp);
	debug("writing exit to server\n");
	if(ret) {
		log_err("rdma_post_recv() failed");
		return 1;
	}

	/* write to the server */
	//ret = rdma_write_c2s_non_block(pcc);
	//ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
	ret = rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
	if(ret) {
		log_err("rdma_write_c2s() failed");
		return 1;
	}

	struct ibv_wc wc;
		
	debug("Waiting for completion \n");

	ret = rdma_spin_cq(pcc->cq, &wc, 1);
	if(ret != 1) {
		log_err("rdma_poll_cq() failed");
		exit(1);
	}
	rdma_buffer_deregister(pcc->sd_request_mr);
	rdma_buffer_deregister(pcc->sd_response_mr);	

	return 0;

}

int client_sd_sd(PEARS_CLT_CTX *pcc)
{
	int ret = -1;
	
	//pcc->sd_request.type = GET;
	//strcpy(pcc->sd_request.key, "key123");
	
	
	pcc->sd_request_mr = rdma_buffer_register(pcc->pd, &(pcc->sd_request), sizeof(pcc->sd_request), PERM_L_RW);
	pcc->sd_response_mr = rdma_buffer_register(pcc->pd, &(pcc->sd_response), sizeof(pcc->sd_response), PERM_L_RW);

	rdma_send_wr_prepare(&(pcc->send_wr), &(pcc->snd_sge), pcc->sd_request_mr);
	rdma_recv_wr_prepare(&(pcc->recv_wr), &(pcc->rec_sge), pcc->sd_response_mr);

	struct timeval start;
	struct timeval end, t_s, t_e;
	usleep(500);
	
	ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
	if(ret) {
		log_err("rdma_post_recv() failed");
		return 1;
	}

	int count = 0;
	printf("doing %d requests\n", pcc->max_reqs);
	/* do the same request max_reqs times */
	get_time(&start);
	while(count < pcc->max_reqs) {
		ret = prep_request(pcc, &(pcc->sd_request));
		if(ret) {
			fprintf(stderr, "error, request couldnt be sent\n");
			exit(1);
		}

		//get_time(&t_s);
		struct ibv_wc wc;
		//printf("Sending request: %s\n", pcc->sd_request.key);

		
		/* write to the server */
		ret = rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
		if(ret) {
			log_err("rdma_write_c2s() failed");
			return 1;
		}
		ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		if(ret) {
			log_err("rdma_post_recv() failed");
			return 1;
		}
		
		ret = rdma_spin_cq(pcc->cq, &wc, 2);
		if(ret != 2) {
			log_err("rdma_poll_cq() failed");
			exit(1);
		}
		//printf("Got response: %s:%s\n", pcc->sd_response.key, pcc->sd_response.val);
			
		debug("Waiting for completion \n");

		count++;
	}
	get_time(&end);
	double time = compute_time(start, end, SCALE_MSEC);
	printf("== processed %d requests in %.0f ms\n",
	                pcc->max_reqs, time);
	printf("== ops/s: %.1f\n", pcc->max_reqs/(time/1000));
	/* send exit to server */
	//strcpy(pcc->kvs_request, "E");
	pcc->sd_request.type = EXIT;
	
	/* post receive, we always expect a response */
	ret = rdma_post_recv(pcc->response_mr, pcc->qp);
	debug("writing exit to server\n");
	if(ret) {
		log_err("rdma_post_recv() failed");
		return 1;
	}

	/* write to the server */
	//ret = rdma_write_c2s_non_block(pcc);
	//ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
	ret = rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
	if(ret) {
		log_err("rdma_write_c2s() failed");
		return 1;
	}

	struct ibv_wc wc;
		
	debug("Waiting for completion \n");

	ret = rdma_spin_cq(pcc->cq, &wc, 1);
	if(ret != 1) {
		log_err("rdma_poll_cq() failed");
		exit(1);
	}
	rdma_buffer_deregister(pcc->sd_request_mr);
	rdma_buffer_deregister(pcc->sd_response_mr);	

	return 0;
}



int client_wr_rd(PEARS_CLT_CTX *pcc)
{
	int ret = -1;
	/* set the CQ to nonblock */
	set_comp_channel_non_block(pcc->comp_channel);

	if(pcc->using_file) {
		get_input(&(pcc->kvs_request), MAX_LINES);
	} else {
		strcpy(pcc->kvs_request, "G:key123");
	}

	/* prepare read and write */
	rdma_write_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->kvs_request_mr, pcc->server_md_attr);
	rdma_read_wr_prepare(&(pcc->rd_wr), &(pcc->rd_sge), pcc->kvs_request_mr, pcc->server_md_attr);
	//rdma_write_wr_prepare(&(pcc->send_wr), &(pcc->snd_sge), pcc->response_mr, pcc->server_rd_md_attr);

	int count = 0;
	/* do the same request 10 million times */
	while(count < pcc->max_reqs) {
		if(pcc->using_file) {
			get_input(&(pcc->kvs_request), MAX_LINES);
		} else {
			strcpy(pcc->kvs_request, "G:key123");
		}
		struct ibv_wc wc;
		/* write to the server */
		ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
		if(ret) {
			log_err("rdma_write_c2s() failed");
			return 1;
		}

			
		debug("Waiting for completion \n");
		ret = rdma_spin_cq(pcc->cq, &wc, 1);
		if(ret != 1) {
			log_err("rdma_poll_cq() failed");
			exit(1);
		}

		/* now do the read until the result is not 0 */
		while(pcc->kvs_request[0] != 'R') {
			debug("checking remote memory for result\n");
			ret = rdma_post_write_reuse(&(pcc->rd_wr), pcc->qp);
			if(ret) {
				log_err("rdma_read() failed");
				exit(1);
			}
			ret = rdma_spin_cq(pcc->cq, &wc, 1);
			if(ret != 1) {
				log_err("rdma_poll_cq() failed");
				exit(1);
			}
			debug("memory is currently set to: %s\n", pcc->kvs_request);
		}
		debug("%s\n", pcc->kvs_request);
		//memset(pcc->response, 0, sizeof(pcc->response_mr->length));
		


		count++;
	}

	/* send exit to server */
	strcpy(pcc->kvs_request, "E");

	/* write to the server */
	ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
	if(ret) {
		log_err("rdma_write_c2s() failed");
		return 1;
	}

	struct ibv_wc wc;
		
	debug("Waiting for completion \n");

	ret = rdma_spin_cq(pcc->cq, &wc, 1);
	if(ret != 1) {
		log_err("rdma_poll_cq() failed");
		exit(1);
	}
	debug("waiting for final response\n");
	while(pcc->kvs_request[0] != 'O') {
		ret = rdma_post_write_reuse(&(pcc->rd_wr), pcc->qp);
		if(ret) {
			log_err("rdma_read() failed");
			exit(1);
		}
		ret = rdma_spin_cq(pcc->cq, &wc, 1);
		if(ret != 1) {
			log_err("rdma_poll_cq() failed");
			exit(1);
		}
	}
	/* now we should zero out the memory server side again to make sure we read the same thing next time*/

	return 0;
}

