#include "ordered_queue.h"
/*#include "dijkstra.h"*/


/* FIXME: Want a more generic implementation of this. */
int d_queue_add( queue_t *queue, uint32_t ip, uint32_t ip2, uint32_t distance )
{
	/* Variables added to list */
	queue_item_t *q_link= (queue_item_t*)malloc(sizeof(queue_item_t));
	queue_data_t *data= (queue_data_t*)malloc(sizeof(queue_data_t));

	/* Temporary variables */
	queue_item_t *tempitem, *previtem;
	queue_data_t *tempdata;

	/* Sort out data element */
	data->ip= ip;
	data->ip2= ip2;
	data->distance= distance;

	q_link->data= data;

	if ( !queue->length ) { /* Empty */
		/* Sort out link in queue */
		q_link->next= NULL;

		/* Add to queue */
		queue->head= q_link;
		queue->tail= q_link;
		queue->length= 1;

		return TRUE;
	}

	/* Check to see whether or not item exists in queue. If it does, 
	 * remove it and add it again to position it properly. */
	tempitem= queue->head;
	previtem= NULL;
	tempdata= tempitem->data;
	while ( tempitem != NULL && ip != tempdata->ip ) {
		previtem= tempitem;
		tempitem= tempitem->next;
		if ( tempitem != NULL )
			tempdata= tempitem->data;
	}
	if ( tempitem != NULL && ip == tempdata->ip ) {
		if ( previtem == NULL )
			queue->head= tempitem->next;
		else
			previtem->next= tempitem->next;

		if ( tempitem == queue->tail )
			queue->tail= previtem;

		queue->length--;
		free( tempitem->data );
		free( tempitem );
		free( q_link );
		free( data );
		return d_queue_add( queue, ip, ip2, distance );
	}

	/* We don't have the IP. Can simply add to queue. */
	tempitem= queue->head;
	previtem= NULL;
	tempdata= tempitem->data;
	/* Loop while there are more links to loop over AND the distances 
	 * referred to by those links are still less than the distance we 
	 * want to insert. */
	while ( tempitem != NULL && distance > tempdata->distance ) {
		previtem= tempitem;
		tempitem= tempitem->next;
		if ( tempitem != NULL )
			tempdata= tempitem->data;
	}

	/* If tempitem == NULL, tempitem has run past the end of the queue, 
	 * and previtem points to the tail of the queue. Fix queue->tail. */
	if ( tempitem == NULL )
		queue->tail= q_link;

	/* If previtem == NULL, we haven't moved from the head of the list.
	 * If this is the case, we know we want to add to the front of the
	 * list. Sort out pointers in the case that we're adding to the front,
	 * or we're not. */
	if ( previtem == NULL )
		queue->head= q_link;
	else
		previtem->next= q_link;

	q_link->next= tempitem;

	queue->length++;

	return TRUE;
}

typedef struct
{
	uint32_t sd;
	double utility;
} util_queue_data_t;

/* FIXME: Want a more generic implementation of this. */
int util_queue_add( queue_t *queue, uint32_t sd, double utility )
{
	/* Variables added to list */
	queue_item_t *q_link= (queue_item_t*)malloc(sizeof(queue_item_t));
	util_queue_data_t *data= (util_queue_data_t*)malloc(sizeof(util_queue_data_t));

	/* Temporary variables */
	queue_item_t *tempitem, *previtem;
	util_queue_data_t *tempdata;

	/* Sort out data element */
	data->sd= sd;
	data->utility= utility;

	q_link->data= data;

	if ( !queue->length ) { /* Empty */
		/* Sort out link in queue */
		q_link->next= NULL;

		/* Add to queue */
		queue->head= q_link;
		queue->tail= q_link;
		queue->length= 1;

		return TRUE;
	}

	/* Check to see whether or not item exists in queue. If it does, 
	 * remove it and add it again to position it properly. */
	tempitem= queue->head;
	previtem= NULL;
	tempdata= tempitem->data;
	while ( tempitem != NULL && sd != tempdata->sd ) {
		previtem= tempitem;
		tempitem= tempitem->next;
		if ( tempitem != NULL )
			tempdata= tempitem->data;
	}
	if ( tempitem != NULL && sd == tempdata->sd ) {
		if ( previtem == NULL )
			queue->head= tempitem->next;
		else
			previtem->next= tempitem->next;

		if ( tempitem == queue->tail )
			queue->tail= previtem;

		queue->length--;
		free( tempitem->data );
		free( tempitem );
		free( q_link );
		free( data );
		return util_queue_add( queue, sd, utility );
	}

	/* We don't have the IP. Can simply add to queue. */
	tempitem= queue->head;
	previtem= NULL;
	tempdata= tempitem->data;
	/* Loop while there are more links to loop over AND the distances 
	 * referred to by those links are still less than the distance we 
	 * want to insert. */
	while ( tempitem != NULL && utility < tempdata->utility ) {
		previtem= tempitem;
		tempitem= tempitem->next;
		if ( tempitem != NULL )
			tempdata= tempitem->data;
	}

	/* If tempitem == NULL, tempitem has run past the end of the queue, 
	 * and previtem points to the tail of the queue. Fix queue->tail. */
	if ( tempitem == NULL )
		queue->tail= q_link;

	/* If previtem == NULL, we haven't moved from the head of the list.
	 * If this is the case, we know we want to add to the front of the
	 * list. Sort out pointers in the case that we're adding to the front,
	 * or we're not. */
	if ( previtem == NULL )
		queue->head= q_link;
	else
		previtem->next= q_link;

	q_link->next= tempitem;

	queue->length++;

	return TRUE;
}

int util_queue_dequeue( queue_t *queue, uint32_t *sd, double *utility )
{
	queue_item_t *item;
	util_queue_data_t *data;

	if ( queue->length ) {
		item= queue->head;
		queue->head= item->next;

		data= item->data;
		*sd= data->sd;
		*utility= data->utility;

		/* Discard item */
		free(data);
		free(item);
		queue->length--;

		return TRUE;
	}
	else {
		*sd= 0;
		*utility= 0;
		return FALSE;
	}
}

/* FIXME: Want a more generic implementation of this. */
int  d_queue_contains( queue_t *queue, uint32_t ip )
{
	queue_item_t *tempitem;
	queue_data_t *tempdata;

	if ( !queue->length )
		return -1;

	tempitem= queue->head;
	tempdata= tempitem->data;
	while ( tempitem != NULL && ip != (tempdata->ip) ) {
		tempitem= tempitem->next;
		if ( tempitem != NULL )
			tempdata= tempitem->data;
	}

	if ( tempitem == NULL )
		return -1;

	return tempdata->distance;
}

