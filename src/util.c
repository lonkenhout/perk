
#include "./util.h"

int get_line(char *buff, size_t max)
{
	int ch, extra;

	if(read(STDIN_FILENO, buff, max) == 0) return EOF;

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
	for(int i = 0; i < (int)strlen(buff); i++) {
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

int get_file_line(FILE *input_file, char *buff, size_t max)
{
	int ch, extra;
	char *ret = NULL;
	//if(read(*input_file, buff, max) == 0) return EOF;
	ret = fgets(buff, max, input_file);
	if(ret == NULL) {
		return EOF;
	}
	/* If the message was too long, there'll be no newline. In that case we flush
	 * to EOL so that any excess does not affect the next call. */
	printf("char at %ld is %c, max is %ld\n", strlen(buff)-1, buff[strlen(buff)-1], max);
	if(buff[strlen(buff)-1] != '\n') {
		extra = 0;
		/* Clean up any extranneous chars */
		while(((ch = getchar()) != '\n') && (ch != EOF))
			extra = 1;
		return (extra == 1) ? TOO_LONG : OK;
	}

	/* Check for ansi escape characters and remove trailing chars by spaces */
	int ansi_flag = 0;
	for(int i = 0; i < (int)strlen(buff); i++) {
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