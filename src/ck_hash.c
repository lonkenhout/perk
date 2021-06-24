#include "ck_hash.h"


/* Dan bernstein widely used hash implementation (including GLIB2) */
uint64_t ck_str_hash(unsigned char *str)
{
    int c;
    unsigned long hash = 5381;
    while (c = *str++) {
        hash = ((hash << 5) + hash) + c;
    }
    return (uint64_t)hash % CK_MAX_SIZE;
}

struct ck_hash_table *ck_hash_table_init()
{
	struct ck_hash_table *ct = (struct ck_hash_table *)calloc(1, sizeof(struct ck_hash_table));
	int i;
	for(i = 0; i < CK_MAX_SIZE; i++) {
		pthread_rwlock_init(&(ct->locks[i]), NULL);
	}
	return ct;
}

void ck_hash_table_destroy(struct ck_hash_table *ct)
{
	int i;
	for(i = 0; i < CK_MAX_SIZE; i++) {
		pthread_rwlock_destroy(&(ct->locks[i]));
	}
	free(ct);
}

void ck_hash_print_entries(struct ck_hash_table *ct)
{
	int i, j;
	int cnt = 0;
	for(j = 0; j < CK_MAX_POS; j++) {
		for(i = 0; i < CK_MAX_SIZE; i++) {
			if(ct->pairs[j][i].key[0] != 0) {
				printf("%d - {%s:%s}\n", cnt, ct->pairs[j][i].key, ct->pairs[j][i].val);
				cnt++;
			}
		}
	}
}

void ck_hash_table_insert(struct ck_hash_table *ct, char *key, char *val) 
{
	int i;
	uint64_t hash = ck_str_hash(key);
	for(i = 0; i < CK_MAX_POS; i++) {
		pthread_rwlock_wrlock(&(ct->locks[hash]));
		if(memcmp(ct->pairs[i][hash].key, key, MAX_KEY_SIZE) == 0 || ct->pairs[i][hash].key[0] == 0) {
			memcpy(ct->pairs[i][hash].key, key, MAX_KEY_SIZE);
			memcpy(ct->pairs[i][hash].val, val, MAX_VAL_SIZE);
			pthread_rwlock_unlock(&(ct->locks[hash]));
			return;
		}
		pthread_rwlock_unlock(&(ct->locks[hash]));
	}

	pthread_rwlock_wrlock(&(ct->locks[hash]));
	for(i = 0; i < CK_MAX_POS-1; i++) {
		memcpy(ct->pairs[i][hash].key, ct->pairs[i+1][hash].key, MAX_KEY_SIZE);
		memcpy(ct->pairs[i][hash].val, ct->pairs[i+1][hash].val, MAX_VAL_SIZE);
	}
	memcpy(key, ct->pairs[0][hash].key, MAX_KEY_SIZE);
	memcpy(val, ct->pairs[0][hash].val, MAX_VAL_SIZE);
	pthread_rwlock_unlock(&(ct->locks[hash]));
}

int ck_hash_table_get(struct ck_hash_table *ct, char *key, char *res)
{
	int i;
	uint64_t hash = ck_str_hash(key);
	for(i = 0; i < CK_MAX_POS; i++) {
		pthread_rwlock_rdlock(&(ct->locks[hash]));
		if(memcmp(ct->pairs[i][hash].key, key, MAX_KEY_SIZE) == 0) {
			memcpy(res, ct->pairs[i][hash].val, MAX_VAL_SIZE);
			pthread_rwlock_unlock(&(ct->locks[hash]));
			return 0;
		}
		pthread_rwlock_unlock(&(ct->locks[hash]));
	}
	return 1;
}

