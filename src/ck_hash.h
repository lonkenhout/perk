#ifndef __CK_HASH_UTIL_H__
#define __CK_HASH_UTIL_H__

#include "util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdint.h>

#include <pthread.h>

#define CK_MAX_SIZE 10000
#define CK_MAX_POS 2

/* structure used internally for storing the entries */
struct pair{
	uint64_t kh;
	char key[MAX_KEY_SIZE];
	char val[MAX_VAL_SIZE];
};

/* the hash table is a set of 2 arrays */
struct ck_hash_table{
	struct pair pairs[CK_MAX_POS][CK_MAX_SIZE];
	pthread_rwlock_t locks[CK_MAX_SIZE];
	uint64_t count;
};


/* initialize the cuckoo hash table */
struct ck_hash_table *ck_hash_table_init();

/* destroy hash table referred to by ct */
void ck_hash_table_destroy(struct ck_hash_table *ct);

/* print all entries in the hash table ct */
void ck_hash_print_entries(struct ck_hash_table *ct);

/* insert a new key-value pair into the hash table ct*/
void ck_hash_table_insert(struct ck_hash_table *ct, char *key, char *val);

/* retrieve a hash table entry, store the value for that key in dst */
int ck_hash_table_get(struct ck_hash_table *ct, char *key, char *dst);

#endif
