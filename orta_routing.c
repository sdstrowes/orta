
#include "orta_routing.h"
#include "members.h"
#include "links.h"
/* #include "dijkstra.h" */

#include "orta_t.h"

/**
 *
 */
int routing_add_route( route_table_t *routes, 
		       uint32_t source, 
		       uint32_t outbound_link )
{
	return 0;
}


/**
 * Removes a given link from 
 */
int routing_drop_route( route_table_t *routes, 
			uint32_t source, 
			uint32_t outbound_link )
{
	return 0;
}


/**
 * Constructs a routing table from the information found in "links"
 */
int routing_build_table( orta_t *o )
{
	member_t    *member;
	route_t     *route;
	links_t     *shortest_paths;

	link_from_t *tempnode;
	link_to_t   *templink;

	route_table_t *r;
	route_table_t *tmp_r;

	if ( !routing_table_init( &r ) )
		return FALSE;

	links_init( &shortest_paths );

	/* Calculate shortest path spanning tree from each source, then store 
	 * the links in the tree rooted at that source, but only links which 
	 * move away from the source (such that information is not forwarded 
	 * back up the tree). */
	for(member= o->members->head; member != NULL; member= member->next){

		shortest_path_graph(member->member, o->links, shortest_paths);

 		tempnode= shortest_paths->head;
 		while (tempnode != NULL && tempnode->ip != o->local_ip) {
			tempnode= tempnode->next_node;
 		}

		if ( tempnode != NULL ) {
			templink= tempnode->links;
			while (templink != NULL) {
				routing_table_add( r, 
						   member->member, 
						   templink->ip );
				templink= templink->next_link;
			}
		}

		links_clear( shortest_paths );
	}

	pthread_mutex_lock( o->route->lock );

	/* Destroy the lock that was created for the new routing table.
	 * Replace it with the lock from the old routing table.
	 * Anybody holding onto the lock shant be dissappointed this way. */
	pthread_mutex_destroy( r->lock );
	free( r->lock );

	tmp_r= o->route;
	o->route= r;
	/* Pass on the lock */
	o->route->lock= tmp_r->lock;

	pthread_mutex_unlock( o->route->lock );

	/* Destroy temporary links table */
	links_destroy( &shortest_paths );
	/* Destroy old route table, but not its mutex. */
	routing_table_free( &tmp_r );

	return TRUE;
}
