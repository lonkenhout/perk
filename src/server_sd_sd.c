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
	g_hash_table_destroy(ght);
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

	/*pc_conn->imm_data = rdma_buffer_alloc(pc_conn->pd, MAX_LINE_LEN, PERM_L_RW);
	ret = rdma_post_recv(pc_conn->imm_data, pc_conn->qp);
	if(ret) {
		log_err("failed to ACK cm event");
		return -errno;
	}*/

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
			/*pc_conn->imm_data = rdma_buffer_alloc(pc_conn->pd, MAX_LINE_LEN, PERM_L_RW);
			ret = rdma_post_recv(pc_conn->imm_data, pc_conn->qp);
			if(ret) {
				log_err("failed to ACK cm event");
				return -errno;
			}*/

			/* finalize the connection */
			ret = process_established_req(psc, pc_conn);

			pthread_create(&(conns->threads[conn_i]), NULL, worker, (void*) pc_conn);
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
			/* clear out the cq if necessary*/
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

void *worker(void *args)
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
	int clients_connected = 0;
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
						get_time(&end);
						double time = compute_time(start, end, SCALE_MSEC);
						printf("== processed %d requests in %.0f ms\n",
								psc->total_ops, time);
						printf("== ops/s: %.1f\n", psc->total_ops/(time/1000));
						psc->total_ops = 0;

					}
					debug("disconnect processed successfully\n");
				} else if(res == POLL_CLIENT_CONNECT_ESTABLISHED){
					/* as soon as first client connects, start the timer*/
					clients_connected++;
					if(clients_connected == 1) {
						get_time(&start);
					}
				}
			}
		}

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
