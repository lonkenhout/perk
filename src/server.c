/* Server source file */

#include "./server.h"



static GHashTable *ght = NULL;
static PEARS_SVR_CTX *psc = NULL;
static PEARS_CLIENT_COLL *conns = NULL;

/* mutex for performing put requests from a worker */
static pthread_mutex_t put_mutex;

/* usage */
void print_usage(char *cmd){
	printf("Usage:\n");
	printf("\t%s -a [IP] -p [PORT]\n", cmd);
}

/* parse options */
int parse_opts(int argc, char **argv){
	int ret = 0, option;
	while((option = getopt(argc, argv, "a:p:r:")) != -1){
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
			case 'r':
				if(strncmp(optarg, "wr_sd", strlen("wr_sd")) == 0) {
					printf("Using WRITE/SEND configuration");
					psc->config.client = RDMA_COMBO_WR;
					psc->config.server = RDMA_COMBO_SD;
				} else if(strncmp(optarg, "wrimm_sd", strlen("wrimm_sd")) == 0) {
					printf("using WRITE with IMM/SEND configuration\n");
					psc->config.client = RDMA_COMBO_WRIMM;
					psc->config.server = RDMA_COMBO_SD;
				} else if(strncmp(optarg, "sd_sd", strlen("sd_sd")) == 0) {
					printf("using SEND/SEND configuration\n");
					psc->config.client = RDMA_COMBO_SD;
					psc->config.server = RDMA_COMBO_SD;
				} else if(strncmp(optarg, "wr_wr", strlen("wr_wr")) == 0) {
					printf("using WRITE/WRITE configuration\n");
					psc->config.client = RDMA_COMBO_WR;
					psc->config.server = RDMA_COMBO_WR;
				} else if(strncmp(optarg, "wr_rd", strlen("wr_rd")) == 0) {
					printf("using WRITE/READ configuration\n");
					psc->config.client = RDMA_COMBO_WR;
					psc->config.server = RDMA_COMBO_RD;
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

void pears_kv_init()
{
	ght = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	if(ght == NULL){
		log_err("failed to initialize hash table");
		exit(1);
	}
}

void pears_kv_destroy()
{
	pthread_mutex_lock(&put_mutex);
	g_hash_table_destroy(ght);
	pthread_mutex_unlock(&put_mutex);
}

void pears_kv_insert(void *key, void *val)
{
	pthread_mutex_lock(&put_mutex);
	g_hash_table_insert(ght, g_strdup(key), g_strdup(val));
	pthread_mutex_unlock(&put_mutex);
}

char *pears_kv_get(void *key)
{
	return (char*) g_hash_table_lookup(ght, key);
}


void pears_kv_list_all()
{
	GList *keys = g_hash_table_get_keys(ght);
	GList *values = g_hash_table_get_values(ght);
	GList *it1, *it2;
	int i = 0;
	for(it1 = keys, it2 = values; it1 != NULL; it1 = it1->next, it2 = it2->next, i++){
		printf("%s : %s\n", (char*)it1->data, (char*)it2->data);
	}
	printf("Total count: %d\n", i);	
	g_list_free(keys);
	g_list_free(values);
}

int process_connect_req(PEARS_SVR_CTX *psc, PEARS_CLIENT_CONN *pc_conn)
{
	int ret = -1;

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

	return POLL_CLIENT_CONNECT_SUCCESS;
}

/* finalize the request */
int process_established_req(PEARS_SVR_CTX *psc, PEARS_CLIENT_CONN *pc_conn)
{
	int ret = -1;
	ret = finalize_client_conn(pc_conn);
	if(ret) {
		log_err("finalize_client_conn() failed");
		return POLL_CLIENT_CONNECT_FAILED;
	}

	ret = send_md_s2c(pc_conn);
	if(ret) {
		log_err("send_md_s2c() failed");
		return POLL_CLIENT_CONNECT_FAILED;
	}
	return POLL_CLIENT_CONNECT_ESTABLISHED;
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
	/* retrieve some event on the connection management channel */
	struct rdma_cm_event *cm_event = NULL;
	struct rdma_cm_id *tmp_id = NULL;
	int ret = -1;
	char buff[100] = {0,};
	
	debug("Grabbing single cm event\n");

	ret = rdma_cm_event_rcv_any(psc->cm_ec, &cm_event);
	if(ret) {
		log_err("failed to retrieve cm event");
		return ret;
	}
	/* grab information so we can identify the connection */
	struct sockaddr *src = &(cm_event->id->route.addr.dst_addr);
	get_addr_port(buff, src);
	tmp_id = cm_event->id;


	PEARS_CLIENT_CONN *pc_conn = NULL;
	int conn_i;

	switch(cm_event->event) {
		case RDMA_CM_EVENT_CONNECT_REQUEST:
			debug("processing connect request\n");
			ret = rdma_ack_cm_event(cm_event);
			if(ret) {
				log_err("failed to ACK cm event");
				return -errno;
			}
			/* setup basic client resources */
			/* first find a free entry for the client to use */
			conn_i = client_coll_find_free(conns);
			pc_conn = &(conns->clients[conn_i]);
			pc_conn->cm_cid = tmp_id;
			pc_conn->config.client = psc->config.client;
			pc_conn->config.server = psc->config.server;
			ret = process_connect_req(psc, pc_conn);
			if(ret == POLL_CLIENT_CONNECT_SUCCESS) {
				conns->active[conn_i] = 1;
				printf("added client to entry: %d\n", conn_i);
				ret = 0;
			} else {
				ret = -2;
			}
			//set_comp_channel_non_block(pc_conn->io_cc);
			break;
		case RDMA_CM_EVENT_ESTABLISHED:
			debug("processing established request\n");
			ret = rdma_ack_cm_event(cm_event);
			if(ret) {
				log_err("failed to ACK cm event");
				return -errno;
			}
			/* find the entry setup at */
			conn_i = client_coll_find_conn(conns, src);
			debug("finalizing connection at entry %d\n", conn_i);
			pc_conn = &(conns->clients[conn_i]);
			/* finalize the connection */
			ret = process_established_req(psc, pc_conn);
			if(psc->config.client == RDMA_COMBO_WR && psc->config.server == RDMA_COMBO_WR) {
				pthread_create(&(conns->threads[conn_i]), NULL, worker_wr_wr, (void*) pc_conn);
			} else if(psc->config.client == RDMA_COMBO_WR && psc->config.server == RDMA_COMBO_SD) {
				pthread_create(&(conns->threads[conn_i]), NULL, worker_wr_sd, (void*) pc_conn);
			} else if(psc->config.client == RDMA_COMBO_SD && psc->config.server == RDMA_COMBO_SD) {
				pthread_create(&(conns->threads[conn_i]), NULL, worker, (void*) pc_conn);
			} else if(psc->config.client == RDMA_COMBO_WRIMM && psc->config.server == RDMA_COMBO_SD) {
				pthread_create(&(conns->threads[conn_i]), NULL, worker_wrimm_sd, (void*) pc_conn);
			} else if(psc->config.client == RDMA_COMBO_WR && psc->config.server == RDMA_COMBO_RD) {
				pthread_create(&(conns->threads[conn_i]), NULL, worker_wr_rd, (void*) pc_conn);
			} else {
				fprintf(stderr, "error: unknown rdma combination\n");
				exit(1);
			}


			if(ret == POLL_CLIENT_CONNECT_ESTABLISHED) {
				conns->established[conn_i] = 1;
			} else {
				ret = -2;
			}
			break;
		case RDMA_CM_EVENT_DISCONNECTED:
			/* find the entry the client for this disconnect is stored in */
			conn_i = client_coll_find_conn(conns, src);
			pc_conn = &(conns->clients[conn_i]);
			ret = rdma_ack_cm_event(cm_event);
			if(ret) {
				log_err("failed to ACK cm event");
				return -errno;
			}
			/* wait for thread to complete, should have completed already though because of the EXIT request */
			pthread_join(conns->threads[conn_i], NULL);
			rdma_clear_cq(pc_conn->cq);
			/* actually process the disconnect and reset the entry */
			ret = process_disconnect_req(psc, pc_conn);
			conns->active[conn_i] = 0;
			conns->established[conn_i] = 0;
			psc->total_ops += pc_conn->ops;
			pc_conn->ops = 0;

			if(ret != POLL_CLIENT_DISCONNECT_SUCCESS) {
				ret = -2;
			}
			break;
		default:
			log_err("Unexpected event received: %s", rdma_event_str(cm_event->event));
			ret = 0;
	}
	return ret;
}


void server(PEARS_SVR_CTX *psc, PEARS_CLIENT_COLL *conns)
{
	/* setup resources */
	struct epoll_event ev, events[MAX_EVENTS];
	int epollfd, n_fds;
	pthread_mutex_init(&put_mutex, NULL);
	struct timeval start, end;
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
	//TODO: self-pipe trick so we can intercept ctrl-c and still clean up
	debug("polling now....\n");
	printf("starting main loop\n");
	int clients_connected = 0;
	int stop = 0;
	/* first wait for all clients */
	while(1){
		n_fds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if(n_fds == -1) {
			log_err("epoll_wait failed");
			return;
		}

		for(i = 0; i < n_fds; ++i) {
			if(events[i].data.fd == psc->cm_ec->fd) {
				debug("cm event channel ready\n");

				res = process_cm_event(psc, conns);

				if(res == -2) {
					log_err("process_cm_event() failed, continuing...");
				} else if(res == POLL_CLIENT_DISCONNECT_SUCCESS) {
					/* decrement clients, if no more are connected, print ops/sec */
					clients_connected--;
					if(clients_connected == 0) {
						bm_ops_end(&end);
						bm_ops_show(psc->total_ops, start, end);

						psc->total_ops = 0;
						if(PERK_SERVER_EXIT_ON_DC){
							stop = 1;
							break;
						}
					}
					debug("disconnect processed successfully\n");
				} else if(res == POLL_CLIENT_CONNECT_ESTABLISHED){
					/* as soon as first client connects, start the timer*/
					clients_connected++;
					if(clients_connected == 1) {
						bm_ops_start(&start);
					}
				}
			}
		}
		if(stop) break;

    }
    pthread_mutex_destroy(&put_mutex);
    destroy_server_dev(psc);
	pears_kv_destroy();
	free(psc);
	free(conns);
	printf("Cleanup complete, exiting..\n");
	exit(0);
}

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

	/* struct to store multiple clients in, holds enough for MAX_CLIENTS */
	conns = (PEARS_CLIENT_COLL *)calloc(1, sizeof(*conns));

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
	
	//TODO: cleaner exit
	destroy_server_dev(psc);
	pears_kv_destroy();
	free(psc);
	free(conns);
	printf("Cleanup complete, exiting..\n");
	return res;
}
