
#ifndef _ROUTING_TABLE__
#define _ROUTING_TABLE__

#include <stdint.h>
#include <pthread.h>

typedef struct _route_t
{ 
	uint32_t source;
	uint32_t fwd_link;
	struct _route_t *next_link;
	struct _route_t *next_node;
} route_t;

typedef struct
{
	route_t *head;
	uint32_t length;
	pthread_mutex_t *lock;
} route_table_t;

int  routing_table_init( route_table_t **r );
void routing_table_add( route_table_t *r, uint32_t source, uint32_t fwd_link );
int  routing_table_rm( route_table_t *r, uint32_t source, uint32_t fwd_link );
int  routing_table_clear( route_table_t *r );
int  routing_table_destroy( route_table_t **r );

#endif
