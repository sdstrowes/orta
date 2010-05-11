#include "linked_list.h"
#include <stdio.h>

/**
 * list_init:
 * Allocates space and initialises a linked list, sorted by increasing order
 * on each entry's key; `list' will point to the linked list structure after 
 * initialisation, or NULL if the operation fails.
 * Returns TRUE on success, FALSE on failure.
 */
int list_init( linked_list_t **list )
{
	linked_list_t *l= (linked_list_t*)malloc(sizeof(linked_list_t));

	if ( l ) {
		l->head= NULL;
		l->tail= NULL;
		l->length= 0;

		l->lock= (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init( l->lock, NULL );

		*list= l;
		return TRUE;
	}

	list= NULL;
	return FALSE;
}

/**
 * list_add:
 * Adds `data' to the linked list, to be identified by `key'. If that key 
 * already exists, the existing data element is free()'d, and the new data 
 * element is plugged in in it's place.
 * FIXME: Always returns TRUE.
 */
int list_add( linked_list_t *list, uint32_t key, void *data )
{
	list_item_t *tempitem;
	list_item_t *item= (list_item_t*)malloc(sizeof(list_item_t));

	if ( list == NULL )
		return FALSE;

	item->key=  key;
	item->data= data;

	/* Empty list */
	if ( !list->length ) {
		item->prev= NULL;
		item->next= NULL;

		list->length= 1;
		list->head= item;
		list->tail= item;

		return TRUE;
	}

	/* New key is smaller than existing minimum key */
	if ( key < (list->head->key) ) {
		item->prev= NULL;
		item->next= list->head;

		list->length++;
		list->head->prev= item;
		list->head= item;

		return TRUE;
	}

	/* New key is larger than existing maximum key */
	if ( key > (list->tail->key) ) {
		item->prev= list->tail;
		item->next= NULL;

		list->length++;
		list->tail->next= item;
		list->tail= item;

		return TRUE;
	}

	/* New key is somewhere in the middle; scan to correct point. */
	tempitem= list->head;
	while ( key > (tempitem->key) ) {
		tempitem= tempitem->next;
	}

	/* We're replacing a key; free up memory and switch pointers */
	if ( key == tempitem->key ) {
		free(tempitem->data);
		free(item);

		tempitem->data= data;

		return TRUE;
	}
	/* Else, insert new data at this point in the list */
	else {
		item->prev= tempitem->prev;
		item->next= tempitem;

		list->length++;
		tempitem->prev->next= item;
		tempitem->prev= item;
	}

	return TRUE;
}


/**
 * list_rm:
 * Removes the data tagged by the value `key' from the list, leaving `data' 
 * to point at said data.
 * Returns TRUE if the key is found and the data is returned; FALSE otherwise 
 * (setting *data to NULL if so).
 */
int list_rm( linked_list_t *list, uint32_t key, void **data )
{
	list_item_t *tempitem;

	if ( list == NULL ) {
		*data= NULL;
		return FALSE;
	}

	/* If the list is empty, clearly the item is not found */
	if ( !list->length ) {
		*data= NULL;
		return FALSE;
	}

	/* Scan the list for the key */
	tempitem= list->head;
	while ( tempitem != NULL && key != (tempitem->key) ) {
		tempitem= tempitem->next;
	}

	/* Item wasn't found. */
	if ( tempitem == NULL ) {
		*data= NULL;
		return FALSE;
	}

	/* Item was found, fix pointers, return TRUE */
	tempitem->prev->next= tempitem->next;
	tempitem->next->prev= tempitem->prev;
	if ( list->head == tempitem )
		list->head= tempitem->next;
	if ( list->tail == tempitem )
		list->tail= tempitem->prev;

	list->length--;

	/* Pass pointer back, in case it needs free()'d. */
	*data= tempitem->data;

	free( tempitem );

	return TRUE;
}


/**
 * list_rm_min:
 * Removes the data element indexed by the lowest key in the list. Returns 
 * -1 if the list is empty, setting *data to NULL; otherwise, points *data to 
 * the data being removed from the list, and returns the key value which has 
 * just been removed.
 */
uint32_t  list_rm_min( linked_list_t *list, void **data )
{
	uint32_t returnval;
	list_item_t *temp;

	if ( list == NULL ) {
		*data= NULL;
		return FALSE;
	}

	/* If the list is empty, clearly the item is not found */
	if ( !list->length ) {
		*data= NULL;
		return -1;
	}

	returnval= list->head->key;
	temp= list->head;
	*data= list->head->data;

	list->head= list->head->next;
	/* Set prev to be NULL on new head item */
	if ( list->head != NULL )
		list->head->prev= NULL;

	free(temp);
	list->length--;

	return returnval;
}


/**
 * list_rm_max:
 * Removes the data element indexed by the largest key in the list. Returns 
 * -1 if the list is empty, setting *data to NULL; otherwise, points *data to 
 * the data being removed from the list, and returns the key value which has 
 * just been removed.
 */
uint32_t  list_rm_max( linked_list_t *list, void **data )
{
	uint32_t returnval;
	list_item_t *temp;

	if ( list == NULL ) {
		*data= NULL;
		return FALSE;
	}

	/* If the list is empty, clearly the item is not found */
	if ( !list->length ) {
		*data= NULL;
		return -1;
	}

	returnval= list->tail->key;
	temp= list->tail;
	*data= list->tail->data;

	list->tail= list->tail->prev;
	/* Set next to be NULL on new tail item */
	if ( list->tail != NULL )
		list->tail->next= NULL;
	free(temp);
	list->length--;

	return returnval;
}


/**
 * list_contains:
 * Scans the list for the indicated key, returning TRUE if the item is found, 
 * or false if not.
 */
int list_contains( linked_list_t *list, uint32_t key )
{
	list_item_t *tempitem;

	if ( list == NULL )
		return FALSE;

	tempitem= list->head;
	while ( tempitem != NULL && key != (tempitem->key) ) {
		tempitem= tempitem->next;
	}

	if ( tempitem == NULL )
		return FALSE;

	return TRUE;
}



/**
 * list_get:
 * Finds the data indexed by `key', and makes *data point to it. Neither the 
 * data or the key are removed from the list, this just allows access to data 
 * held in the list.
 * Returns a TRUE on successfully finding the key, or FALSE if not (also 
 * setting *data to NULL).
 */
int list_get( linked_list_t *list, uint32_t key, void **data )
{
	list_item_t *tempitem;

	if ( list == NULL ) {
		*data= NULL;
		return FALSE;
	}

	for (tempitem= list->head; tempitem != NULL; tempitem= tempitem->next){
		if ( key == tempitem->key )
			break;
	}

	/* Item was not found */
	if ( tempitem == NULL ) {
		*data= NULL;
		return FALSE;
	}

	/* Item was found */
	*data= tempitem->data;
	return TRUE;
}


/**
 * list_destroy:
 * Empties the list, and sets *list to NULL.
 */
void list_destroy( linked_list_t **list )
{
        linked_list_t *l= *list;
	/*void *data= malloc(sizeof(void*));*/
	void *data;

	while ( l->length ) {
		list_rm_min( l, &data );
		free(data);
	}

	pthread_mutex_destroy( l->lock );
	free( l->lock );
	free(l);
	*list= NULL;
}
