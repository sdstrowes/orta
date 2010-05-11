
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dijkstra.h"
#include "links.h"

#include "fifo_queue.h"
#include "ordered_queue.h"

#include "common_defs.h"


#ifdef ORTA_DEBUG
void dijkstra_print_links( links_t *links )
{
	link_from_t *node;
	link_to_t *link;

	printf( "---- start print_links (length: %d) --\n", links->length );
	printf( "----- FROM -----+----  TO  -----+------ Weight ---+\n" );

	for ( node= links->head; node != NULL; node= node->next_node ) {
		for (link= node->links; link != NULL; link= link->next_link) {
			printf( "---- \t%s\t|\t",
				print_ip(node->ip) );
			printf( "%s\t|\t%d\n",
				print_ip(link->ip),
				link->distance );
		}
	}

	printf( "---- end print_links (length: %d) --\n", links->length );
}


void dijkstra_print_links_list( linked_list_t *links )
{
	list_item_t *link;

	printf( "---- start print_links (length: %d) --\n", links->length );
	printf( "----  TO  -----+------ Weight ---+\n" );

	for ( link= links->head; link != NULL; link= link->next ) {
		uint32_t temp= *((uint32_t*)link->data);
		if ( temp == INFINITY )
			printf( "%s\t|\tINF\n",
				print_ip(link->key) );
		else
			printf( "%s\t|\t%d\n",
				print_ip(link->key),
				temp );
	}

	printf( "---- end print_links (length: %d) --\n", links->length );
}

#endif


/**
 * Initialises the ordered queue such that it is ready for the shortest path 
 * algorithm to be run using it by placing all known nodes into the queue with 
 * some large weight, as well as marking thenode to commence the algorithm 
 * from by adding it with weight zero, such that it appears at the front of 
 * the queue.
 */
static void d_queue_init( queue_t *queue, links_t *links, uint32_t start_node )
{
	link_from_t *tmp_node;
	link_to_t   *tmp_link;

	for ( tmp_node= links->head; tmp_node != NULL; tmp_node= tmp_node->next_node ) {
		for ( tmp_link= tmp_node->links; tmp_link != NULL; tmp_link= tmp_link->next_link ) {
			/*if ( d_queue_contains( queue, tmp_link->ip ) == -1 ) {*/
			d_queue_add( queue, 
				     tmp_node->ip, 
				     tmp_link->ip, 
				     INFINITY );
			/*}*/
		}
	}

	d_queue_add( queue, start_node, 0, 0 );
}


/**
 * Performs Dijkstra's shortest path algorithm over the set of links provided.
 * start_node should probably be local IP, but could be anything.
 */
void shortest_path_graph( uint32_t start_node, links_t *links, links_t *out )
{
	queue_t *queue;
	link_from_t *node;
	link_to_t   *link;

	queue_init( &queue );

	/* Add all nodes to the queue.
	 * All nodes apart from the starting node will be added with some large
	 * value, starting node will be entered with value 0.
	 */
	d_queue_init( queue, links, start_node );

	while ( queue->length ) {
		uint32_t z_distance;
		queue_data_t *data= queue_dequeue( queue );

		/* Grab the list of neighbours for this IP */
		for (node= links->head; node != NULL; node= node->next_node) {
 			if (node->ip == data->ip)
				break;
		}

		if ( node == NULL ) {
			return;
		}

		/* For each vertex adjacent to u, such that it is in 'queue' */
		for (link= node->links; link != NULL; link= link->next_link ) {
			if ( (z_distance= d_queue_contains( queue, link->ip )) != -1 ) {
				if (data->distance+link->distance< z_distance){
					z_distance= data->distance+link->distance;
					d_queue_add( queue, link->ip, node->ip, z_distance  );
				}
			}
		}

		/* Add link to output graph */
		links_add( out, data->ip2, data->ip );
		link_update( out, data->ip2, data->ip, data->distance );
	}

	queue_destroy( &queue );
}



/**
 * Performs Dijkstra's shortest path algorithm over the set of links provided.
 * start_node should probably be local IP, but could be anything.
 */
void shortest_paths( uint32_t start_node, links_t *links, linked_list_t *out )
{
	queue_t *queue;

	link_from_t *node;
	link_to_t   *link;
	uint32_t *outvar;

	queue_init( &queue );

	/* Add all nodes to the queue.
	 * All nodes apart from the starting node will be added with some large
	 * value, starting node will be entered with value 0.
	 */
	d_queue_init( queue, links, start_node );


	while ( queue->length ) {
		uint32_t z_distance;
		queue_data_t *data= queue_dequeue( queue );

		/* Grab the list of neighbours for this IP */
		for (node= links->head; node != NULL; node= node->next_node) {
 			if (node->ip == data->ip)
				break;
		}

		if ( node == NULL ) {
			outvar= (uint32_t*)malloc(sizeof(uint32_t));
			*outvar= INFINITY;
			list_add( out, data->ip, outvar );
		}

		else {
			/* For each vertex adjacent to u, such that it is in 'queue' */
			for (link= node->links; link != NULL; link= link->next_link ) {
				z_distance= d_queue_contains( queue, link->ip );
				if ( z_distance != -1 ) {
					if (data->distance+link->distance< z_distance){
						z_distance= data->distance+link->distance;
						d_queue_add( queue, link->ip, node->ip, z_distance  );
					}
				}
			}

			outvar= (uint32_t*)malloc(sizeof(uint32_t));
			*outvar= data->distance;
			list_add( out, data->ip, outvar );
		}
	}

	queue_destroy( &queue );
}


void link_add( links_t *g, uint32_t from, uint32_t to, uint32_t weight )
{
	links_add( g, from, to );
	link_update( g, from, to, weight );
}

/* int main( int argc, char** argv ) */
/* { */
/* 	int i; */
/* 	links_t *graph; */
/* 	links_t *output; */

/* 	links_init( &graph ); */
/* 	links_init( &output ); */

/* 	shortest_path_graph( 7, graph, output ); */
/* 	dijkstra_print_links( output ); */

/* 	return 1; */

/* } */

	/*links_init( &graph );

	link_add( graph, 1, 2, 2704 );
	link_add( graph, 1, 3, 1846 );
	link_add( graph, 1, 4, 1464 );
	link_add( graph, 1, 5, 337 );
	link_add( graph, 2, 1, 2704 );
	link_add( graph, 2, 3, 867 );
	link_add( graph, 2, 6, 1258 );
	link_add( graph, 2, 8, 187 );
	link_add( graph, 3, 1, 1846 );
	link_add( graph, 3, 2, 867 );
	link_add( graph, 3, 9, 849 );
	link_add( graph, 3, 8, 740 );
	link_add( graph, 3, 7, 621 );
	link_add( graph, 3, 4, 802 );
	link_add( graph, 4, 1, 1464 );
	link_add( graph, 4, 3, 802 );
	link_add( graph, 4, 8, 1391 );
	link_add( graph, 4, 6, 1121 );
	link_add( graph, 4, 5, 1235 );
	link_add( graph, 5, 1, 337 );
	link_add( graph, 5, 4, 1235 );
	link_add( graph, 5, 6, 2342 );
	link_add( graph, 6, 5, 2342 );
	link_add( graph, 6, 4, 1121 );
	link_add( graph, 6, 7, 946 );
	link_add( graph, 6, 8, 1050 );
	link_add( graph, 6, 2, 1258 );
	link_add( graph, 7, 6, 946 );
	link_add( graph, 7, 3, 621 );
	link_add( graph, 7, 8, 184 );
	link_add( graph, 8, 7, 184 );
	link_add( graph, 8, 4, 1391 );
	link_add( graph, 8, 6, 1090 );
	link_add( graph, 8, 2, 187 );
	link_add( graph, 8, 9, 144 );
	link_add( graph, 9, 8, 144 );
	link_add( graph, 9, 3, 849 );

	dijkstra_print_links( graph );


/* 	links_rm( graph, 1, 2 ); */
/* 	print_links( graph ); */
/* 	links_rm( graph, 1, 3 ); */
/* 	print_links( graph ); */
/* 	links_rm( graph, 1, 4 ); */
/* 	print_links( graph ); */
/* 	links_rm( graph, 1, 5 ); */
/* 	print_links( graph ); */
/* 	links_rm( graph, 2, 1 ); */


/* 	print_links( graph ); */

/* 	links_rm( graph, 3, 9 ); */
/* 	links_rm( graph, 3, 8 ); */
/* 	links_rm( graph, 3, 7 ); */
/* 	links_rm( graph, 3, 4 ); */
/* 	links_rm( graph, 4, 1 ); */
/* 	links_rm( graph, 4, 3 ); */

/* /\\* 	print_links( graph ); *\\/ */

/* 	links_rm( graph, 4, 8 ); */
/* 	links_rm( graph, 4, 6 ); */
/* 	links_rm( graph, 4, 5 ); */
/* 	links_rm( graph, 5, 1 ); */
/* 	links_rm( graph, 5, 4 ); */
/* 	links_rm( graph, 5, 6 ); */

/* /\\* 	print_links( graph ); *\\/ */

/* 	links_rm( graph, 8, 9 ); */
/* 	links_rm( graph, 9, 8 ); */
/* 	links_rm( graph, 9, 3 ); */

/* /\\* 	print_links( graph ); *\\/ */

/* 	links_rm( graph, 6, 5 ); */
/* 	links_rm( graph, 6, 4 ); */
/* 	links_rm( graph, 6, 7 ); */
/* 	links_rm( graph, 6, 8 ); */
/* 	links_rm( graph, 6, 2 ); */
/* 	links_rm( graph, 7, 6 ); */

/* /\\* 	print_links( graph ); *\\/ */

/* 	links_rm( graph, 2, 3 ); */
/* 	links_rm( graph, 2, 6 ); */
/* 	links_rm( graph, 2, 8 ); */
/* 	links_rm( graph, 3, 1 ); */
/* 	links_rm( graph, 3, 2 ); */

/* /\\* 	print_links( graph ); *\\/ */

/* 	links_rm( graph, 7, 3 ); */
/* 	links_rm( graph, 7, 8 ); */
/* 	links_rm( graph, 8, 7 ); */
/* 	links_rm( graph, 8, 4 ); */
/* 	links_rm( graph, 8, 6 ); */
/* 	links_rm( graph, 8, 2 ); */



/*
	printf( "About to run with:\n" );

#ifdef ORTA_DEBUG
	dijkstra_print_links( graph );
#endif

	for ( i= 1; i<=9; i++ ) {
		printf( "### Output from %d.\n", i );
		dijkstra_print_links( shortest_path( i, graph ) );
		printf( "\n" );
	}
	printf( "### Output from %d.\n", 7 );
#ifdef ORTA_DEBUG
	dijkstra_print_links( shortest_path( 7, graph ) );
	printf( "\n" );
#endif

}
*/
