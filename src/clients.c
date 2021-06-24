
#include "clients.h"

int prepare_request_side(PERK_CLT_CTX *pcc)
{
	int ret = 0;
	switch(pcc->config.client) {
		case RDMA_COMBO_SD:
			rdma_send_wr_prepare(&(pcc->send_wr), &(pcc->snd_sge), pcc->sd_request_mr);
			break;
		case RDMA_COMBO_WR:
			rdma_write_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->sd_request_mr, pcc->server_md_attr);
			break;
		case RDMA_COMBO_WRIMM:
			rdma_write_imm_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->sd_request_mr, pcc->server_md_attr);
			break;
		default:
			break;
	}
	return ret;
}

int prepare_response_side(PERK_CLT_CTX *pcc)
{
	int ret = 0;
	switch(pcc->config.server) {
		case RDMA_COMBO_SD:
			rdma_recv_wr_prepare(&(pcc->recv_wr), &(pcc->rec_sge), pcc->sd_response_mr);
			ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
		    if(ret) {
		        log_err("rdma_post_recv() failed");
		        ret = 1;
		    }
			break;
		case RDMA_COMBO_WR:
			pcc->sd_response.type = EMPTY;
			break;
		case RDMA_COMBO_RD:
			rdma_read_wr_prepare(&(pcc->rd_wr), &(pcc->rd_sge), pcc->sd_response_mr, pcc->server_md_attr);
		default:
			break;
	}
	return ret;
}

int prep_request(PERK_CLT_CTX *pcc, struct request *request)
{
	int req = -1, ret = -1;
	
	if(pcc->using_file) {
		get_input(&(pcc->raw_request), MAX_LINES);
		req = parse_request(pcc->raw_request, 
							request->key, MAX_KEY_SIZE,
							request->val, MAX_VAL_SIZE);
		ret = validate_request(req, pcc->raw_request);
		request->type = req;
	} else {
		request->type = GET;
		strcpy(request->key, "key123");
		ret = 0;
	}
	return ret;
}

int send_request(PERK_CLT_CTX *pcc)
{
	int ret = -1;
	switch(pcc->config.client) {
		case RDMA_COMBO_SD:
			ret = rdma_post_send_reuse(&(pcc->send_wr), pcc->qp);
			if(ret) {
	            log_err("rdma_write_c2s() failed");
	            return ret;
	        }
			break;
		case RDMA_COMBO_WR: case RDMA_COMBO_WRIMM:
			ret = rdma_post_write_reuse(&(pcc->wr_wr), pcc->qp);
			if(ret) log_err("rdma_write_c2s() failed");
		default:
			break;
	}

	return ret;
}

int recv_response(PERK_CLT_CTX *pcc)
{
	int ret = -1;
	struct ibv_wc wc[2];
	switch(pcc->config.server) {
		case RDMA_COMBO_SD:
			ret = rdma_post_recv_reuse(&(pcc->recv_wr), pcc->qp);
	        if(ret) {
	            log_err("rdma_post_recv() failed");
	            return ret;
	        }
			/* WR from request, WR from response */
			ret = rdma_spin_cq(pcc->cq, wc, 2);
			if(ret != 2) {
				log_err("rdma_poll_cq() failed");
				exit(1);
			}
			pcc->rcv_ps--;
			break;
		case RDMA_COMBO_WR:
			/* WR for request, none for response */
			ret = rdma_spin_cq(pcc->cq, wc, 1);
			if(ret != 1) {
				log_err("rdma_poll_cq() failed");
				exit(1);
			}
			/* spin on memory instead */
			while(!(pcc->sd_response.type == RESPONSE_OK || pcc->sd_response.type == RESPONSE_EMPTY || pcc->sd_response.type == EXIT_OK));
			break;
		case RDMA_COMBO_RD:
			/* WR for request */
            ret = rdma_spin_cq(pcc->cq, wc, 1);
            if(ret != 1) {
                log_err("rdma_poll_cq() failed");
                exit(1);
            }
			uint64_t count = 0;
			/* check remote memory until the result has been placed in response buffer */
			while(!(pcc->sd_response.type == RESPONSE_OK || pcc->sd_response.type == RESPONSE_EMPTY || pcc->sd_response.type == EXIT_OK)) {
				
				ret = rdma_post_write_reuse(&(pcc->rd_wr), pcc->qp);
				if(ret) log_err("reading from remote memory failed");
				ret = rdma_spin_cq(pcc->cq, wc, 1);
				if(ret != 1) {
					log_err("rdma_poll_cq() failed");
					exit(1);
				}
				bm_reads_incr(&count);
			}
			bm_reads_print(req_type_str(pcc->sd_request.type), count);
			pcc->sd_response.type = EMPTY;
			break;
		default:
			break;
	}
    return 0;
}

int prep_next_iter(PERK_CLT_CTX *pcc)
{
	int ret = -1;
    switch(pcc->config.server) {
        case RDMA_COMBO_WR:
			pcc->sd_response.type = EMPTY;
            break;
        default:
            break;
    }
    return 0;
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



int client(PERK_CLT_CTX *pcc)
{
	int ret = -1;
	struct timeval start;
	struct timeval end, t_s, t_e, cs, ce;
	struct ibv_wc wc;

	ret = prepare_request_side(pcc);
	if(ret) return ret;

	ret = prepare_response_side(pcc);
	if(ret) return ret;

	int count = 0;
	bm_ops_start(&t_s);

	pcc->sd_request.rid = 0;
	/* do the same request max_reqs times */
	while(count < pcc->max_reqs) {
		ret = prep_request(pcc, &(pcc->sd_request));
		if(ret) {
			fprintf(stderr, "error, request couldnt be sent\n");
			exit(1);
		}
		bm_latency_start(&start);

		if(send_request(pcc)) return 1;
		if(recv_response(pcc)) return 1;
		count++;

		bm_latency_end(&end);
		bm_latency_show("comp", start, end);
		print_req(pcc->sd_request, pcc->sd_response);
		pcc->sd_request.rid++;
		if(prep_next_iter(pcc)) return 1;
	}
	bm_ops_start(&t_e);
	bm_ops_show(pcc->max_reqs, t_s, t_e);
	/* send exit to server */
	pcc->sd_request.type = EXIT;
	if(send_request(pcc)) return 1;
	if(recv_response(pcc)) return 1;
	

	return 0;

}

