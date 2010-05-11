#include <stdlib.h>
#include <stdio.h>

#include "routing_table.h"
#include "common_defs.h"

int routing_table_init( route_table_t **r )
{
	route_table_t *table= (route_table_t*)malloc(sizeof(route_table_t));

	if ( table ) {
		table->head= NULL;
		table->length= 0;

		table->lock= (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init( table->lock, NULL );

		*r= table;
		return TRUE;
	}
	return FALSE;
}


void routing_table_add( route_table_t *r, uint32_t source, uint32_t fwd_link )
{
 	route_t *route= (route_t*)malloc(sizeof(route_t));
	route_t *temproute, *prevroute;

	route->source=   source;
	route->fwd_link= fwd_link;

	if ( !r->length ) {
		route->next_link= NULL;
		route->next_node= NULL;

		r->length= 1;
		r->head= route;

		return;
	}

	if ( source < (r->head->source) ) {
		route->next_link= NULL;
		route->next_node= r->head;

		r->length++;
		r->head= route;

		return;
	}

	/* We know that the list is at least one element long, so */
	temproute= r->head;
	prevroute= NULL;
	while ( temproute != NULL && source > temproute->source ) {
		prevroute= temproute;
		temproute= temproute->next_node;
	}

	/* Three possibilities:
	   - source == templink->source;
	   - source <  templink->source;
	   - templink == NULL;
	*/
	if ( temproute == NULL ) {
		route->next_node= NULL;
		route->next_link= NULL;

		/* Set all next_node pointers on last item to point to 'link'*/
		for ( temproute= prevroute; temproute != NULL; temproute= temproute->next_link ) {
			temproute->next_node= route;
		}

		r->length++;

		return;
	}
	if ( source == temproute->source ) {
		/* Must scan all entries in this chain to make sure we're not 
		 * duplicating anything. */
		while ( temproute != NULL && fwd_link != temproute->fwd_link ){
			prevroute= temproute;
			temproute= temproute->next_link;
		}

		/* If we already do have this link, update link weight */
		if ( temproute == NULL ) {
			prevroute->next_link= route;
			route->next_node= prevroute->next_node;

			route->next_link= NULL;
			r->length++;
		}

		return;
	}

	if ( source < temproute->source ) {
		route->next_link= NULL;
		route->next_node= temproute;

		/* Set all of previous node to point to 'link', or make head 
		   point to it if there are no previous items...  */
		if ( prevroute != NULL ) {
			for ( temproute= prevroute; temproute != NULL; temproute= temproute->next_link ) {
				temproute->next_node= route;
			}
		}
		else {
			r->head= route;
		}
		r->length++;

		return;
	}

	/* Can we get here?? */
	fprintf( stderr, "routing_table_add: Failed to add \"%u -- %u\"", 
		 source, 
		 fwd_link );
	return;
}

/* FIXME: Comments */
/* Removes the specified link from the table.
 * Return value is the weight being removed, or -1 if the link does not 
 * exist. */
int routing_table_rm( route_table_t *r, uint32_t source, uint32_t fwd_link )
{
	/* prevnode= NULL, or previous list of nodes.
	   prevlink= NULL, or previous link in list of links originating from
	      IP1.
	 */
	route_t *temproute, *prevroute, *prevnode;

	if ( !r->length ) {
		return -1;
	}

	temproute= r->head;
	prevroute= NULL;
	prevnode= NULL;

	while ( temproute != NULL && source != temproute->source ) {
		prevnode= temproute;
		temproute= temproute->next_node;
	}

	/* ip1 not found */
	if ( temproute == NULL ) {
		return -1;
	}
	
	/* Scan through all links from this node to find ip2 */
	while ( temproute != NULL && fwd_link != temproute->fwd_link ) {
		prevroute= temproute;
		temproute= temproute->next_link;
	}
	/* ip2 not found */
	if ( temproute == NULL ) {
		return -1;
	}

	/* Found the link. Fix pointers and return the weight. */
	if ( prevroute == NULL ) {
		if ( prevroute == NULL ) {
			/* Reset head */
			if ( temproute->next_link != NULL ) {
				r->head= temproute->next_link;
			}
			else {
				r->head= temproute->next_node;
			}
		}
		else {
			/* Reset next_node on all previous node's 
			   outgoing links */
			while ( prevnode != NULL ) {
				if ( temproute->next_link != NULL ) {
					prevnode->next_node= temproute->next_link;
				}
				else {
					prevnode->next_node= temproute->next_node;
				}

				prevnode= prevnode->next_link;
			}
		}
	}
	else {
		prevroute->next_link= temproute->next_link;
	}

	r->length--;
	free(temproute);
	return 1;
}

int routing_table_clear( route_table_t *r )
{
	route_t *tmp_route;

	while ( r->length ) {
		tmp_route= r->head;

		routing_table_rm(r, tmp_route->source,tmp_route->fwd_link);
	}

}

/* Frees the data structures without giving up the lock. */
int routing_table_free( route_table_t **r )
{
	route_table_t *route= *r;

	routing_table_clear( route );

	free( route );

	*r= NULL;

	return TRUE;
}

int  routing_table_destroy( route_table_t **r )
{
	route_table_t *route= *r;

	routing_table_clear( route );

	pthread_mutex_unlock( route->lock );
	pthread_mutex_destroy( route->lock );
	free( route );

	*r= NULL;

	return TRUE;
}
