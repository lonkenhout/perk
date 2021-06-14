/* Client source */

#include "./client.h"


static PERK_CLT_CTX *pcc = NULL;
static int use_id_based_file = 0;
static char *file_name = NULL;

/* usage */
void print_usage(char *cmd){
	printf("Usage:\n");
	printf("\t%s -a [IP] -p [PORT] [-i [INPUT_FILE]]\n", cmd);
}

static void open_file(char *file)
{
	pcc->f_ptr = fopen(file, "r");
	if(!pcc->f_ptr) {
		fprintf(stderr, "Failed to open: %s\n", file);
	} else {
		pcc->using_file = 1;
	}
}

/* parse options */
int parse_opts(int argc, char **argv){
	int ret = 0, option;
	while((option = getopt(argc, argv, "a:p:ui:c:r:")) != -1){
		switch(option){
			case 'a':
				ret = get_addr(optarg, (struct sockaddr*) &(pcc->client_sa));
				if(ret){
					log_err("invalid ip address provided: %s", optarg);
					return ret;
				}
				debug("SET ip address to %s\n", optarg);
				break;
			case 'p':
				pcc->client_sa.sin_port = htons(strtol(optarg, NULL, 0));
				debug("SET port to %s\n", optarg);
				break;
			case 'u':
				use_id_based_file = 1;
				break;
			case 'i':
				debug("opening %s\n", optarg);
				file_name = optarg;
				break;
			case 'c':
				pcc->max_reqs = strtol(optarg, NULL, 10);
				debug("SET max requests to %ld requests\n", pcc->max_reqs);
				break;
			case 'r':
				if(strncmp(optarg, "wr_sd", strlen("wr_sd")) == 0) {
					debug("Using WRITE/SEND configuration");
					pcc->config.client = RDMA_COMBO_WR;
					pcc->config.server = RDMA_COMBO_SD;
				} else if(strncmp(optarg, "wrimm_sd", strlen("wrimm_sd")) == 0) {
					debug("using WRITE with IMM/SEND configuration\n");
					pcc->config.client = RDMA_COMBO_WRIMM;
					pcc->config.server = RDMA_COMBO_SD;
				} else if(strncmp(optarg, "sd_sd", strlen("sd_sd")) == 0) {
					debug("using SEND/SEND configuration\n");
					pcc->config.client = RDMA_COMBO_SD;
					pcc->config.server = RDMA_COMBO_SD;
				} else if(strncmp(optarg, "wr_wr", strlen("wr_wr")) == 0) {
					debug("using WRITE/WRITE configuration\n");
					pcc->config.client = RDMA_COMBO_WR;
					pcc->config.server = RDMA_COMBO_WR;
				} else if(strncmp(optarg, "wr_rd", strlen("wr_rd")) == 0) {
					debug("using WRITE/READ configuration\n");
					pcc->config.client = RDMA_COMBO_WR;
					pcc->config.server = RDMA_COMBO_RD;
				} else {
					fprintf(stderr, "Invalid configuration provided: %s\n", optarg);
				}
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
		debug("Requesting input line from file or stdin [%d/%d]\n", i+1, lines);
		if(pcc->using_file) {
			ret = get_file_line(pcc->f_ptr, dest[i]);
		} else {
			ret = get_line(dest[i]);
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

/* extract cid from programname */
void get_cid(char *prog_name) {
	char *curr = strtok(prog_name, ".");
	if(!curr) return;
	curr = strtok(NULL, ".");
	if(!curr) return;
	pcc->cid = strtoul(curr, NULL, 10);
}

int main(int argc, char **argv){

	int ret = -1;
	if(argc < 2){
		print_usage(argv[0]);
	}

	struct sockaddr_in svr_sa;
	bzero(&svr_sa, sizeof svr_sa);
	svr_sa.sin_family = AF_INET;
	svr_sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	svr_sa.sin_port = 20838;

	pcc = (PERK_CLT_CTX *)calloc(1, sizeof(*pcc));
	pcc->f_ptr = NULL;
	pcc->using_file = 0;
	pcc->max_reqs = 1000000;
	pcc->raw_request = calloc(1, MAX_KEY_SIZE + MAX_VAL_SIZE + 50);

	pcc->config.client = default_client_rdma_config;
	pcc->config.client = default_server_rdma_config;

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

	if(use_id_based_file) {
		get_cid(argv[0]);
		char final_file[100] = {0,};
		snprintf(final_file, sizeof(final_file), "%s%d.in", file_name, pcc->cid);
		printf("Client %lu using file %s\n", pcc->cid, final_file);
		fflush(stdout);
		open_file(final_file);
	} else if(file_name) {
		open_file(file_name);
	}
	ret = client(pcc);
	if(ret) {
		log_err("client() failed");
		goto clean_exit;
	}
	
	ret = client_disconnect(pcc);
	if(ret) {

		log_err("disconnect failed");
		goto clean_exit;
	}
	free(pcc->raw_request);
	free(pcc->response);
	if(pcc->using_file) {
		fclose(pcc->f_ptr);
	}

	free(pcc);

	return 0;

clean_exit:
	free(pcc->raw_request);
	free(pcc->response);
	if(pcc->using_file) {
		fclose(pcc->f_ptr);
	}
	free(pcc);
	return ret;
}
