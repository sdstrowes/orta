
#ifndef __LINKS_TABLE_
#define __LINKS_TABLE_

#include <stdint.h>
#include <pthread.h>

#include "orta.h"
#include "fifo_queue.h"
#include "linked_list.h"

#define TRUE 1
#define FALSE 0

typedef struct _link_to_t {
	uint32_t ip;
	/* Reported value as far as routing code is concerned */
	uint32_t distance;
	struct _link_to_t *next_link;
} link_to_t;

typedef struct _link_from_t {
	uint32_t ip;
	struct _link_from_t *next_node;
	struct _link_to_t   *links;
} link_from_t;


typedef struct {
	link_from_t *head;
	uint32_t length;
	pthread_mutex_t *lock;
} links_t;


/**
 * links_init:
 * Initiates the adjacency list of links and makes `links' point to it. If the 
 * init fails, this function returns FALSE, but TRUE otherwise.
 */
int links_init( links_t **links );

/**
 * links_destroy:
 */
int links_destroy( links_t **links );

/**
 * links_clear:
 */
int links_clear( links_t *links );

/**
 * links_add:
 * Adds the link `from'-->`to' to the adjacency list with a distance of 
 * `distance'. Will not add the link in the situation that the link already 
 * exists, or has previously existed and is now marked as a dead link.
 */
int links_add( links_t *l, uint32_t from_ip, uint32_t to_ip );


/**
 * links_add:
 * Adds the link `from'-->`to' to the adjacency list with a distance of 
 * `distance'.
 */
int link_update(links_t *l, uint32_t from_ip, uint32_t to_ip, uint32_t dist);

/**
 * links_rm:
 * Removes the link `from'-->`to' from the adjacency list with a distance of 
 * `distance', returning the weight of that link if it exists, and -1 
 * otherwise.
 */
int links_rm( links_t *l, uint32_t from_ip, uint32_t to_ip );


/**
 * link_distance_to:
 * Finds and returns the distance over the link from the local host to the 
 * host indicated.
 * If there is no direct link, -1 is returned.
 */
int link_distance_to( linked_list_t *l, uint32_t to_ip );

/**
 * links_distance:
 * Returns the weight of link `from'-->`to' if that link exists, and -1 
 * otherwise.
 */
int link_distance( links_t *l, uint32_t from_ip, uint32_t to_ip );


/**
 * Returns the links of outbound links from the given `ip', or NULL if no such 
 * set of links exists.
 */
link_to_t *links_from( links_t *l, uint32_t ip );

/**
 * num_links_from:
 * Returns the number of links from the given IP address. If that address 
 * is not found, zero is returned, as would be expected (there are obviously 
 * not any links from it if it's not there).
 */
int num_links_from( links_t *links, uint32_t ip );

#endif

