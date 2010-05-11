#include "common_defs.h"
#include "links.h"

#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/**
 * links_init:
 * Initiates the adjacency list of links and makes `links' point to it. If the 
 * init fails, this function returns FALSE, but TRUE otherwise.
 */
int links_init( links_t **links )
{
	links_t *l= (links_t*)malloc(sizeof(links_t));

	if ( l ) {
		l->head= NULL;
		l->length= 0;

		l->lock= (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init( l->lock, NULL );

		*links= l;
		return TRUE;
	}
	return FALSE;
}


/**
 * links_clear:
 * 
 * Clears the list, returning an empty list.
 */
int links_clear( links_t *links )
{
	link_from_t *from, *temp_from;
	link_to_t *to, *temp_to;

	for ( from= links->head; from != NULL; ) {
		for ( to= from->links; to != NULL; ) {
			temp_to= to;
			to= to->next_link;
			free( temp_to );
		}
		temp_from= from;
		from= from->next_node;
		free( temp_from );
	}

	links->length= 0;
	links->head= NULL;

	return TRUE;
}


/**
 * links_destroy:
 * 
 * Destroys the links table structure.
 */
int links_destroy( links_t **links )
{
	links_t *l= *links;
	link_from_t *from, *temp_from;
	link_to_t *to, *temp_to;

	links_clear( l );
	pthread_mutex_unlock( l->lock );
	pthread_mutex_destroy( l->lock );

	free( l->lock );
	free( l );

	*links= NULL;

	return TRUE;
}


/**
 * links_add:
 * 
 * Adds the link `from'-->`to' to the adjacency list with a distance
 * of `distance'. Will not add the link in the situation that the link
 * already exists.
 */
int links_add( links_t *l, uint32_t from_ip, uint32_t to_ip )
{
	link_from_t *from, *prev;
	link_to_t *to, *prev_to;

	from= l->head;
	prev= NULL;

	/* Find the correct position for from_ip */
	while ( from != NULL && from_ip > from->ip ) {
		prev= from;
		from= from->next_node;
	}

	/* "from" hasn't been found, add a new node. */
	if ( from == NULL ) {
		to= (link_to_t*)malloc(sizeof(link_to_t));
		to->ip= to_ip;
		to->distance= DEFAULT_DIST;
		to->next_link= NULL;

		from= (link_from_t*)malloc(sizeof(link_from_t));
		from->ip= from_ip;
		from->links= to;
		from->next_node= NULL;

		/* Determine whether to add to head or middle of list */
		if ( prev == NULL ) 
			l->head= from;
		else
			prev->next_node= from;

		l->length++;
		return TRUE;
	}

	/* If "from" wasn't NULL, we're either inserting into the middle of 
	 * the list of nodes, or we already know of "from" */
	if ( from_ip < from->ip ) {
		link_from_t *tmp= from;

		to= (link_to_t*)malloc(sizeof(link_to_t));
		to->ip= to_ip;
		to->distance= DEFAULT_DIST;
		to->next_link= NULL;

		from= (link_from_t*)malloc(sizeof(link_from_t));
		from->ip= from_ip;
		from->links= to;
		from->next_node= tmp;

		if ( prev != NULL )
			prev->next_node= from;
		else
			l->head= from;

		l->length++;
		return TRUE;
	}


	/* We already know of "from" ... try to find "to" */
	if ( from->ip == from_ip ) {
		to= from->links;
		prev_to= NULL;
		while ( to != NULL && to->ip != to_ip ) {
			prev_to= to;
			to= to->next_link;
		}

		/* "to" hasn't been found, add a new link. */
		if ( to == NULL ) {
			to= (link_to_t*)malloc(sizeof(link_to_t));
			to->ip= to_ip;
			to->distance= DEFAULT_DIST;
			to->next_link= NULL;

			prev_to->next_link= to;

			l->length++;


			return TRUE;
		}
		else
			return FALSE;
	}

	return TRUE;
}

/**
 * link_update:
 * 
 * Updates the reported distance over a link between two hosts, by
 * taking into account the new value, and updating the output if this
 * input has varied by some reasonable amount (specified by
 * "VARIATION").
 */
int link_update( links_t *l, uint32_t from_ip, uint32_t to_ip, uint32_t dist )
{
	link_from_t *from;
	link_to_t *to;

	from= l->head;
	while ( from != NULL && from->ip != from_ip )
		from= from->next_node;

	/* If the link doesn't exist, add it, and set its weight */
	if ( from == NULL && links_add( l, from_ip, to_ip ) )
		return link_update( l, from_ip, to_ip, dist );

	to= from->links;
	while ( to!= NULL && to->ip != to_ip )
		to= to->next_link;

	/* If the link doesn't exist, add it, and set its weight */
	if ( to == NULL && links_add( l, from_ip, to_ip ) )
		return link_update( l, from_ip, to_ip, dist );

	/* Link exists. But if we already have this weight, we're not 
	 * changing any state. Return FALSE. */
	if ( to->distance == dist )
		return FALSE;

	to->distance= dist;
	return TRUE;
}


/**
 * links_rm:
 * 
 * Removes the link `from'-->`to' from the adjacency list with a
 * distance of `distance', returning the weight of that link if it
 * exists, and -1 otherwise.
 */
int links_rm( links_t *l, uint32_t from_ip, uint32_t to_ip )
{
	link_from_t *from, *prev_from;
	link_to_t *to, *prev_to;
	int temp_dist= -1;

	from= l->head;
	prev_from= NULL;
	while ( from != NULL && from_ip != from->ip ) {
		prev_from= from;
		from= from->next_node;
	}

	/* Not found! */
	if ( from == NULL )
		return temp_dist;

	/* 'from' found, find 'to' */
	to= from->links;
	prev_to= NULL;
	while ( to != NULL && to_ip != to->ip ) {
		prev_to= to;
		to= to->next_link;
	}

	/* Not found! */
	if ( to == NULL )
		return temp_dist;

	/* Found. Fix pointers correctly, and return weight of link */
	if ( prev_from == NULL && prev_to == NULL ) {
		temp_dist= to->distance;

		if ( to->next_link == NULL ) {
			l->head= from->next_node;
			free( from );
		}
		else
			from->links= to->next_link;

		free( to );
		l->length--;
	}
	else if ( prev_to == NULL ) {
		temp_dist= to->distance;

		if ( to->next_link == NULL ) {
			prev_from->next_node= from->next_node;
			free( from );
		}
		else
			from->links= to->next_link;

		free( to );
		l->length--;
	}
	else {
		prev_to->next_link= to->next_link;
		temp_dist= to->distance;
		free( to );
		l->length--;
	}

	return temp_dist;
}

/**
 * link_distance_to:
 * 
 * Finds and returns the distance over the link from the local host to
 * the host indicated.  If there is no direct link, INFINITY is returned.
 */
int link_distance_to( linked_list_t *l, uint32_t to_ip )
{
	list_item_t* link;

	for ( link= l->head; link != NULL; link= link->next ) {
		if ( link->key == to_ip )
			break;
	}

	if ( link == NULL )
		return INFINITY;

	return *((uint32_t*)link->data);
}


/**
 * links_distance:
 * 
 * Returns the weight of link `from'-->`to' if that link exists, and
 * INFINITY otherwise.
 */
int link_distance( links_t *l, uint32_t from_ip, uint32_t to_ip )
{
	link_from_t *from;
	link_to_t *to;

	from= l->head;

	while ( from != NULL && from_ip != from->ip ) {
		from= from->next_node;
	}

	/* `from' not found, link doesn't exist */
	if ( from == NULL )
		return INFINITY;

	/* `from' found; try to find `to' */
	to= from->links;

	/* Scan through all links from this node to find `to' */
	while ( to != NULL && to_ip != to->ip ) {
		to= to->next_link;
	}
	if ( to == NULL )
		return INFINITY;

	return to->distance;
}


/**
 * links_from:
 * 
 * Returns the links of outbound links from the given `ip', or NULL if
 * no such set of links exists.
 */
link_to_t *links_from( links_t *links, uint32_t ip )
{
	link_from_t *from= links->head;

	while ( from != NULL && from->ip != ip )
		from= from->next_node;

	if ( from != NULL )
		return from->links;

	return NULL;
}


/**
 * num_links_from:
 * 
 * Returns the number of links from the given IP address. If that
 * address is not found, zero is returned, as would be expected (there
 * are obviously not any links from it if it's not there).
 */
int num_links_from( links_t *links, uint32_t ip )
{
	int count= 0;
	link_to_t *l= links_from( links, ip );

	while ( l != NULL ) {
		l= l->next_link;
		count++;
	}

	return count;
}

/* int main( int argc, char** argv ) */
/* { */
/* 	links_t *links; */
/* 	links_init( &links ); */

/* 	links_add( links, 5, 6, 2342 ); */
/* 	links_add( links, 6, 5, 2342 ); */
/* 	links_add( links, 6, 4, 1121 ); */
/* 	links_add( links, 6, 7, 946 ); */
/* 	links_add( links, 6, 8, 1050 ); */
/* 	links_add( links, 6, 2, 1258 ); */
/* 	links_add( links, 7, 6, 946 ); */
/* 	links_add( links, 7, 3, 621 ); */
/* 	links_add( links, 7, 8, 184 ); */
/* 	links_add( links, 8, 7, 184 ); */
/* 	links_add( links, 8, 4, 1391 ); */
/* 	links_add( links, 8, 6, 1090 ); */
/* 	links_add( links, 8, 2, 187 ); */
/* 	links_add( links, 8, 9, 144 ); */
/* 	links_add( links, 9, 8, 144 ); */
/* 	links_add( links, 9, 3, 849 ); */
/* 	links_add( links, 1, 2, 2704 ); */
/* 	links_add( links, 1, 3, 1846 ); */
/* 	links_add( links, 1, 4, 1464 ); */
/* 	links_add( links, 1, 5, 337 ); */
/* 	links_add( links, 2, 1, 2704 ); */
/* 	links_add( links, 2, 3, 867 ); */
/* 	links_add( links, 2, 6, 1258 ); */
/* 	links_add( links, 2, 8, 187 ); */
/* 	links_add( links, 3, 1, 1846 ); */
/* 	links_add( links, 3, 2, 867 ); */
/* 	links_add( links, 3, 9, 849 ); */
/* 	links_add( links, 3, 8, 740 ); */
/* 	links_add( links, 3, 7, 621 ); */
/* 	links_add( links, 3, 4, 802 ); */
/* 	links_add( links, 4, 1, 1464 ); */
/* 	links_add( links, 4, 3, 802 ); */
/* 	links_add( links, 4, 8, 1391 ); */
/* 	links_add( links, 4, 6, 1121 ); */
/* 	links_add( links, 4, 5, 1235 ); */
/* 	links_add( links, 5, 1, 337 ); */
/* 	links_add( links, 5, 4, 1235 ); */

/* 	print_links( links ); */

/* 	links_rm( links, 3, 1 ); */
/* 	links_rm( links, 9, 3 ); */
/* 	links_rm( links, 6, 8 ); */
/* 	links_rm( links, 6, 2 ); */
/* 	links_rm( links, 1, 2 ); */
/* 	links_rm( links, 6, 2 ); */
/* 	links_rm( links, 7, 6 ); */

/* 	print_links( links ); */

/* 	links_purge( links ); */

/* 	print_links( links ); */


/* 	return 1; */
/* } */
