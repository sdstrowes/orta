#include "common_defs.h"

#include "orta_debug.h"
#include "routing_table.h"
#include "members.h"

#include "linked_list.h"

#include <netdb.h>
#include <stdio.h>
#include <netinet/in.h>  /* For: inet_ntoa */

char* print_ip( uint32_t ip )
{
	struct in_addr a;
	a.s_addr= ip;
	return inet_ntoa(a);
}


void debug_packet( void* packet, int packet_length )
{
	int i; /* debug var */
	uint32_t *debug;

	debug= (uint32_t*)packet;
	printf( "\n" );
	for ( i= 0; i<packet_length; ) {
		printf( "PACKET (%03d): ", i );
		printf( "%3u ", ((*debug)&0x000000ff) );
		i++;
		if ( i < packet_length )
			printf( "%3u ", ((*debug)&0x0000ff00)>>8 );
		i++;
		if ( i < packet_length )
			printf( "%3u ", ((*debug)&0x00ff0000)>>16 );
		i++;
		if ( i < packet_length )
			printf( "%3u ", ((*debug)&0xff000000)>>24 );
		i++;
		printf( "\t(%u)", *debug );
		printf( "\n" );
		debug++;
	}
	printf( "\n" );
}

static void print_routes( orta_t *orta )
{
	route_t *node;
	route_t *route;

	printf( "---- ROUTING TABLE: (length: %d) --\n", 
		orta->route->length );
	printf( "---- SOURCE ---------+-------- FWD TO ---------+\n" );

	for ( node= orta->route->head; node != NULL; ) {
		for ( route= node; route != NULL; route= route->next_link ) {
			printf( "%s\t|\t", print_ip(route->source) );
			printf( "%s\t|\n", print_ip(route->fwd_link) );

			node= route->next_node;
		}
	}
}

void print_links( orta_t *orta )
{
	link_from_t *from;
	link_to_t   *to;

	printf( "---- LINKS: (length: %d) --\n", orta->links->length );
	printf("---- FROM ------+--------  TO  --------+--- Distance ---+\n" );

	for ( from= orta->links->head; from != NULL; from= from->next_node) {
		for ( to= from->links; to != NULL; to= to->next_link ) {
			printf( "%s\t|\t", print_ip(from->ip) );
			printf("%s\t|\t%d\n", print_ip(to->ip), to->distance);
		}
	}
}


static void print_neighbours( orta_t * orta )
{
	neighbour_t *neighbour;

	neighbour= orta->neighbours->head;

	printf( "---- NEIGHBOURS: (length: %d) --\n", 
		orta->neighbours->length );
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


static void print_group_members( orta_t * orta )
{
	member_t *member;

	printf( "---- GROUP MEMBERS: (length: %d) --\n", 
		orta->members->length );
	printf( "----- IP Addr -----+----- Seq -----+--- Timestamp ---+\n" );
	for ( member= orta->members->head; member != NULL; member= member->next ) {
		printf( "%s\t|\t%u\t|\t%u.%u\n", 
			print_ip(member->member),
			member->seq,
			member->tv.tv_sec,
			member->tv.tv_usec);
	}
}


void print_state( orta_t *orta )
{
	printf("-------------------------------------------------\n");
	printf("print_state: Current local state at %s is:\n", 
	       print_ip( orta->local_ip ) );
	print_group_members( orta );
	printf("\n");fflush(stdout);
	print_neighbours( orta );
	printf("\n");fflush(stdout);
	print_links( orta );
	printf("\n");fflush(stdout);
	print_routes( orta );
	printf("-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-\n");
	fflush(stdout);
	printf( "print_state: Sleeping.\n" );
}
