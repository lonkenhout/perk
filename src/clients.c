
#include "clients.h"

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

int client_sd_sd(PEARS_CLT_CTX *pcc)
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
	
	rdma_send_wr_prepare(&(pcc->send_wr), &(pcc->snd_sge), pcc->kvs_request_mr);

	int count = 0;
	printf("doing %d requests\n", pcc->max_reqs);
	/* do the same request max_reqs times */
	while(count < pcc->max_reqs) {
		struct ibv_wc wc;
		/* post receive, we always expect a response */
		ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		if(ret) {
			log_err("rdma_post_recv() failed");
			return 1;
		}

		
		/* write to the server */
		rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
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

