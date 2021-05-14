
#include "util.h"

#include <libmemcached/memcached.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "bm.h"

#define MAX_CONFIG_LEN (50)

static FILE *f_ptr = NULL;
static int using_file = 0;
static int max_reqs = 1000000;

static char *ip = NULL;
static char *port = NULL;

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
				ip = optarg;
                                debug("ip address set to %s\n", optarg);
                                break;
                        case 'p':
                                port = optarg;
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
                                ret = 1;
                                break;
                }
        }
        return ret;
}


int main(int argc, char **argv) {
 	memcached_st *memc;
 	memcached_return rc;
	char *key = "keystring";
	size_t key_length = strlen(key);
	char *value = "keyvalue";
	uint32_t flags;

	if(parse_opts(argc, argv)) {
		exit(1);
	}

	/* timing */
	struct timeval o_s, o_e, l_s, l_e;

	char config[MAX_CONFIG_LEN];
	memset(config, 0, sizeof(config));

	//const char *config = "--SERVER=10.149.0.53:11211";
	snprintf(config, sizeof(config) - 1, "--SERVER=%s:%s", ip, port);

	/* connect to the server */
	memc = memcached(config, strlen(config));
	if(memc == NULL) {
		log_err("memcached() failed with configuration: %s\n", config);
		exit(1);
	}
	printf("Connected successfully to memcached server on %s:%s\n", ip, port);

	/* put some random value in the server side cache */
	rc = memcached_set(memc, key, key_length, value, strlen(value), 0, 0);
	if(rc != MEMCACHED_SUCCESS) {
		log_err("error inserting key\n");
	}
	/* start timer for ops/sec */
	bm_ops_start(&o_s);
	int i = 0;
	while(i < max_reqs) {
		bm_latency_start(&l_s);
		rc = memcached_mget(memc, (const char * const*)&key, &key_length, 1);

		char ret_key[MEMCACHED_MAX_KEY];
		memset(ret_key, 0, sizeof(ret_key));
		size_t ret_key_len;
		char *ret_value;
		size_t ret_value_len;
		while((ret_value = memcached_fetch(memc, ret_key, &ret_key_len, &ret_value_len, &flags, &rc))) {
			if(rc != MEMCACHED_SUCCESS) {
				printf("error retrieving value for key\n"); break;
			} 
			bm_latency_end(&l_e);
			bm_latency_show("mcd", l_s, l_e);
			free(ret_value);
			memset(ret_key, 0, sizeof(ret_key));
		}
		i++;
	}
	bm_ops_end(&o_e);
	bm_ops_show(max_reqs, o_s, o_e);

	memcached_free(memc);
	return 0;
}
