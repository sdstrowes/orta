
#ifndef __MEMBERS_LIST_
#define __MEMBERS_LIST_

#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

struct _member_t
{
	uint32_t member;
	uint32_t seq;
	struct timeval tv;
	struct _member_t *next;
};

struct _members_list_t
{
	struct _member_t *head;
	uint32_t length;
	pthread_mutex_t *lock;
};

typedef struct _member_t member_t;
typedef struct _members_list_t members_list_t;

int members_init( members_list_t **list );

int members_add( members_list_t *list, uint32_t member );

int member_update( member_t *member, uint32_t seq );
int members_update( members_list_t *list, uint32_t member, uint32_t seq );

int members_rm( members_list_t *list, uint32_t member );

int members_contains( members_list_t *list, uint32_t member );

member_t *members_get( members_list_t *list, uint32_t member_ip );

int members_clear( members_list_t *m );

int members_length( members_list_t *n );

void members_ip_nums( members_list_t *n, int *array );

int members_destroy( members_list_t **m );

#endif
