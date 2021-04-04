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

int process_connect_req(PEARS_SVR_CTX *psc, PEARS_CLIENT_CONN *pc_conn)
{
	int ret = -1;

	//TODO: check if can be removed:
	/*
	debug("client request received");
	res = wait_for_client_conn(psc, pc_conn);
	if(res) {
		log_err("wait_for_client_conn() failed");
		exit(1);
	}*/

	/* setup resources for that client */
	ret = init_server_client_resources(pc_conn);
	if(ret) {
		log_err("init_server_client_resources() failed");
		return POLL_CLIENT_CONNECT_FAILED;
	}

	ret = accept_client_conn(psc, pc_conn);
	if(ret) {
		log_err("accept_client_conn() failed");
		return POLL_CLIENT_CONNECT_FAILED;
	}

	ret = send_md_s2c(pc_conn);
	if(ret) {
		log_err("send_md_s2c() failed");
		return POLL_CLIENT_CONNECT_FAILED;
	}
	return POLL_CLIENT_CONNECT_SUCCESS;
}

int process_disconnect_req(PEARS_SVR_CTX *psc, PEARS_CLIENT_CONN *pc_conn)
{
	int ret = disconnect_client_conn(psc, pc_conn);
	if(ret) {
		log_err("disconnect_client_conn() failed");
		return POLL_CLIENT_DISCONNECT_FAILED;
	}
	return POLL_CLIENT_DISCONNECT_SUCCESS;
}

int process_cm_event(PEARS_SVR_CTX *psc, PEARS_CLIENT_COLL *conns) 
{
	//TODO: first retrieve the event, if connect, handle connect
	// 		else handle disconnect
	struct rdma_cm_event *cm_event = NULL;
	struct rdma_cm_id *tmp_id = NULL;
	int ret = -1;
	
	debug("Grabbing single cm event\n");

	ret = rdma_cm_event_rcv_any(psc->cm_ec, &cm_event);
	if(ret) {
		log_err("failed to retrieve cm event");
		return ret;
	}
	/* grab information so we can identify the connection */
	struct sockaddr *src = &(cm_event->id->route.addr.dst_addr); 
	tmp_id = cm_event->id;

	ret = rdma_ack_cm_event(cm_event);
	if(ret) {
		log_err("failed to ACK cm event");
		return -errno;
	}


	
	
	//char addr[100] = {0,};
	//ret = get_addr_port(addr, src);

	/* determine which connection this is about*/
	PEARS_CLIENT_CONN *pc_conn = NULL;
	int conn_i;
	//&(conns->clients[conns->count])

	switch(cm_event->event) {
		case RDMA_CM_EVENT_CONNECT_REQUEST:
			conn_i = client_coll_find_free(conns);
			pc_conn = &(conns->clients[conn_i]);
			pc_conn->cm_cid = tmp_id;
			ret = process_connect_req(psc, pc_conn);
			if(POLL_CLIENT_CONNECT_SUCCESS) conns->active[conn_i] = 1;
			break;
		case RDMA_CM_EVENT_DISCONNECTED:
			conn_i = client_coll_find_conn(conns, src);
			pc_conn = &(conns->clients[conn_i]);
			ret = process_disconnect_req(psc, pc_conn);
			break;
		default:
			log_err("Unexpected event received: %s", rdma_event_str(cm_event->event));
			ret = -1;
	}
	return ret;
}

void server(PEARS_SVR_CTX *psc, PEARS_CLIENT_COLL *conns)
{
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
    	debug("polling now....\n");
        n_fds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if(n_fds == -1) {
			log_err("epoll_wait() failed");
			return;
		}

		for(i = 0; i < n_fds; ++i) {
			if(events[i].data.fd == psc->cm_ec->fd) {
				debug("cm event channel ready\n");
				/* take first available slot setup new connection there */
				//TODO: cant choose entry yet
				

				/* accept or disconnect */
				res = process_cm_event(psc, conns);

				//TODO: add mechanism to manage entries
				if(res == POLL_CLIENT_CONNECT_SUCCESS) {
					//conns->count++;
					debug("should increment count.. but wont\n");
				} else if(res == POLL_CLIENT_DISCONNECT_SUCCESS) {
					debug("should decrement count and reset entry.. but wont\n");
				}


				//TODO: add completion channel(?) fd to set of polled on fds
				//		should do in process_cm_event() or make process_cm_event()
				//		return whether it was a connect or disconnect so can do here				
			} else {
				//process connected clients request
				debug("nothing done here yet");
			}
		}
    }
    destroy_server_dev(psc);
	pears_kv_destroy();
	free(psc);
	free(conns);
	printf("Cleanup complete, exiting..\n");
	exit(0);
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
	//PEARS_CLIENT_CONN *pc_conn = &(conns->clients[0]);

	/* initialize key-value store */
	pears_kv_init();

	/* setup server rdma resources */
	res = init_server_dev(psc);
	if(res) {
		destroy_server_dev(psc);
		pears_kv_destroy();
		free(psc);
		free(conns);
		exit(1);
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
