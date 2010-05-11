#include "neighbours.h"
#include "common_defs.h"
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>


/**
 * neighbours_init:
 * Initiates the neighbours list and makes `list' point to it. If the init 
 * fails, this function returns FALSE, but TRUE otherwise.
 */
int neighbours_init( neighbours_list_t **list )
{
	neighbours_list_t *l= 
		(neighbours_list_t*)malloc(sizeof(neighbours_list_t));

	if ( l ) {
		l->head= NULL;
		l->length= 0;
		l->max_sd= 0;

		l->lock= (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init( l->lock, NULL );

		*list= l;
		return TRUE;
	}
	return FALSE;
}


/**
 * neighbours_add:
 * Adds the neighbour connected to through `sd' and described by `addr' with 
 * a distance of `distance' to the neighbours list.
 */
int neighbours_add( neighbours_list_t *list, 
		    uint32_t sd, 
		    struct sockaddr_in *addr )
{
	neighbour_t *item;
	neighbour_t *tempitem, *previtem;

	if ( neighbours_contains( list, addr->sin_addr.s_addr ) )
		return FALSE;

	item= (neighbour_t*)malloc(sizeof(neighbour_t));

	if ( item == NULL )
		return FALSE;

	item->sd= sd;
	item->addr= addr;
	item->distance= DEFAULT_DIST;

	if ( sd > list->max_sd ) 
		list->max_sd= sd;


	if ( !list->length ) { /* Empty */
		item->next= NULL;

		list->length= 1;
		list->head= item;

		return TRUE;
	}

	if ( sd < (list->head->sd) ) {
		item->next= list->head;

		list->length++;
		list->head= item;

		return TRUE;
	}

	tempitem= list->head;
	previtem= NULL;
	while ( tempitem != NULL && sd > (tempitem->sd) ) {
		previtem= tempitem;
		tempitem= tempitem->next;
	}

	if ( tempitem == NULL ) {
		item->next= NULL;

		list->length++;
		previtem->next= item;
		return TRUE;
	}

	/* We already have this neighbour... */
	if ( sd == (tempitem->sd) ) {
		free(item);

		return FALSE;
	}

	item->next= tempitem;

	previtem->next= item;
	item->next= tempitem;
	list->length++;
	list->max_sd= sd;

	return TRUE;
}


/**
 * neighbour_update:
 * Sets the distance to this neighbour if the neighbour is recently added and 
 * has not yet had a distance set, or smooths the value giving bias to 
 * previous noted distances, to avoid major fluctuations in the reported 
 * distance.
 */
int neighbour_update( neighbour_t *n, 
		      uint32_t distance )
{
	/* Update smoothed link weight */
	if ( n->distance == DEFAULT_DIST )
		n->distance= distance;
	else
		n->distance= PREV_WEIGHT*(n->distance)+(1-PREV_WEIGHT)*distance;

	return n->distance;
}


/**
 * neighbours_get_nbr:
 * Retrieves the neighbour_t which is held for `sd'. Returns NULL if that 
 * socket descriptor is not held here.
 */
neighbour_t *neighbours_get_nbr( neighbours_list_t *list, uint32_t sd )
{
	neighbour_t *tempitem= list->head;

	while ( tempitem != NULL && sd != (tempitem->sd) )
		tempitem= tempitem->next;

	/* If we've found the neighbour in question */
	if ( tempitem != NULL )
		return tempitem;

	/* Else */
	return NULL;
}

/**
 * neighbours_get_addr:
 * Retrieves the (struct sackaddr_in*) for the associated socket descriptor, or
 * NULL if that socket descriptor does not exist.
 */
struct sockaddr_in *neighbours_get_addr( neighbours_list_t *list, uint32_t sd )
{
	neighbour_t *tempitem= list->head;

	while ( tempitem != NULL && sd != (tempitem->sd) )
		tempitem= tempitem->next;

	/* If we've found the neighbour in question */
	if ( tempitem != NULL )
		return tempitem->addr;

	/* Else */
	return NULL;
}


/**
 * neighbours_rm:
 * Removes the neighbour pointed to by `sd' and returns the associated
 * (struct sackaddr_in*) for that socket descriptor, or NULL if that socket
 * descriptor does not point to a known neighbour.
 */
struct sockaddr_in * neighbours_rm( neighbours_list_t *list, uint32_t sd )
{
	neighbour_t *tempitem, *previtem;
	struct sockaddr_in *addr;

	tempitem= list->head;
	previtem= NULL;
	while ( tempitem != NULL && sd != (tempitem->sd) ) {
		previtem= tempitem;
		tempitem= tempitem->next;
	}

	/* Socket descriptor not found */
	if ( tempitem == NULL )
		return NULL;

	/* Socket descriptor found, fix pointers */
	if ( previtem == NULL )
		list->head= tempitem->next;
	else 
		previtem->next= tempitem->next;

	/* Fix maximum known socket descriptor. List is sorted by socket 
	 * descriptor, so if we're removing the maximum sd, simply set the 
	 * sd to be that of the previous in the list, or zero if no other 
	 * exists. */
	if ( tempitem->sd == list->max_sd ) {
		if ( previtem != NULL )
			list->max_sd= previtem->sd;
		else
			list->max_sd= 0;
	}

	/* Decrement length and return */
	list->length--;

	addr= tempitem->addr;
	free( tempitem );

	return addr;
}


/**
 * neighbours_max_sd:
 * Returns the maximum known socket descriptor for any neighbour; if the list 
 * is empty, returns 0.
 */
int neighbours_max_sd( neighbours_list_t *list )
{
	if ( !list->length )
		return 0;
	/*else
	  return list->max_sd;*/
	neighbour_t *tempitem= list->head;

	while (tempitem->next != NULL) {
		tempitem= tempitem->next;
	}
	return tempitem->sd;
}


/**
 * neighbours_contains:
 * Searches for the given IP address in the neighbours list, returning TRUE 
 * if that neighbour is found, FALSE otherwise.
 */
int neighbours_contains( neighbours_list_t *list, uint32_t neighbour )
{
	neighbour_t *tempitem;

	if ( !list->length )
		return FALSE;

	tempitem= list->head;
	while (tempitem != NULL && 
	       neighbour != (tempitem->addr->sin_addr.s_addr)) {
		tempitem= tempitem->next;
	}
	if ( tempitem == NULL )
		return FALSE;

	return tempitem->sd;
}


/* print_neighbours */
#ifdef ORTA_DEBUG
static void p_n( neighbours_list_t *neighbours )
{
        neighbour_t *neighbour;

        neighbour= neighbours->head;

        printf( "---- NEIGHBOURS: (length: %d; max: %d) --\n",
                neighbours->length, neighbours->max_sd );
        printf( "-- SD --+---- IP Addr -----------+------ DISTANCE ------+\n");
        while ( neighbour != NULL ) {
                printf( "%u\t|\t%s\t|\t%d\t\n",
                        neighbour->sd,
                        inet_ntoa(neighbour->addr->sin_addr),
                        neighbour->distance
                        );
                neighbour= neighbour->next;
        }
}
#endif

int neighbours_clear( neighbours_list_t *n )
{
	int sd;
	sockaddr_in_t *addr;

	while ( n->length ) {
#ifdef ORTA_DEBUG
		p_n( n );
#endif
                sd= neighbours_max_sd( n );
                addr= neighbours_get_addr( n, sd );

                neighbours_rm( n, sd );

                free( addr );
                close( sd );
        }

	n->head= NULL;
	n->max_sd= 0;
	n->length= 0;

	return TRUE;
}

int neighbours_destroy( neighbours_list_t **n )
{
	neighbours_list_t *n_list= *n;

	neighbours_clear( n_list );

	pthread_mutex_unlock( n_list->lock );
	pthread_mutex_destroy( n_list->lock );
	free( n_list->lock );
	free( n_list );

	*n= NULL;

	return TRUE;
}
