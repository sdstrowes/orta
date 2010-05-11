
#include "fifo_queue.h"

#include <stdio.h>


/**
 * queue_init:
 * Initialises `queue' to be ready for addition or removal of items. 
 * Returns TRUE on successfully setting up the queue, FALSE otherwise.
 */
int queue_init( queue_t **queue )
{
	queue_t *q= (queue_t*)malloc(sizeof(queue_t));

	if ( q ) {
		q->head= NULL;
		q->length= 0;

		q->lock= (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init( q->lock, NULL );
		q->flag= (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
		pthread_cond_init ( q->flag, NULL );

		*queue= q;
		return TRUE;
	}
	return FALSE;
}


/**
 * queue_add:
 * Adds `data' to `queue', returning TRUE on success and FALSE on failure.
 */
int  queue_add( queue_t *queue, void *data )
{
	queue_item_t *item= (queue_item_t*)malloc(sizeof(queue_item_t));
	queue_item_t *tempitem, *previtem;

	/* Something's gone horribly wrong. Abort! Abort! */
	if ( item == NULL )
		return FALSE;

	item->data= data;
	item->next= NULL;

	 /* if empty */
	if ( !queue->length ) {
		queue->head= item;
		queue->tail= item;
		queue->length= 1;

		return TRUE;
	}

	/* else !empty */
	queue->tail->next= item;
	queue->tail= item;
	queue->length++;

	return TRUE;
}

/**
 * queue_dequeue:
 * Removes the data element which has existed in the queue the longest.
 * Returns a pointer to the item if the queue is nonempty, NULL if empty.
 */
void* queue_dequeue( queue_t *queue )
{
	queue_item_t *item;
	void* data;

	if ( queue->length ) {
		item= queue->head;
		queue->head= item->next;
		data= item->data;
		/* Discard item */
		free(item);
		queue->length--;

		return data;
	}
	else
		return NULL;
}


/**
 * queue_clear:
 * Removes all items from `queue'.
 */
void queue_clear( queue_t *queue )
{
	while ( queue->length ) {
		free( queue_dequeue( queue ) );
	}
}


/**
 * queue_destroy:
 * 
 */
int queue_destroy( queue_t **queue )
{
	queue_t *q= *queue;

	queue_clear( q );
	pthread_mutex_unlock( q->lock );
	pthread_mutex_destroy( q->lock );
	free( q->lock );
	free( q );
	*queue= NULL;

	return TRUE;
}
