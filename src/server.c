/* Server source file */

#include "./server.h"



static GHashTable *ght = NULL;
static PEARS_SVR_CTX *psc = NULL;

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

void server(){
    int done = 0;
    while(!done){
        //poll for requests
        //process active requests
        done = 1;
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
	PEARS_CLIENT_CONN *pc_conn = (PEARS_CLIENT_CONN *)calloc(1, sizeof(*pc_conn));
	/* initialize key-value store */
	pears_kv_init();

	/* setup server rdma resources */
	/*psc->ctx = init_ibv_ctx(NULL);*/
	res = init_server_dev(psc);
	if(res) {
		goto clean_exit_err;
	}
	
	//TODO: transform code below to do following:
	/*	loop (until ctrl-c || target is met -> all clients disconnected):
			poll on incoming connections and buffers
			if(new connection)
				setup client
			else if(buffer from client modified)
				read request and print contents
			else if(client want disconnect)
				disconnect the client and clean up for it
		end loop
		if(ctrl-c was hit)
			try cleaning up all client resources
		clean up server resources

	*/
	/* wait for single client to connect */
	res = wait_for_client_conn(psc, pc_conn);
	if(res) {
		goto clean_exit_err;
	}

	/* setup resources for that client */
	res = init_server_client_resources(pc_conn);
	if(res) {
		goto clean_exit_err;
	}

	//TODO: accept multiple clients
	res = accept_client_conn(psc, pc_conn);
	if(res) {
		goto clean_exit_err;
	}

	res = send_md_s2c(pc_conn);
	if(res) {
		goto clean_exit_err;
	}
	
	/* destroy any leftover resources */
	/*destroy_ibv_context(psc->ctx, psc->pd);*/
	res = disconnect_client_conn(psc, pc_conn);
	if(res) {
		goto clean_exit_err;
	}

	destroy_server_dev(psc);
	pears_kv_destroy();
	free(psc);
	free(pc_conn);
	printf("Cleanup complete, exiting..\n");
	return res;
	
clean_exit_err:
	destroy_server_dev(psc);
	free(psc);
	free(pc_conn);
	exit(1);
}
