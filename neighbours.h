
#ifndef __NEIGHBOURS_LIST_
#define __NEIGHBOURS_LIST_

#include <stdint.h>
#include <pthread.h>
#include <arpa/inet.h> /* Struct sockaddr_in */


/* For smoothing values, how much emphasis is put on older values. (0..1) */
#define PREV_WEIGHT 0.95

struct _neighbour_t
{
	uint32_t sd;
	struct sockaddr_in *addr;
	/* distance measures the distance between this host and the host 
	 * at the other end of this link. This value should be smoothed 
	 * to help prevent flooding the network with state changes. */
	uint32_t distance;
	/* Number of refresh cycles since information on this link was sent */
	uint16_t last_sent;
	struct _neighbour_t *next;
};

struct _neighbours_list_t
{
	struct _neighbour_t *head;
	uint32_t max_sd;
	uint32_t length;
	pthread_mutex_t *lock;
};

typedef struct _neighbour_t neighbour_t;
typedef struct _neighbours_list_t neighbours_list_t;


/**
 * neighbours_init:
 * Initiates the neighbours list and makes `list' point to it. If the init 
 * fails, this function returns FALSE, but TRUE otherwise.
 */
int neighbours_init( neighbours_list_t **list );


/**
 * neighbours_add:
 * Adds the neighbour connected to through `sd' and described by `addr' with 
 * a distance of `distance' to the neighbours list.
 */
int neighbours_add( neighbours_list_t *list, 
		    uint32_t sd, 
		    struct sockaddr_in *addr );

/**
 * neighbour_update:
 * 
 */
int neighbour_update( neighbour_t *n, 
		      uint32_t distance );

/**
 * neighbours_get_nbr:
 * Retrieves the neighbour_t which is held for `sd'. Returns NULL if that 
 * socket descriptor is not held here.
 */
neighbour_t *neighbours_get_nbr( neighbours_list_t *list, uint32_t sd );

/**
 * neighbours_get_addr:
 * Retrieves the (struct sackaddr_in*) for the associated socket descriptor, or
 * NULL if that socket descriptor does not exist.
 */
struct sockaddr_in *neighbours_get_addr( neighbours_list_t *list, 
					 uint32_t sd );


/**
 * neighbours_rm:
 * Removes the neighbour pointed to by `sd' and returns the associated
 * (struct sackaddr_in*) for that socket descriptor, or NULL if that socket
 * descriptor does not point to a known neighbour.
 */
struct sockaddr_in * neighbours_rm( neighbours_list_t *list, 
				    uint32_t sd );

/**
 * neighbours_max_sd:
 * Returns the maximum known socket descriptor for any neighbour; if the list 
 * is empty, returns 0.
 */
int neighbours_max_sd( neighbours_list_t *list );


/**
 * neighbours_contains:
 * Searches for the given IP address in the neighbours list, returning the sd  
 * for that neighbour if it is found, FALSE otherwise.
 */
int neighbours_contains( neighbours_list_t *list, 
			 uint32_t neighbour );

int neighbours_clear( neighbours_list_t *n );

int neighbours_destroy( neighbours_list_t **n );


#endif
