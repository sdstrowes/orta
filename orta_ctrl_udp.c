#include "orta_ctrl_udp.h"

#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>


#include "common_defs.h"
#include "orta_control_packets.h"
#include "orta_data_packets.h"

#include "linked_list.h"
#include "routing_table.h"
/*#include "dijkstra.h"*/

#include "orta_ctrl_tcp.h"
#include "orta_data.h"

#include "links.h"

/**
 * random_ping:
 * 
 */
void* random_ping( orta_t *orta )
{
	ping_packet_t ping;
	int length= sizeof(ping_packet_t);
	sockaddr_in_t dest;
	dest.sin_family= AF_INET;
	dest.sin_port= htons(orta->udp_tx_port);
	memset(&(dest.sin_zero), '\0', 8);

	ping.header.type= ping_request;
	gettimeofday( &(ping.time), NULL );

	pthread_mutex_lock( orta->members->lock );
	pthread_mutex_lock( orta->neighbours->lock );

	/* Ping a random member of the group if we have members in the
	 * group who are not yet out neighbour.
	 */
	/* Do a -1 on members length to take into account this peer */
	if ( orta->members->length && 
	     (orta->members->length-1) > orta->neighbours->length) {
		int random, i;
		member_t *member;
		struct in_addr a1;

		member= orta->members->head;

		do {
			random= rand()%orta->members->length;
			member= orta->members->head;
			for ( i= 0; i<random; i++ ) {
				member= member->next;
			}
		} while ( member->member == orta->local_ip || 
			  neighbours_contains( orta->neighbours, 
					       member->member ) );

		a1.s_addr= member->member;
#ifdef ORTA_DEBUG
		printf( "random_ping: Pinging %s\n", print_ip(member->member));
#endif


		dest.sin_addr= a1;
		if ( sendto( orta->udp_sd, &ping, length, 0, 
			     (struct sockaddr*)&dest, 
			     sizeof(struct sockaddr) ) <= 0 ) {
			perror( "random_ping" );
		}
	}

	pthread_mutex_unlock( orta->neighbours->lock );
	pthread_mutex_unlock( orta->members->lock );
}


/**
 * Repeatedly pings orta neighbours.
 */
void* pinger( orta_t *orta )
{
	ping_packet_t ping;
	unsigned int key;
	int length= sizeof( ping_packet_t );
	neighbour_t *neighbour;

	sockaddr_in_t dest;
	dest.sin_family= AF_INET;
	dest.sin_port= htons(orta->udp_tx_port);
	memset(&(dest.sin_zero), '\0', 8);


	ping.header.type= ping_request;
	gettimeofday( &(ping.time), NULL );

	pthread_mutex_lock( orta->members->lock );
	pthread_mutex_lock( orta->neighbours->lock );

	for( neighbour= orta->neighbours->head; neighbour != NULL; neighbour= neighbour->next ) {
		/* Set end-point address for ping */
		dest.sin_addr= neighbour->addr->sin_addr;

		if ( sendto( orta->udp_sd, &ping, length, 0, 
			     (struct sockaddr*)&dest, 
			     sizeof(struct sockaddr) ) <= 0 ) {
			perror( "pinger" );
		}
	}

	pthread_mutex_unlock( orta->neighbours->lock );
	pthread_mutex_unlock( orta->members->lock );
}


/**
 * handle_udp_data:
 * 
 */
void* handle_udp_data( void* o )
{
	orta_t *orta= (orta_t*)o;

	int nbytes, i;
	data_packet_header_t *packet= (data_packet_header_t*)malloc(PACKET_SIZE);
	struct sockaddr_in dest;
	int addr_len= sizeof(struct sockaddr);
	uint32_t *temp;

	while( orta->alive ) {
		/* If we have actual data, packet size is 1 or more.*/
		if ((nbytes= recvfrom(orta->udp_sd, packet, PACKET_SIZE, 0, 
				      (sockaddr_t*)&dest, &addr_len )) <= 0) {
			perror( "handle_udp_data" );
			/* FIXME: Look into this; is this what I want? :) */
			continue;
		}

		switch (packet->type) {

		/* Recieved a ping request; send a ping response back. */
		case ping_request: {
			ping_packet_t *ping= (ping_packet_t*)packet;

			ping->header.type= ping_response;

			sendto(orta->udp_sd, ping, sizeof( ping_packet_t ), 0, 
			       (struct sockaddr*)&dest, 
			       sizeof(struct sockaddr));

			break;
		}

		/* Recieved a ping response; calculate amount of time that has 
		   passed, and update pings table as appropriate. */
		case ping_response: {
			ping_packet_t *ping= (ping_packet_t*)packet;
			struct timeval time;
			uint32_t ip_addr= dest.sin_addr.s_addr;
			uint32_t difference;
			neighbour_t *neighbour= orta->neighbours->head;

			gettimeofday( &time, NULL );

			pthread_mutex_lock( orta->neighbours->lock );

			for (neighbour= orta->neighbours->head;
			     neighbour != NULL; 
			     neighbour= neighbour->next) {
				if (neighbour->addr->sin_addr.s_addr == ip_addr)
					break;
			}

			pthread_mutex_unlock( orta->neighbours->lock );

			/* FIXME: Comment */
			/* We've recieved a ping response from a group member 
			 * who is not our neighbour. This is one of our random 
			 * pings; evaluate the usefullness of adding this link.
			 */


			/* FIXME -- is this correct? */
			difference= (((time.tv_sec)-(ping->time.tv_sec))*1000000)+
				((time.tv_usec)-(ping->time.tv_usec));

			if ( difference < MIN_WEIGHT )
				difference= MIN_WEIGHT;

			if (neighbour == NULL) {
#ifdef ORTA_DEBUG
				printf( "handle_udp_data: Random ping from %s\n", print_ip(ip_addr) );
				printf( "time.tv_sec: %u\n", time.tv_sec );
				printf( "ping->time.tv_sec: %u\n", ping->time.tv_sec );
				printf( "time.tv_usec: %u\n", time.tv_usec );
				printf( "ping->time.tv_usec: %u\n", ping->time.tv_usec);
				printf( "difference: %u\n", difference );
#endif
				evaluate_add_link( orta, ip_addr, difference );
			}
			else {
				neighbour_update( neighbour, difference );
			}

			break;
		}
		case data: {
			handle_data( orta, (data_packet_t*)packet );
			break;
		}

		} /* end switch */
	}

	free( packet );
#ifdef ORTA_DEBUG
	printf( "Finished UDP recv'er.\n" );
#endif
	pthread_exit(NULL);
}
