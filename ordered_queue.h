#ifndef __ORDERED_QUEUE_
#define __ORDERED_QUEUE_

#include "fifo_queue.h"

int d_queue_add(queue_t *queue, uint32_t ip, uint32_t ip2, uint32_t distance );

int util_queue_add(     queue_t *queue, uint32_t sd,  double utility  );
int util_queue_dequeue( queue_t *queue, uint32_t *sd, double *utility );

int  d_queue_contains( queue_t *queue, uint32_t ip );

typedef struct
{
	uint32_t distance;
	uint32_t ip;
	/* FIXME: Variable names. IP2 is simply the other end of the
	   connection, */
	uint32_t ip2;
} queue_data_t;

#endif
