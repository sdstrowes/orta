#ifndef _LINKED_LIST__
#define _LINKED_LIST__

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

#define TRUE 1
#define FALSE 0

struct _list_item
{
	struct _list_item *prev;
	struct _list_item *next;
	uint32_t key;
	void* data;
};

typedef struct _list_item list_item_t;

typedef struct _linked_list
{
	list_item_t *head;
	list_item_t *tail;
	uint32_t length;
	pthread_mutex_t *lock;
} linked_list_t;


/**
 * list_init:
 * Allocates space and initialises a linked list, sorted by increasing order
 * on each entry's key; `list' will point to the linked list structure after 
 * initialisation, or NULL if the operation fails.
 * Returns TRUE on success, FALSE on failure.
 */
int list_init( linked_list_t **list );

/**
 * list_add:
 * Adds `data' to the linked list, to be identified by `key'. If that key 
 * already exists, the existing data element is free()'d, and the new data 
 * element is plugged in in it's place.
 * FIXME: Always returns TRUE.
 */
int list_add( linked_list_t *list, uint32_t key, void *data );

/**
 * list_rm:
 * Removes the data tagged by the value `key' from the list, leaving `data' 
 * to point at said data.
 * Returns TRUE if the key is found and the data is returned; FALSE otherwise 
 * (setting *data to NULL if so).
 */
int list_rm( linked_list_t *list, uint32_t key, void **data );

/**
 * list_rm_min:
 * Removes the data element indexed by the lowest key in the list. Returns 
 * -1 if the list is empty, setting *data to NULL; otherwise, points *data to 
 * the data being removed from the list, and returns the key value which has 
 * just been removed.
 */
uint32_t list_rm_min( linked_list_t *list, void **data );

/**
 * list_rm_max:
 * Removes the data element indexed by the largest key in the list. Returns 
 * -1 if the list is empty, setting *data to NULL; otherwise, points *data to 
 * the data being removed from the list, and returns the key value which has 
 * just been removed.
 */
uint32_t list_rm_max( linked_list_t *list, void **data );

/**
 * list_contains:
 * Scans the list for the indicated key, returning TRUE if the item is found, 
 * or false if not.
 */
int list_contains( linked_list_t *list, uint32_t key );

/**
 * list_get:
 * Finds the data indexed by `key', and makes *data point to it. Neither the 
 * data or the key are removed from the list, this just allows access to data 
 * held in the list.
 * Returns a TRUE on successfully finding the key, or FALSE if not (also 
 * setting *data to NULL).
 */
int list_get( linked_list_t *list, uint32_t key, void **data );

/**
 * list_destroy:
 * Empties the list, and sets *list to NULL.
 */
void list_destroy( linked_list_t **list );

#endif

