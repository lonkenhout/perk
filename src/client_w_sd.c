/* Client source */

#include "./client.h"


static PEARS_CLT_CTX *pcc = NULL;
static FILE *f_ptr = NULL;
static int using_file = 0;
static int max_reqs = 10000000;

/* usage */
void print_usage(char *cmd){
	printf("Usage:\n");
	printf("\t%s -a [IP] -p [PORT] [-i [INPUT_FILE]]\n", cmd);
}

/* parse options */
int parse_opts(int argc, char **argv){
	int ret = 0, option;
	while((option = getopt(argc, argv, "a:p:i:c:")) != -1){
		switch(option){
			case 'a':
				ret = get_addr(optarg, (struct sockaddr*) &(pcc->client_sa));
				if(ret){
					log_err("invalid ip address provided: %s", optarg);
					return ret;
				}
				debug("ip address set to %s\n", optarg);
				break;
			case 'p':
				pcc->client_sa.sin_port = htons(strtol(optarg, NULL, 0));
				debug("port set to %s\n", optarg);
				break;
			case 'i':
				debug("opening %s\n", optarg);
				f_ptr = fopen(optarg, "r");
				if(!f_ptr) {
					//log_err("opening file failed, using default instead");
					printf("opening file failed, using default request instead\n");
				} else {
					using_file = 1;
				}
				break;
			case 'c':
				max_reqs = strtol(optarg, NULL, 0);
				debug("doing %d requests\n", max_reqs);
				break;
			default:
				print_usage(argv[0]);
				ret = -1;
				break;
		}
	}
	return ret;
}


int get_input(char **dest, int lines) {
	debug("Gathering lines to send\n");
	int i, ret = -1, total = 0;
	for(i = 0; i < lines; ++i) {
		debug("Requesting input line via stdin [%d/%d]\n", i+1, lines);
		if(using_file) {
			ret = get_file_line(f_ptr, dest[i], MAX_LINE_LEN);
		} else {
			ret = get_line(dest[i], MAX_LINE_LEN);
		}
		if(ret == TOO_LONG) {
			log_err("input line was too long");
			return -1;
		} else if(ret == EOF) {
			log_err("EOF encountered");
			return -1;
		}
		total++;
	}
	return total;
}

int client(PEARS_CLT_CTX *pcc)
{
	int ret = -1;
	/* set the CQ to nonblock */
	set_comp_channel_non_block(pcc->comp_channel);

	if(using_file) {
		get_input(&(pcc->kvs_request), MAX_LINES);
	} else {
		strcpy(pcc->kvs_request, "G:key123");
	}

	/* prepare read and write */
	rdma_recv_wr_prepare(&(pcc->recv_wr), &(pcc->rec_sge), pcc->response_mr);
	rdma_write_wr_prepare(&(pcc->wr_wr), &(pcc->wr_sge), pcc->kvs_request_mr, pcc->server_md_attr);


	int count = 0;
	/* do the same request 10 million times */
	while(count < max_reqs) {
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

int main(int argc, char **argv){
	printf("Started client\n");
	int ret = -1;
	if(argc < 2){
		print_usage(argv[0]);
	}

	struct sockaddr_in svr_sa;
	bzero(&svr_sa, sizeof svr_sa);
	svr_sa.sin_family = AF_INET;
	svr_sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	svr_sa.sin_port = 20838;

	pcc = (PEARS_CLT_CTX *)calloc(1, sizeof(*pcc));
	pcc->kvs_request = (char *)calloc(MAX_LINE_LEN, sizeof(char));
	pcc->response = (char *)calloc(MAX_LINE_LEN, sizeof(char));

	strcpy(pcc->kvs_request,"placeholder");

	parse_opts(argc, argv);

	ret = init_client_dev(pcc, &svr_sa);
	if(ret) {
		log_err("init_client_dev() failed");
		goto clean_exit;
	}

	ret = client_pre_post_recv_buffer(pcc);
	if(ret) {
		log_err("client_pre_post_recv_buffer() failed");
		goto clean_exit;
	}

	ret = connect_to_server(pcc);
	if(ret) {
		log_err("connect_to_server() failed");
		goto clean_exit;
	}

	ret = send_md_c2s(pcc);
	if(ret) {
		log_err("send_md_c2s() failed");
		goto clean_exit;
	}

	ret = client(pcc);
	if(ret) {
		log_err("client() failed");
		goto clean_exit;
	}
	

	//sleep(2);
	ret = client_disconnect(pcc);
	if(ret) {

		log_err("disconnect failed");
		goto clean_exit;
	}
	free(pcc->kvs_request);
	free(pcc->response);

	free(pcc);
	if(using_file) {
		fclose(f_ptr);
	}

	return 0;

clean_exit:
	free(pcc->kvs_request);
	free(pcc->response);
	free(pcc);
	if(using_file) {
		fclose(f_ptr);
	}
	return ret;
}
