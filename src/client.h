#ifndef __PERK_CLIENT_H__
#define __PERK_CLIENT_H__

/* standard includes */
#include <stdio.h>

/* project includes */
#include "./util.h"
#include "./rdma.h"

/* parse client-side command line arguments */
int parse_opts(int argc, char **argv);

/* print help */
void print_usage(char *cmd);

/* gets some number of input lines, either from a file or stdin */
int get_input(char **dest, int lines);

/* extract client id from the program name */
void get_cid(char *prog_name);

#endif
