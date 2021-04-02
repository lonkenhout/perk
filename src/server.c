/* Server source file */

#include "./server.h"



static GHashTable *ght = NULL;
static PEARS_SVR_CTX *psc = NULL;
static PEARS_CLIENT_COLL *conns = NULL;

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
				ret = get_addr(optarg, (struct sockaddr*) &(psc->server_sa));
				if(ret){
					log_err("invalid ip address provided: %s", optarg);
					return ret;
				}
				debug("ip address set to %s\n", optarg);
				break;
			case 'p':
				psc->server_sa.sin_port = htons(strtol(optarg, NULL, 0));
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

void pears_kv_init(){
	ght = g_hash_table_new(NULL, NULL);
	if(ght == NULL){
		log_err("failed to initialize hash table");
		exit(1);
	}
}

void pears_kv_destroy(){
	g_hash_table_destroy(ght);
}

void pears_kv_insert(void *key, void *val){
	g_hash_table_insert(ght, key, val);
}


void pears_kv_list_all(){
	GList *keys = g_hash_table_get_keys(ght);
	GList *values = g_hash_table_get_values(ght);
	int i = 0;
	for(GList *it1 = keys, *it2 = values; it1 != NULL; it1 = it1->next, it2 = it2->next, i++){
		printf("%s : %s\n", (char*)it1->data, (char*)it2->data);
	}
	printf("Total count: %d\n", i);	
	g_list_free(keys);
	g_list_free(values);
}

void server(PEARS_SVR_CTX *psc, PEARS_CLIENT_COLL *conns){
	struct epoll_event ev, events[MAX_EVENTS];
	int epollfd, n_fds;
    int i, res = 0, done = 0;
    debug("preparing poll\n");
	epollfd = epoll_create1(0);
	if(epollfd == -1) {
		log_err("epoll_create1() failed");
		return;
	}

	debug("adding event channel to poll fds\n");
	/* add event channel to fds polled on */
	ev.events = EPOLLIN;
	ev.data.fd = psc->cm_ec->fd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, psc->cm_ec->fd, &ev) == -1) {
		log_err("epoll_ctl() failed");
		return;
	}
	//TODO: self-pipe trick so we can intercept ctrl-c

    while(1){
        n_fds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if(n_fds == -1) {
			log_err("epoll_wait() failed");
			return;
		}

		for(i = 0; i < n_fds; ++i) {
			if(events[i].data.fd == psc->cm_ec->fd) {
				//TODO: add handler:
				//		for RDMA_CM_EVENT_CONNECT_REQUEST
				//		and RDMA_CM_EVENT_DISCONNECTED
				debug("cm event channel ready\n");
				/* take first available slot setup new connection there */
				PEARS_CLIENT_CONN *pc_conn = &(conns->clients[conns->count]);

				debug("client request received");
				res = wait_for_client_conn(psc, pc_conn);
				if(res) {
					log_err("wait_for_client_conn() failed");
					exit(1);
				}

				/* setup resources for that client */
				res = init_server_client_resources(pc_conn);
				if(res) {
					log_err("init_server_client_resources() failed");
					exit(1);
				}

				res = accept_client_conn(psc, pc_conn);
				if(res) {
					log_err("accept_client_conn() failed");
					exit(1);
				}

				res = send_md_s2c(pc_conn);
				if(res) {
					log_err("send_md_s2c() failed");
					exit(1);
				}

				res = disconnect_client_conn(psc, pc_conn);
				if(res) {
					log_err("disconnect_client_conn() failed");
					exit(1);
				}

				destroy_server_dev(psc);
				pears_kv_destroy();
				free(psc);
				free(conns);
				printf("Cleanup complete, exiting..\n");
				exit(0);
			} else {
				//process connected clients request
				debug("nothing done here yet");
			}
		}
    }
}

/*int pears_init_server(){
	
}*/

int main(int argc, char **argv){
	if(argc < 2){
		print_usage(argv[0]);
	}
	
	/* prepare server resources and parse program arguments */
	psc = (PEARS_SVR_CTX *)calloc(1, sizeof(*psc));
	int res = parse_opts(argc, argv);
	if(res != 0){
		free(psc);
		return res;
	}
	if(!(psc->server_sa.sin_port)){
		psc->server_sa.sin_port = htons(DEFAULT_PORT);
	}

	/* prepare resources for a single client connection */
	//PEARS_CLIENT_CONN *pc_conn = (PEARS_CLIENT_CONN *)calloc(1, sizeof(*pc_conn));
	conns = (PEARS_CLIENT_COLL *)calloc(1, sizeof(*conns));
	//conns->
	PEARS_CLIENT_CONN *pc_conn = &(conns->clients[0]);

	/* initialize key-value store */
	pears_kv_init();

	/* setup server rdma resources */
	res = init_server_dev(psc);
	if(res) {
		goto clean_exit_err;
	}
	
	server(psc, conns);
	
	//TODO: let this get executed again or move to server()
	destroy_server_dev(psc);
	pears_kv_destroy();
	free(psc);
	free(conns);
	printf("Cleanup complete, exiting..\n");
	return res;
}
