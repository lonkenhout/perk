
#include "util.h"

#include <libmemcached/memcached.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "bm.h"
#include "util.h"

#define MAX_CONFIG_LEN (50)

static FILE *f_ptr = NULL;
static int using_file = 0;
static int use_id_based_file = 0;
static uint32_t cid;
static char *file_name = NULL;
static int max_reqs = 1000000;

static char *ip = NULL;
static char *port = NULL;

/* usage */
void print_usage(char *cmd){
        printf("Usage:\n");
        printf("\t%s -a [IP] -p [PORT] [-i [INPUT_FILE]]\n", cmd);
}

static void open_file(char *file)
{
    f_ptr = fopen(file, "r");
    if(!f_ptr) {
        fprintf(stderr, "Failed to open: %s\n", file);
    } else {
        using_file = 1;
    }
}

/* parse options */
int parse_opts(int argc, char **argv){
        int ret = 0, option;
        while((option = getopt(argc, argv, "a:p:ui:c:")) != -1){
                switch(option){
                        case 'a':
				ip = optarg;
                                debug("ip address set to %s\n", optarg);
                                break;
                        case 'p':
                                port = optarg;
                                debug("port set to %s\n", optarg);
                                break;
						case 'u':
							use_id_based_file = 1;
							break;
                        case 'i':
                                debug("opening %s\n", optarg);
								file_name = optarg;
                                break;
                        case 'c':
                                max_reqs = strtol(optarg, NULL, 10);
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

uint64_t get_mcd_ops(memcached_st *memc)
{
	uint64_t ops = 0;
 	memcached_return rc;
	memcached_stat_st *stats;
	stats = memcached_stat(memc, NULL, &rc);
	if(!stats) {
		log_err("could not fetch memcached server statistics");
		exit(1);
	}

	char *g_ops = NULL,
		*s_ops = NULL;
	uint64_t g_int = 0, s_int = 0;
	g_ops = memcached_stat_get_value(memc, stats, "cmd_get", &rc);
	s_ops = memcached_stat_get_value(memc, stats, "cmd_set", &rc);
	char *endptr;
	ops += strtoul(g_ops, &endptr, 10);
	ops += strtoul(s_ops, &endptr, 10);
	
	free(stats);
	return ops;
}

int get_input(char **dest, int lines) {
	debug("Gathering lines to send\n");
	int i, ret = -1, total = 0;
	for(i = 0; i < lines; ++i) {
		debug("Requesting input line from file or stdin [%d/%d]\n", i+1, lines);
		if(using_file) {
			ret = get_file_line(f_ptr, dest[i]);
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

int prep_request(char *request, char *key, char *val)
{
	int req = -1, ret = -1;
	
	if(using_file) {
		get_input(&request, MAX_LINES);
		req = parse_request(request, 
							key, MAX_KEY_SIZE,
							val, MAX_VAL_SIZE);
		ret = req;
	} else {
		strcpy(key, "key123");
		ret = GET;
	}
	return ret;
}

/* extract cid from programname */
void get_cid(char *prog_name) {
    char *curr = strtok(prog_name, ".");
    if(!curr) return;
    curr = strtok(NULL, ".");
    if(!curr) return;
    cid = strtoul(curr, NULL, 10);
}


int main(int argc, char **argv) {
 	memcached_st *memc;
 	memcached_return rc;
	size_t key_length;
	char *key = (char *)calloc(1, MAX_KEY_SIZE);
	char *value = (char *)calloc(1, MAX_VAL_SIZE);
	char *request = (char *)calloc(1, MAX_KEY_SIZE + MAX_VAL_SIZE + 50);
	uint32_t flags;

	if(parse_opts(argc, argv)) {
		exit(1);
	}

	if(use_id_based_file) {
        get_cid(argv[0]);
        char final_file[100] = {0,};
        snprintf(final_file, sizeof(final_file), "%s%d.in", file_name, cid);
        open_file(final_file);
    } else if(file_name) {
		open_file(file_name);
	}

	/* timing */
	struct timeval o_s, o_e, l_s, l_e;

	char config[MAX_CONFIG_LEN];
	memset(config, 0, sizeof(config));

	snprintf(config, sizeof(config) - 1, "--SERVER=%s:%s", ip, port);

	/* connect to the server */
	memc = memcached(config, strlen(config));
	if(memc == NULL) {
		log_err("memcached() failed with configuration: %s\n", config);
		exit(1);
	}
	printf("Connected successfully to memcached server on %s:%s\n", ip, port);
	fflush(stdout);

	/* start timer for ops/sec */
	bm_ops_start(&o_s);
	int i = 0, req = 0;
	size_t ret_value_len;
	char *ret_value = NULL;
	while(i < max_reqs) {
		bm_latency_start(&l_s);
		req = prep_request(request, key, value);
		key_length = strlen(key);
		switch(req) {
			case GET:
				//rc = memcached_mget(memc, (const char * const*)&key, &key_length, 1);
				ret_value = memcached_get(memc, key, key_length, &ret_value_len, NULL, &rc);
				if(rc != MEMCACHED_SUCCESS && rc != MEMCACHED_NOTFOUND) {
					fprintf(stderr, "memcached_mget() failed: %s\n", memcached_strerror(memc, rc));
				}
				bm_latency_end(&l_e);
				bm_latency_show("mcd", l_s, l_e);
				free(ret_value);
				/*char ret_key[MEMCACHED_MAX_KEY];
				memset(ret_key, 0, sizeof(ret_key));
				size_t ret_key_len;
				char *ret_value;
				size_t ret_value_len;
				while((ret_value = memcached_fetch(memc, ret_key, &ret_key_len, &ret_value_len, &flags, &rc))) {
					if(rc != MEMCACHED_SUCCESS) {
						printf("error retrieving value for key\n"); break;
					} 
					free(ret_value);
					memset(ret_key, 0, sizeof(ret_key));
				}*/
				break;
			default:
				rc = memcached_set(memc, key, key_length, value, strlen(value), 0, 0);
				if(rc != MEMCACHED_SUCCESS) {
					log_err("error inserting key\n");
				}
				break;
		}
		i++;
	}
	bm_ops_end(&o_e);
	uint64_t ops = get_mcd_ops(memc);
	bm_ops_show(ops, o_s, o_e);

	free(key);
	free(value);
	free(request);
	memcached_free(memc);
	return 0;
}

