/* Client source */

#include "./client.h"


static PEARS_CLT_CTX *pcc = NULL;

/* usage */
void print_usage(char *cmd){
	printf("Usage:\n");
	printf("\t%s -a [IP] -p [PORT]\n", cmd);
}

/* parse options */
int parse_opts(int argc, char **argv){
	int ret = 0, option;
	while((option = getopt(argc, argv, "a:p:")) != -1){
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
				debug("ip address set to %s\n", optarg);
				break;
			default:
				print_usage(argv[0]);
				ret = -1;
				break;
		}
	}
	return ret;
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
	pcc->kvs_request = (char *)calloc(1, 30);
	pcc->remote = (char *)calloc(1, 30);
	strcpy(pcc->kvs_request,"G:randomkey:randomvalue");

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

	ret = rdma_write_c2s(pcc);
	if(ret) {
		log_err("rdma_write_c2s() failed");
		goto clean_exit;
	}

	printf("Local buff:  %s\n", pcc->kvs_request);
	printf("Remote buff: %s\n", pcc->remote);

	ret = client_disconnect(pcc);
	if(ret) {
		log_err("disconnect failed");
		goto clean_exit;
	}


	

	return 0;

clean_exit:
	free(pcc);
	return ret;
}
