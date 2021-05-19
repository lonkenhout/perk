
#include "./util.h"


int parse_request(char *request, 
				char *k, size_t k_sz,
				char *v, size_t v_sz)
{
	int ret = EMPTY;
	switch(request[0]) {
		case 'P':
			ret = parse_put_request(request + 2, k, k_sz, v, v_sz);
			break;
		case 'G':
			ret = parse_get_request(request + 2, k, k_sz);
			break;
		case 'E':
			ret = EXIT;
			break;
		default:
			ret = EMPTY;
			break;
	}
	return ret;
}

int parse_put_request(char *request, 
				char *k, size_t k_sz,
				char *v, size_t v_sz)
{
	int i = 0, j = 0;
	while(request[i] != ':' && j < k_sz) {
		k[j++] = request[i++];
	}
	if(j > k_sz) return MALFORMED;
	k[j] = '\0';
	i++;

	j = 0;
	while(request[i] != '\0' && j < v_sz) {
		v[j++] = request[i++];
	}
	if(j > v_sz) return MALFORMED;
	v[j] = '\0';

	return PUT;
}

int parse_get_request(char *request, 
				char *k, size_t k_sz)
{
	int i = 0;
	while(request[i] != '\0' && i < k_sz) {
		k[i] = request[i];
		i++;
	}
	if(i > k_sz) return MALFORMED;
	k[i] = '\0';
	return GET;
}



int get_line(char *buff)
{
	int ch, extra, i;
	if(read(STDIN_FILENO, buff, MAX_KEY_SIZE + MAX_VAL_SIZE + 10) == 0) return EOF;

	/* If the message was too long, there'll be no newline. In that case we flush
	 * to EOL so that any excess does not affect the next call. */
	if(buff[strlen(buff)-1] != '\n') {
		extra = 0;
		/* Clean up any extranneous chars */
		while(((ch = getchar()) != '\n') && (ch != EOF))
			extra = 1;
		return (extra == 1) ? TOO_LONG : OK;
	}

	/* Check for ansi escape characters and remove trailing chars by spaces */
	int ansi_flag = 0;
	for(i = 0; i < (int)strlen(buff); i++) {
		if(buff[i] == '\33') {
			buff[i] = ' '; i++;
			while(buff[i] != ' ' && buff[i] != '\33' && buff[i] != '\0' && buff[i] != '\t' && i < (int)strlen(buff)) {
				buff[i] = ' ';
				i++;
				ansi_flag = 1;
			}
			if(ansi_flag == 1) {
				i--; ansi_flag = 0;
			}
		}
	}

	/* Otherwise remove newline and store string in buff */
	buff[strlen(buff)-1] = '\0';
	return OK;
}

int get_file_line(FILE *input_file, char *buff)
{
	int ch, extra;
	char *ret = NULL;
	int i;
	//if(read(*input_file, buff, max) == 0) return EOF;
	ret = fgets(buff, MAX_KEY_SIZE + MAX_VAL_SIZE + 10, input_file);
	if(ret == NULL) {
		return EOF;
	}
	/* If the message was too long, there'll be no newline. In that case we flush
	 * to EOL so that any excess does not affect the next call. */
	if(buff[strlen(buff)-1] != '\n') {
		extra = 0;
		/* Clean up any extranneous chars */
		while(((ch = getchar()) != '\n') && (ch != EOF))
			extra = 1;
		return (extra == 1) ? TOO_LONG : OK;
	}

	/* Check for ansi escape characters and remove trailing chars by spaces */
	int ansi_flag = 0;
	for(i = 0; i < (int)strlen(buff); i++) {
		if(buff[i] == '\33') {
			buff[i] = ' '; i++;
			while(buff[i] != ' ' && buff[i] != '\33' && buff[i] != '\0' && buff[i] != '\t' && i < (int)strlen(buff)) {
				buff[i] = ' ';
				i++;
				ansi_flag = 1;
			}
			if(ansi_flag == 1) {
				i--; ansi_flag = 0;
			}
		}
	}

	/* Otherwise remove newline and store string in buff */
	buff[strlen(buff)-1] = '\0';
	return OK;
}

/* Code acknowledgment: rping.c from librdmacm/examples */
int get_addr(char *dst, struct sockaddr *addr)
{
	struct addrinfo *res;
	int ret = -1;
	ret = getaddrinfo(dst, NULL, NULL, &res);
	if (ret) {
		log_err("getaddrinfo failed - invalid hostname or IP address");
		return ret;
	}
	memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
	freeaddrinfo(res);
	return ret;
}

int get_addr_port(char *res, struct sockaddr *addr)
{
	socklen_t client_len = sizeof(struct sockaddr_storage);
	char host[NI_MAXHOST] = {0,};
	char port[NI_MAXSERV] = {0,};
	int ret = getnameinfo(addr,
						 client_len,
						 host,
						 sizeof(host),
						 port,
						 sizeof(port),
						 NI_NUMERICHOST | NI_NUMERICSERV);
	if(ret) {
		log_err("getnameinfo() failed");
		return ret;
	}
	strcat(res, host);
	strcat(res, ":");
	strcat(res, port);
	return 0;
}

int addr_eq(struct sockaddr *addr1, struct sockaddr *addr2)
{
	int i, ret = 1;
	for(i = 0; i < 6; ++i) {
		if(addr1->sa_data[i] != addr2->sa_data[i]) {
			return 0;
		}
	}
	return ret;
}

void get_time(struct timeval *t)
{
	if(gettimeofday(t, 0) != 0) {
		log_err("gettimeofday() failed");
		exit(1);
	}
}

void print_time_diff(char *msg, struct timeval t_s, struct timeval t_e)
{
	double time = compute_time(t_s, t_e, SCALE_MCSEC);
	printf("== benchmark [latency:%s][%.4f usec]\n", msg, time);
}

void print_ops_per_sec(uint64_t ops, struct timeval t_s, struct timeval t_e)
{
	double time = compute_time(t_s, t_e, SCALE_MSEC);
	printf("== processed %ld requests in %.0f msec\n", ops, time);
	printf("== benchmark [ops_per_sec][%.1f]\n", ops/(time/1000.0));
}

char *req_type_str(enum REQUEST_TYPE type)
{
	char *ret = NULL;
	switch(type) {
		case GET:
			ret = "GET"; break;
		case PUT:
			ret = "PUT"; break;
		case RESPONSE_OK:
			ret = "RESPONSE_OK"; break;
		case RESPONSE_EMPTY:
			ret = "RESPONSE_EMPTY"; break;
		case RESPONSE_ERR:
			ret = "RESPONSE_ERR"; break;
		case EMPTY:
			ret = "EMPTY"; break;
		case EXIT:
			ret = "EXIT"; break;
		case EXIT_OK:
			ret = "EXIT_OK"; break;
		case MALFORMED:
			ret = "MALFORMED"; break;
		default:
			ret = "?"; break;
	}
	return ret;
}

void print_request(struct request req, struct request res)
{
	char *rq_tp = req_type_str(req.type);
	char *rs_tp = req_type_str(res.type);
	if(req.type == PUT) {
		printf("[%s] %s:%s ===> [%s]\n", rq_tp, req.key, req.val, rs_tp);
	} else {
		printf("[%s] %s ===> [%s] %s\n", rq_tp, req.key, rs_tp, (res.type == RESPONSE_OK)? res.val:"-");
	}
}

double compute_time(struct timeval start, struct timeval end, double scale)
{
	return ((end.tv_sec + (end.tv_usec / 1000000.0)) -
			(start.tv_sec + (start.tv_usec / 1000000.0))) * scale;
}

void print_curr_time(void)
{
	struct timeval t;
	if(gettimeofday(&t, 0) != 0) {
		log_err("gettimeofday failed");
		return;
	}
	debug("tv_sec[%ld] tv_usec [%ld]\n", t.tv_sec, t.tv_usec);
}
