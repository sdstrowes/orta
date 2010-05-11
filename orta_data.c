#include <string.h>
#include "orta_data.h"

/**
 * route_m:
 * 
 */
int route_m( orta_t *orta, data_packet_t* packet )
{
	route_t *route;
	struct sockaddr_in dest;

#ifdef EVAL_RDP
	uint32_t *last_hop, *dist_to_here, *packet_no;
	uint32_t neighbour_sd;
#endif

	/* Sort out sockaddr stuff */
	dest.sin_family= AF_INET;
	dest.sin_port= htons(orta->udp_tx_port);
	memset(&(dest.sin_zero), '\0', 8);

	/* Decrement ttl; if ttl hits zero, return and don't forward */
	if ( !packet->ttl-- )
		return 1;

	/** The following section of code is used for evaluation; it 
	 * helps generate numbers to evaluate the relative delay penalty 
	 * incurred by the overlay. */
#ifdef EVAL_RDP
	pthread_mutex_lock( orta->neighbours->lock );

	last_hop= (uint32_t*)(&(packet->data));
	dist_to_here= last_hop+1;
	packet_no= dist_to_here+1;

	neighbour_sd= neighbours_contains( orta->neighbours, *last_hop );

	*last_hop= orta->local_ip;

	if ( !neighbour_sd ) {
		*dist_to_here= 0;
	}
	else {
		neighbour_t *n= orta->neighbours->head;
		while ( n != NULL && n->sd != neighbour_sd ) {
			n= n->next;
		}

		*dist_to_here+= n->distance;

		printf( "RDP %s ", print_ip(packet->source) );
		printf( "%s %u %u\n", print_ip(orta->local_ip), 
			*packet_no, *dist_to_here );
	}

	pthread_mutex_unlock( orta->neighbours->lock );
#endif
	/** End RDP evaluation section */

	pthread_mutex_lock( orta->route->lock );

	route= orta->route->head;
	while ( route != NULL && route->source != packet->source )
		route= route->next_node;

	while ( route != NULL ) {
		dest.sin_addr.s_addr= route->fwd_link;

#ifdef ORTA_DEBUG
/* 		printf( "Sending %d bytes to %s\n", */
/* 			(packet->datalen)+sizeof(data_header_t), */
/* 			print_ip(route->fwd_link) ); */
/* 		printf( "Sending %d bytes of data to %s\n", */
/* 			(packet->datalen), */
/* 			print_ip(route->fwd_link) ); */
#endif

		if ( sendto( orta->udp_sd, 
			     packet, 
			     (packet->datalen)+sizeof(data_header_t), 
			     0, 
			     (struct sockaddr*)&dest, 
			     sizeof(struct sockaddr) ) <= 0 ) {
			perror( "route_m" );
		}
		route= route->next_link;
	}

	pthread_mutex_unlock( orta->route->lock );

	return 1;
}


/**
 * route:
 * 
 */
int route( orta_t *orta, uint32_t channel, char *buffer, int buflen, int ttl )
{
	char store[PACKET_SIZE];
	data_packet_t *packet= (data_packet_t*)&store;

	/* Sort out actual packet stuff. */
	packet->header.type= data;
	packet->header.channel= channel;
	packet->source= orta->local_ip;
	packet->ttl= ttl;
	packet->datalen= buflen;

	memcpy( &(packet->data), buffer, buflen );

	route_m( orta, packet );

	/* FIXME: Do something more intelligent? */
	return buflen;
}


/**
 * handle_data:
 * 
 */
void handle_data( orta_t *orta, data_packet_t *packet )
{
	uint32_t channel= packet->header.channel;
	queue_t *data_queue;

	/* Packet holder placed into queue */
	packet_holder_t *ph= (packet_holder_t*)malloc(sizeof(packet_holder_t));
	/* packet data placed into packet holder. */
	char *tempdata= (char*)malloc(packet->datalen);

	/* Get the queue for the appropriate data channel */
	pthread_mutex_lock( orta->data_queues->lock );
	list_get( orta->data_queues, channel, (void**)&data_queue );
	pthread_mutex_unlock( orta->data_queues->lock );

	memcpy(tempdata, &packet->data, packet->datalen);

	ph->data= tempdata;
	ph->len= packet->datalen;

	pthread_mutex_lock( data_queue->lock );
	queue_add( data_queue, ph );

	pthread_cond_signal( data_queue->flag );
	pthread_cond_signal( &(orta->data_arrived) );

	pthread_mutex_unlock( data_queue->lock );

	/* Attempt to route the data packet onward */
	route_m( orta, packet );
}
