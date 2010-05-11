#ifndef __FIFO_QUEUE__
#define __FIFO_QUEUE__

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#define TRUE 1
#define FALSE 0


typedef struct _queue_item
{
	void *data;
	struct _queue_item *next;
} queue_item_t;

typedef struct _queue
{
	queue_item_t *head;
	queue_item_t *tail;
	uint32_t length;
	pthread_mutex_t *lock;
	pthread_cond_t  *flag;
} queue_t;


/**
 * queue_init:
 * Initialises `queue' to be ready for addition or removal of items. 
 * Returns TRUE on successfully setting up the queue, FALSE otherwise.
 */
int queue_init( queue_t **queue );

/**
 * queue_add:
 * Adds `data' to `queue', returning TRUE on success and FALSE on failure.
 */
int  queue_add( queue_t *queue, void *data );


/**
 * queue_dequeue:
 * Removes the data element which has existed in the queue the longest.
 * Returns a pointer to the item if the queue is nonempty, NULL if empty.
 */
void* queue_dequeue( queue_t *queue );


/**
 * queue_clear:
 * Removes all items from `queue'.
 */
void queue_clear( queue_t *queue );

/**
 * queue_destroy:
 * 
 */
int queue_destroy( queue_t **queue );
#endif

