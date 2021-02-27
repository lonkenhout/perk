/* Client source */

#include "./client.h"

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
				printf("optarg for a: %s\n", optarg);
				break;
			case 'p':
				printf("optarg for p: %s\n", optarg);
				break;
			default:
				print_usage(argv[0]);
				ret = -1;
				break;
		}
	}
	return ret;
}



int main(int argc, char **argv){
	if(argc < 2){
		print_usage(argv[0]);
	}
	return parse_opts(argc, argv);
}
