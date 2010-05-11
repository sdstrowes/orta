#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>

#include "orta.h"
#include "orta_ctrl_tcp.h"


#include "common_defs.h"

#include "orta_control_packets.h"
#include "linked_list.h"
#include "orta_routing.h"
#include "netTCP.h"


/* OFFSET_FRACTION is the distance from the advertised value the
 * current known weight of a link must be before it is definitely sent
 * in a refresh packet.*/
#define OFFSET_FRACTION 0.1

/* REFRESH_CYCLES_FOR_SEND is the number of times the refresh
 * dispatcher will run before information on a link is guaranteed to
 * be sent to the rest of the group. */
#define REFRESH_CYCLES_FOR_SEND 5

/* Used to store a list of members to pass back to the application */
static int* members_array= NULL;


/**
 * update_membership_for_app:
 *
 * Compiles an array of IP addresses (in 32bit int format) for the
 * application, if and only if the app has registered a callback for
 * this functionality to take place.
 */
void update_membership_for_app( orta_t *o )
{
	if ( o->update_membership != NULL ) {
		int num_members= members_length(o->members);

		if ( members_array != NULL ) {
			free( members_array );
		}

		members_array= (int*)malloc(sizeof(int)*num_members );
		members_ip_nums( o->members, members_array );

		o->update_membership(members_array, num_members);
	}
}


/**
 * flood:
 * 
 * Sends given control data to all group neighbours. If we have no
 * neighbours, no packets will be sent. Flood increments the outgoing
 * sequence count for this node.
 */
static void flood( orta_t *o, void* packet, uint32_t packet_length )
{
	neighbour_t *nbr;
	control_packet_header_t *header= packet;

	assert( header->type >= 0 && header->type <= 13 );

	/* Send this data to all neighbours */
	for( nbr= o->neighbours->head; nbr != NULL; nbr= nbr->next ) {
		send( nbr->sd, packet, packet_length, 0 );
	}
}

/**
 * fwd_flood:
 * 
 * Sends given control data to all group neighbours, apart from the
 * neighbour identified by the given socket descriptor, `sd'. If we
 * have no further neighbours, no packets will be * forwarded.
 */
static void fwd_flood(orta_t *o, void *packet, uint32_t pkt_size, uint32_t sd)
{
	neighbour_t *n;
	int i; /* debug var */
	char* debug;
	control_packet_header_t *header= packet;

	assert( header->type >= 0 && header->type <= 13 );

	for ( n= o->neighbours->head; n!= NULL; n= n->next ) {
		if ( n->sd != sd ) {
			send( n->sd, packet, pkt_size, 0 );
		}
	}
}

/* ============================================================================
 * Processing functions; deal with control messages we're about to send, or 
 * have just received.
 * ========================================================================= */

/**
 * process_refresh_packet:
 * 
 * Updates state from a refresh packet. Performs all mutex locking
 * required.
 */
static int process_refresh_packet( orta_t *o, refresh_packet_t *refresh, 
				   uint32_t buffer_length )
{
	refresh_data_t* link_data= &refresh->data;

	member_t *member;
	uint32_t source_ip= refresh->header.source_ip;
	int i;

#ifdef ORTA_DEBUG
	printf( "Recieved refresh packet from %s.\n", 
		print_ip(refresh->header.source_ip) );
#endif

	pthread_mutex_lock( o->links->lock );
	pthread_mutex_lock( o->members->lock );

	/* Check we haven't seen this packet already. if we have, 
	   make sure we don't forward it. */
	member= members_get( o->members, refresh->header.source_ip );

	/*member_update( member, refresh->header.seq );*/

#ifdef ORTA_DEBUG
	printf( "## Updating member %s with %d\n", 
		print_ip(member->member),
		refresh->header.seq );
#endif

	/* We haven't heard of this member... */
	if ( member == NULL ) {
		if ( members_add(o->members, source_ip) ) {
			/* Inform the application of the change in
			   membership. */
			update_membership_for_app( o );
		}
	}

	/* Have heard of them; try to update sequence number. If that 
	 * fails, unlock things and return FALSE. */
	else if ( !member_update( member, refresh->header.seq ) ) {
		pthread_mutex_unlock( o->members->lock );
		pthread_mutex_unlock( o->links->lock );

#ifdef ORTA_DEBUG
		printf("## process_refresh_packet: FAILED AFTER MEMBER UPDATE\n");
#endif

		return FALSE;
	}

	/* Update link weights for all links provided in the packet.
	 * If there are none, we save ourselves the expense of 
	 * recalculating the routing table. */
	if ( refresh->link_count ) {
		for ( i= 0; i < refresh->link_count; i++ ) {
			link_update( o->links, source_ip, 
				     (link_data+i)->to, 
				     (link_data+i)->weight);
		}

		/* Rebuild routing tables given the new information */
		/* FIXME: This should really only be run if 
		 * link_update has modified state, as the routing 
		 * won't be affected if it hasn't. */
		routing_build_table( o );
	}

	pthread_mutex_unlock( o->members->lock );
	pthread_mutex_unlock( o->links->lock );

	return TRUE;
}


/**
 * process_new_link_packet:
 * 
 * 
 */
static int process_new_link_packet( orta_t *o, flood_new_link_t *new_link, 
				    uint32_t buffer_length )
{
	member_t *member;

#ifdef ORTA_DEBUG
	printf( "flood_new_link: " );
	printf( "Adding link: %s -- ", print_ip(new_link->header.source_ip) );
	printf( "%s: %d.\n", print_ip(new_link->to), new_link->weight );
	printf( "flood_new_link: " );
	printf( "Adding link: %s -- ", print_ip(new_link->to) );
	printf( "%s: %d.\n", print_ip(new_link->header.source_ip), 
		new_link->weight );
#endif

	pthread_mutex_lock( o->links->lock );
	pthread_mutex_lock( o->members->lock );

	/* Check sequence number. If it's been seen already, 
	 * abandon packet. */
	member= members_get(o->members, new_link->header.source_ip);
	if ( member != NULL && 
	     !member_update( member, new_link->header.seq ) ) {
		pthread_mutex_unlock( o->members->lock );
		pthread_mutex_unlock( o->links->lock );

		return FALSE;
	}
	else if ( member == NULL ) {
		if ( members_add(o->members, new_link->header.source_ip) ) {
			/* Inform the application of the change in
			   membership. */
			update_membership_for_app( o );
		}
	}


	links_add( o->links, new_link->header.source_ip, new_link->to );
	links_add( o->links, new_link->to, new_link->header.source_ip );

	link_update(o->links, new_link->header.source_ip, 
		    new_link->to, new_link->weight);
	link_update(o->links, new_link->to, 
		    new_link->header.source_ip, new_link->weight);

	/* FIXME: Do some checking prior to this, to figure whether or not 
	 * the routing table actually has to be rebuilt? */
	routing_build_table( o );

	pthread_mutex_unlock( o->members->lock );
	pthread_mutex_unlock( o->links->lock );

	return TRUE;
}


/**
 * process_refresh_packet:
 * 
 * Updates state from a refresh packet. Performs all mutex locking
 * required.
 */
static int process_drop_link_packet( orta_t *o, flood_drop_links_t *msg, 
				     uint32_t buffer_length )
{
	int i;
	link_name_t *link;
	member_t *member;
	int sd;

#ifdef ORTA_DEBUG
	/* printf( "Got flood drop_link from %s.\n",
	   print_ip(flood->header.source_ip) ); */
#endif

	link= &msg->data;

	pthread_mutex_lock( o->links->lock );
	pthread_mutex_lock( o->members->lock );

	/* Check sequence number. If it's been seen already,
	 * abandon packet. */
	member= members_get(o->members, msg->header.source_ip);
	if ( member != NULL && 
	     !member_update( member, msg->header.seq ) ) {
		pthread_mutex_unlock( o->members->lock );
		pthread_mutex_unlock( o->links->lock );

		return FALSE;
	}

	pthread_mutex_lock( o->neighbours->lock );

	for ( i= 0; i<msg->link_count; i++ ) {
#ifdef ORTA_DEBUG
		/*printf( "flood_drop_links: " );
		  printf( "Removing link: %s -- ", print_ip(link->from));
		  printf( "%s.\n", print_ip(link->to) );*/
#endif

		links_rm(o->links, link->from, link->to);

		if ( link->from == o->local_ip && (sd= neighbours_contains(o->neighbours, link->to))) {
			struct sockaddr_in *addr;
			addr= neighbours_rm(o->neighbours, sd);
			free(addr);
		}

		link++;
	}

	pthread_mutex_unlock( o->neighbours->lock );

	routing_build_table( o );

	pthread_mutex_unlock( o->members->lock );
	pthread_mutex_unlock( o->links->lock );

	return TRUE;
}

/**
 * process_member_leave_packet:
 * 
 */
static int process_member_leave_packet( orta_t *o, flood_member_leave_t *msg, 
					uint32_t buffer_length )
{
	link_name_t *data= &(msg->data);
	int i, sd;
	int fwd= FALSE;

#ifdef ORTA_DEBUG
	printf("flood_member_leave: Got a member_leave packet.\n");
	printf("flood_member_leave: Packet generated by %s.\n",
	       print_ip(msg->header.source_ip) );
#endif

	pthread_mutex_lock( o->links->lock );
	pthread_mutex_lock( o->members->lock );

	if ( members_rm( o->members, msg->member ) )
		fwd= TRUE;

	if ( !o->members->length )
		o->connected= FALSE;

#ifdef ORTA_DEBUG
	printf( "flood_member_leave: " );
	printf( "Removing member: %s\n", print_ip(msg->member) );
#endif

	for ( i= 0; i<msg->link_count; i++ ) {
#ifdef ORTA_DEBUG
		printf( "flood_member_leave: " );
		printf( "Removing: %s -- ", print_ip(data->from) );
		printf( "%s.\n", print_ip(data->to) );
#endif

		if ( links_rm(o->links, data->from, data->to ) != -1 )
			fwd= TRUE;
		data++;
	}

	/* Rebuild routing table with new link state, but only if link 
	 * state has changed.*/
	if ( fwd )
		routing_build_table( o );


	/* Compile member list for the application, if it's registered
	   a callback. */
	if ( fwd ) {
		update_membership_for_app( o );
	}


	pthread_mutex_lock( o->neighbours->lock );

	/* Check if this member is a neighbour; if it is, remove it. */
	if (sd= neighbours_contains(o->neighbours, msg->member)) {
		struct sockaddr_in *addr;
		addr= neighbours_rm(o->neighbours, sd);
		free(addr);
	}

	pthread_mutex_unlock( o->neighbours->lock );
	pthread_mutex_unlock( o->members->lock );
	pthread_mutex_unlock( o->links->lock );

	return fwd;
}

/**
 * threshold_for_add:
 * 
 * Returns a threshold value based on the number of neighbours we
 * already have in the overlay, and the size of the group.
 * 
 * FIXME: Description of the sort of numbers this function returns.
 */
static float threshold_for_add( orta_t *o, uint32_t ip )
{
	float constant= 0.008;
	uint32_t nbr1= o->neighbours->length;
	uint32_t nbr2= num_links_from(o->links, ip);
	float mem= o->members->length;

	return constant*mem*nbr1*nbr2;
}

/**
 * threshold_for_drop:
 * 
 */
static float threshold_for_drop( orta_t *o, uint32_t ip )
{
	float constant= 0.008;
	uint32_t nbr1= (o->neighbours->length-1);
	uint32_t nbr2= num_links_from(o->links, ip)-1;
	float mem= o->members->length;

	if ( nbr2 < 0 ) nbr2= 0;

	/* FIXME: I doubt this is really necessary. */
	return (constant*mem*nbr1*nbr2)-THRESHOLD_BUFFER;
}


/**
 * ctrl_refresh_dispatcher:
 * 
 * Periodically sends refresh packets to overlay neighbours.
 */
void* ctrl_refresh_dispatcher( orta_t *orta )
{
	link_from_t *tmp_node;
	link_to_t   *tmp_link;
	neighbour_t *n;

	refresh_packet_t* packet= (refresh_packet_t*)malloc(PACKET_SIZE);
	refresh_data_t* link_data;

	int packet_length;
	int fwd;

	packet->header.type= flood_refresh;
	packet->header.seq= ++(orta->local_seq);
	packet->header.source_ip= orta->local_ip;

	packet->link_count= 0;
	link_data= (refresh_data_t*)(&packet->data);

	/* FIXME: Initial length of packet. This is a bit ugly. */
	packet_length= sizeof(refresh_packet_t)-sizeof(refresh_data_t);

	pthread_mutex_lock( orta->links->lock );
	pthread_mutex_lock( orta->neighbours->lock );

	/* Group up all known links to transmit to neighbours */
	for ( n= orta->neighbours->head; n != NULL; n= n->next ) {
		uint32_t dest_ip= n->addr->sin_addr.s_addr;
		/* Advertised weight */
		uint32_t ad_weight= link_distance( orta->links, 
						   orta->local_ip, 
						   dest_ip );

		/* FIXME: Check this. */
		if ( ( n->distance > (ad_weight+(int)(ad_weight*OFFSET_FRACTION)) || 
		       n->distance < (ad_weight-(int)(ad_weight*OFFSET_FRACTION)) ) ||
		     n->last_sent == REFRESH_CYCLES_FOR_SEND ) {
			packet->link_count++;

			link_data->to=     dest_ip;
			link_data->weight= n->distance;

			packet_length+= sizeof(refresh_data_t);
			link_data++;

			n->last_sent= 0;
		}
		/* If we're not sending information on this link, increment 
		 * count until next time it will be guaranteed to be sent. */
		else {
			n->last_sent++;
		}
	}

	pthread_mutex_unlock( orta->neighbours->lock );
	pthread_mutex_unlock( orta->links->lock );

	fwd= process_refresh_packet( orta, (refresh_packet_t*)packet, 
				     packet_length );

	if ( fwd ) {
#ifdef ORTA_DEBUG
		printf( "## FORWARDING REFRESH!\n" );
#endif
		flood( orta, packet, packet_length );
	}
#ifdef ORTA_DEBUG
	else {
		printf( "## NOT FORWARDING REFRESH.\n" );
	}
#endif

	free( packet );
}


/*****************************************************************************/
/* Functions used to initiate state change in the group                      */
/*****************************************************************************/



/**
 * ctrl_join: Attempts to join the overlay.
 * 
 * Upon connecting to an existing overlay member, passes information
 * back and forth to bring it up to date with the current state of the
 * system. If the connection cannot be made, will return FALSE, else
 * will return TRUE.  Once a connection is formed, the member to which
 * `dest' points will be our neighbour. The socket descriptor made by
 * this connection will be added to the master read set for reading in
 * ctrl_port_listener().
 */
int ctrl_join( orta_t *o, char *dest )
{
	int sd;
	int i;
	uint32_t new_ip;
	sockaddr_in_t *addr= (sockaddr_in_t*)malloc(sizeof(sockaddr_in_t));

	control_packet_header_t packet;
	flood_new_link_t flood_pkt;

	join_ok_packet_t *response= (join_ok_packet_t*)malloc(PACKET_SIZE);
	member_data_t    *member_data= &(response->data);
	link_data_t      *link_data;

	if ( !strncmp( "127.0.0.1", dest, strlen(dest) ) ) {
		free( addr );
		free( response );

		return TRUE;
	}

	/* Attempt to make the connection. */
#ifdef ORTA_DEBUG
	printf( "ctrl_join: Connecting to destination port %u\n", TCP_PORT );
#endif
	if ( !connectTCP( dest, TCP_PORT, &sd, addr ) ) {
		fprintf(stderr,
			"ctrl_join: Could not connect to %s.\n",
			dest );
		free(addr);
		return FALSE;
	}

	/* Send a 'join' packet */
	packet.type= join;

	send( sd, &packet, sizeof(control_packet_header_t), 0 );

	/* Wait for a join_ok packet */
	if ( (recv( sd, response, PACKET_SIZE, 0 ) <= 0) ||
	     (response->header.type != join_ok) ) {
		/* There was an error ... free up memory and signal our 
		 * defeat. */
		free( addr );
		free( response );
		return FALSE;
	}

	/* Success, we've connected. */
#ifdef ORTA_DEBUG
	printf("ctrl_join: Neighbour has been accepted.\n" );
	printf("ctrl_join: New socket sd is: %d.\n", sd );
#endif

	pthread_mutex_lock( o->links->lock );
	pthread_mutex_lock( o->members->lock );
	pthread_mutex_lock( o->neighbours->lock );

	/* Parse member data... */
	for( i= 0; i < response->member_count; i++ ) {
		if ( members_add(o->members, (member_data+i)->ip_addr) ) {
			/* Inform the application of the change in
			   membership. */
			update_membership_for_app( o );
		}
	}

	/* Parse link state data... */
	link_data= (link_data_t*)(member_data+i);
	for ( i= 0; i < response->link_count; i++ ) {
		links_add( o->links, 
			   (link_data+i)->from, 
			   (link_data+i)->to );
		link_update( o->links, 
			     (link_data+i)->from, 
			     (link_data+i)->to, 
			     (link_data+i)->weight );
	}

	/* Augment recieved state with new local state */
	new_ip= addr->sin_addr.s_addr;

	members_add( o->members, o->local_ip );
	links_add( o->links, new_ip,      o->local_ip );
	links_add( o->links, o->local_ip, new_ip );
	neighbours_add( o->neighbours, sd, addr );

	/* We've succeeded; add this socket descriptor to the read set
	 * for the main select clause to watch */
	if ( sd > o->fdmax )
		o->fdmax= sd;
	FD_SET(sd, o->master);


	/* Build our routing table given the information we've
	 * recieved. */
	routing_build_table( o );

	pthread_mutex_unlock( o->neighbours->lock );
	pthread_mutex_unlock( o->members->lock );
	pthread_mutex_unlock( o->links->lock );

	/* Enlighten the rest of the group with our presence; sort out
	 * packet for flooding back into the overlay, and send. */
	flood_pkt.header.type=      flood_new_link;
	flood_pkt.header.seq=       o->local_seq;
	flood_pkt.header.source_ip= o->local_ip;
	flood_pkt.to= new_ip;
	flood_pkt.weight= DEFAULT_DIST;

	/* We've accepted the join; flood information out to other
	 * members */
	flood( o, &flood_pkt, sizeof(flood_new_link_t) );

	return TRUE;
}

/**
 * ctrl_leave_group:
 * 
 */
int ctrl_leave_group( orta_t *o )
{
	int packet_len;
	flood_member_leave_t *packet;
	link_name_t *data;
	neighbour_t *nbr;

	pthread_mutex_lock( o->neighbours->lock );

	packet_len= sizeof(flood_member_leave_t)+
		(o->neighbours->length*2*sizeof(link_name_t))-
		sizeof(link_name_t);
	packet= (flood_member_leave_t*)malloc(packet_len);

	/* Sort out packet stuff */
	packet->header.type= flood_member_leave;
	packet->header.source_ip= o->local_ip;
	packet->member= o->local_ip;
	packet->link_count= o->neighbours->length*2;

	/* Bundle link data into packet */
	data= &(packet->data);
	nbr= o->neighbours->head;
	while ( nbr != NULL ) {
		data->from= o->local_ip;
		data->to=   nbr->addr->sin_addr.s_addr;
		data++;

		data->from= nbr->addr->sin_addr.s_addr;
		data->to=   o->local_ip;
		data++;

		nbr= nbr->next;
	}

	flood( o, packet, packet_len );

#ifdef ORTA_DEBUG
	printf( "ctrl_leave_group: Closing off connections.\n" );
#endif

	/* Close off all sockets now we've informed neighbours */
	nbr= o->neighbours->head;
	while ( nbr != NULL ) {
		FD_CLR( nbr->sd, o->master );
		close( nbr->sd );
		nbr= nbr->next;
	}

	pthread_mutex_unlock( o->neighbours->lock );

	free(packet);

	return TRUE;
}



/**
 * ctrl_drop_link:
 * 
 * Removes a member of the overlay from our local state.
 */
static int ctrl_drop_link( orta_t *orta, uint32_t sd )
{
	sockaddr_in_t *addr;
	uint32_t dest_ip;

	uint32_t packet_len= sizeof(flood_drop_links_t)+sizeof(link_name_t);
	flood_drop_links_t *flood_pkt= (flood_drop_links_t*)malloc(packet_len);
	link_name_t *link= &(flood_pkt->data);

	/* Remove this socket descriptor from the read set */
	if ( sd == orta->fdmax )
		orta->fdmax--;
	FD_CLR(sd, orta->master);

	/* Get destination address */
        addr= neighbours_rm( orta->neighbours, sd );
        dest_ip= addr->sin_addr.s_addr;

	/* Sort out packet for flooding knowledge of this new link out
	 * into the overlay, and send. */
	flood_pkt->header.type= flood_drop_links;
	flood_pkt->header.source_ip= orta->local_ip;
	flood_pkt->header.seq= ++(orta->local_seq);
	flood_pkt->header.source_ip= orta->local_ip;
	flood_pkt->link_count= 2;

	link->from= orta->local_ip;
	link->to=   dest_ip;

#ifdef ORTA_DEBUG
	printf( "ctrl_drop_link: " );
	printf( "Removing link: %s -- ", print_ip(link->from) );
	printf( "%s.\n", print_ip(link->to) );
#endif

	link++;
	link->from= dest_ip;
	link->to=   orta->local_ip;

#ifdef ORTA_DEBUG
	printf( "ctrl_drop_link: " );
	printf( "Removing link: %s -- ", print_ip(link->from) );
	printf( "%s.\n", print_ip(link->to) );
#endif

	/* Flood this information out to other members */
	flood( orta, flood_pkt, packet_len );

        /* Remove knowledge of this link and this neighbour from
	   overlay state */
        links_rm( orta->links, dest_ip,        orta->local_ip );
        links_rm( orta->links, orta->local_ip, dest_ip );
        free(addr);

        /* Rebuild routing table. */
        routing_build_table( orta );

#ifdef ORTA_DEBUG
	printf( "ctrl_drop_link: %s: Flooding drop_link.\n", 
		print_ip(orta->local_ip) );
#endif

	close( sd );
	free(flood_pkt);

	return TRUE;
}


/**
 * ctrl_add_weighted_link:
 * 
 * Attempts to connect to the ip address `ip', assigning `weight' as
 * that link's weight to be stored locally.  If the connection cannot
 * be made, this function returns -1.  If the connection can be made,
 * but the peer rejects the connection, 0 is returned.  If the
 * connection is succesful, the socket descriptor is returned.
 */
static int ctrl_add_weighted_link( orta_t *o, uint32_t ip, uint32_t weight )
{
	/* Connect to potential new neighbour, and send request to add a link.
	 * On reciept of response, add link and flood new state to group. */
	int sd;
	uint32_t new_ip;
	sockaddr_in_t *addr= (sockaddr_in_t*)malloc(sizeof(sockaddr_in_t));

	control_packet_header_t packet;
	flood_new_link_t flood_pkt;

	/* Attempt to make the connection. */
	if ( !connectTCP( (char*)print_ip(ip), TCP_PORT, &sd, addr ) ) {
		printf("ctrl_add_link: Could not connect to %s.\n",
			print_ip(ip) );
		free(addr);
		return -1;
	}

	/* Send a 'add_link' request packet */
	packet.type= req_add_link;

	send( sd, &packet, sizeof(control_packet_header_t), 0 );

	/* Wait for a reply_add_link packet */
	if ( recv( sd, &packet, sizeof(control_packet_header_t), 0 ) <= 0 ) {
#ifdef ORTA_DEBUG
		printf( "Our connection to %s was rejected.\n",
			print_ip(ip));
		printf( "packet type %u\n", packet.type );
#endif
		/*debug_packet( &packet, 28 );*/
		/* Error in connection, free up memory and return failure */
		free( addr );
		return -1;
	}
	else if (packet.type != req_add_link_ok) {
#ifdef ORTA_DEBUG
		printf( "Our connection to %s was rejected.\n",
                        print_ip(ip));
		printf( "packet type %u\n", packet.type );
#endif
		/*debug_packet( &packet, 28 );*/
		free( addr );
		return FALSE;
	}
	else {
#ifdef ORTA_DEBUG
		printf( "Recieved packet type %d in return.\n", packet.type );
#endif
	}

#ifdef ORTA_DEBUG
	/* Success, we've connected. */
	printf("ctrl_add_link: Neighbour has been accepted.\n" );
	printf("ctrl_add_link: New socket sd is: %d.\n", sd );
#endif

	/* Add knowledge of this link and this neighbour to overlay
	   state */
	new_ip= addr->sin_addr.s_addr;

	links_add(   o->links, new_ip,      o->local_ip );
	link_update( o->links, new_ip,      o->local_ip, weight );
	links_add(   o->links, o->local_ip, new_ip );
	link_update( o->links, o->local_ip, new_ip,      weight );

	neighbours_add( o->neighbours, sd, addr );
	neighbour_update(neighbours_get_nbr(o->neighbours, sd), weight);

	/* We've succeeded; add this socket descriptor to the read set for 
	 * the main select clause to watch */
	if ( sd > o->fdmax )
		o->fdmax= sd;
	FD_SET(sd, o->master);

	/* Build our routing table given the information we've recieved. */
	routing_build_table( o );

	/* Sort out packet for flooding knowledge of this new link out 
	 * into the overlay, and send. */
	flood_pkt.header.type= flood_new_link;
	flood_pkt.header.seq= ++(o->local_seq);
	flood_pkt.header.source_ip= o->local_ip;
	flood_pkt.to=   new_ip;
	flood_pkt.weight= weight;

	/* We've accepted the join; flood information out to other members */
	flood( o, &flood_pkt, sizeof(flood_new_link_t) );

	return sd;
}

/**
 * ctrl_add_link:
 * 
 * Returns socket descriptor value on success, and FALSE on failure.
 */
static int ctrl_add_link( orta_t *o, uint32_t ip )
{
	return ctrl_add_weighted_link( o, ip, DEFAULT_DIST );
}


/*****************************************************************************/
/* Control functions for evaluating adding/dropping of links                 */
/*****************************************************************************/


/**
 * evaluate_drop_link:
 * Looks at the list of links this peer currently has, and determines whether 
 * or not to drop a given link.
 */
void evaluate_drop_link( orta_t *orta )
{
	linked_list_t *distances_with, *distances_without;
	member_t *mem;
	neighbour_t *nbr;

	float utility;
	uint32_t tmp_weight;

	uint16_t sd;
	uint32_t dest_ip;

	int random;
	member_t *member;

	int i;

	utility= 0;

	list_init( &distances_with );
	list_init( &distances_without );

	pthread_mutex_lock( orta->links->lock );
	pthread_mutex_lock( orta->members->lock );
	pthread_mutex_lock( orta->neighbours->lock );

	if ( !orta->neighbours->length ) {
		pthread_mutex_unlock( orta->neighbours->lock );
		pthread_mutex_unlock( orta->members->lock );
		pthread_mutex_unlock( orta->links->lock );

		return;
	}

	random= rand()%orta->neighbours->length;
	nbr= orta->neighbours->head;
	for ( i= 0; i<random; i++ ) {
		nbr= nbr->next;
	}

	dest_ip= nbr->addr->sin_addr.s_addr;
	sd= nbr->sd;

	/* Calculate shortest paths with all links in place */
	shortest_paths( orta->local_ip, orta->links, distances_with );
	/* Remove the link, but remember its weight */
	tmp_weight= links_rm( orta->links, orta->local_ip, dest_ip );
	/* Calculate shortest path over the new graph */
	shortest_paths( orta->local_ip, orta->links, distances_without );
	/* Replace the link to return to previous state */
	links_add(   orta->links, orta->local_ip, dest_ip );
	link_update( orta->links, orta->local_ip, dest_ip, tmp_weight );


#ifdef ORTA_DEBUG
	/*printf( "evaluate_drop_link: distances_with all links in place:\n" );
	  dijkstra_print_links_list( distances_with );*/

	/* printf( "evaluate_drop_link: Looking at link from here (%s) to ", 
	   print_ip(m->local_ip));
	   printf( "%s.\n", print_ip(nbr->addr->sin_addr.s_addr ) );
	   printf( "\nevaluate_drop_link: distances_without this link:\n" );
	   dijkstra_print_links_list( distances_without );*/
#endif

	for (mem= orta->members->head; mem != NULL; mem= mem->next ) {
		int current_latency, new_latency;

		/* Skip local host */
		if ( mem->member == orta->local_ip )
			continue;

		new_latency=     link_distance_to( distances_without, 
						   mem->member);
		current_latency= link_distance_to( distances_with,
						   mem->member);

		/* If by dropping this link we lose the ability to reach 
		 * a node, we simply don't want to drop the link. */
		if ( new_latency == INFINITY ) {
			utility= INFINITY;
			break;
		}

		if ( new_latency > current_latency )
			utility += ((float)(new_latency-current_latency))/((float)new_latency);
	}

#ifdef ORTA_DEBUG
	printf( "## Utility == %f\n", utility );
	printf( "## Threshold to be beneath for drop is: %f.\n", 
		threshold_for_drop(orta, dest_ip) );
#endif

	pthread_mutex_unlock( orta->neighbours->lock );
	pthread_mutex_unlock( orta->members->lock );
	pthread_mutex_unlock( orta->links->lock );

	if ( utility < threshold_for_drop(orta, dest_ip) ) {
#ifdef ORTA_DEBUG
		printf( "## Dropping link to %s.\n", print_ip(dest_ip) );
#endif
		ctrl_drop_link( orta, sd );
	}

	list_destroy( &distances_without );
	list_destroy( &distances_with );
}


/**
 * evaluate_add_link:
 * 
 * Looks at the list of links this peer currently has, and determines
 * whether or not it would be beneficial to add a link to `dest_ip',
 * given the weight of the link as indicated by `weight'.
 */
void evaluate_add_link( orta_t *o, uint32_t dest_ip, uint32_t weight )
{
	linked_list_t *distances_with, *distances_without;
	member_t *member;
	float utility= 0;

	pthread_mutex_lock( o->links->lock );
	pthread_mutex_lock( o->members->lock );
	pthread_mutex_lock( o->neighbours->lock );

#ifdef ORTA_DEBUG
	printf( "evaluate_add_link: Recv'd ping response from non-neighbour: %s: %d\n", 
		print_ip(dest_ip), weight);
#endif

	/* Add the link and calculate shortest paths */
	links_add( o->links, o->local_ip, dest_ip );
	link_update( o->links, o->local_ip, dest_ip, weight );
	list_init( &distances_with );
	shortest_paths( o->local_ip, o->links, distances_with );

	/* Remove the link, and calculate shortest paths */
	links_rm( o->links, o->local_ip, dest_ip );
	list_init( &distances_without );
	shortest_paths( o->local_ip, o->links, distances_without );

	for (member= o->members->head; member != NULL; member= member->next) {
		int current_latency, new_latency;

		/* Skip local host */
		if ( member->member == o->local_ip )
			continue;

		new_latency=     link_distance_to( distances_with, 
						   member->member);
		current_latency= link_distance_to( distances_without,
						   member->member);

		/* FIXME: Check this. */
		if (new_latency == INFINITY || current_latency == INFINITY) {
			utility= INFINITY;
			continue;
		}

		else if ( new_latency < current_latency )
			utility += ((float)(current_latency-new_latency))/((float)current_latency);
	}

#ifdef ORTA_DEBUG
	/* If we're over a given threshold, add as a new neighbour. */
	printf( "evaluate_add_link: %s -- ", print_ip(o->local_ip) );
	printf( "%s.\n", print_ip(dest_ip) );
	printf( "evaluate_add_link: Utility: %f.\n", utility );
	printf( "evaluate_add_link: Threshold is %f for %u neighbours.\n", 
		threshold_for_add(o, dest_ip), o->neighbours->length );
#endif

	if ( utility > threshold_for_add(o, dest_ip) ) {
#ifdef ORTA_DEBUG
		printf( "evaluate_add_link: Adding link to %s\n", print_ip(dest_ip) );
#endif

		if ( ctrl_add_weighted_link( o, dest_ip, weight ) <= 0 ) {
#ifdef ORTA_DEBUG
			printf( "evaluate_add_link: Add link to %s failed.\n", 
				print_ip(dest_ip) );
#endif
		}
	}
	pthread_mutex_unlock( o->neighbours->lock );
	pthread_mutex_unlock( o->members->lock );
	pthread_mutex_unlock( o->links->lock );

	list_destroy( &distances_with );
	list_destroy( &distances_without );
}



/**
 * ctrl_fix_partition:
 * 
 */
int ctrl_fix_partition( orta_t *o )
{
	member_t *mbr, *temp_mbr;
	struct timeval time;
	flood_member_leave_t *leave;
	link_name_t *pkt_link;
	link_to_t *links, *next_link;
	uint32_t packet_size;
	int sd;

	gettimeofday( &time, NULL );

	pthread_mutex_lock( o->links->lock );
	pthread_mutex_lock( o->members->lock );

	/* Skim through all members. If the time since we last heard
	 * from any member is greater than some threshold (ideally a
	 * little larger than the normal refresh cycle time), attempt
	 * to add a link. If adding the link fails, remove member
	 * state and flood that the member and it's links are dead. */
	for ( mbr= o->members->head; mbr != NULL; ) {
		/* Iterate here, else we can segfault on next 
		 * execution of 'for...'. */
		temp_mbr= mbr;
		mbr= mbr->next;

		/* FIXME: Constant in code. */
		/* If [time this member has been silent for is > X AND
		 * attempt to create link fails] OR [time this member
		 * has been silent for is > Y], X being cause for
		 * probing and Y being cause for removing the member,
		 * craft a member_leave packet in their honour, and
		 * flood. */
		if ( (time.tv_sec - temp_mbr->tv.tv_sec) > 70 && 
		     ctrl_add_link(o, temp_mbr->member) < 0 ) {
#ifdef ORTA_DEBUG
			printf( "ctrl_fix_partition: " );
			printf("Member %s has fallen silent and isn't responding.\n", 
			       print_ip( temp_mbr->member ) );
			printf("Removing that member and informing the group.\n" );
#endif

			packet_size= sizeof(flood_member_leave_t)-sizeof(link_name_t);
			leave= (flood_member_leave_t*)malloc(PACKET_SIZE);
			leave->header.type= flood_member_leave;

			leave->member= temp_mbr->member;
			leave->link_count= 0;
			pkt_link= &(leave->data);

			links= links_from( o->links, temp_mbr->member );

			while ( links != NULL ) {
				uint32_t end1= temp_mbr->member;
				uint32_t end2= links->ip;
				links= links->next_link;

				pkt_link->from= end1;
				pkt_link->to  = end2;

#ifdef ORTA_DEBUG
				printf( "Removing: %s -- ", print_ip(end1) );
				printf( "%s\n", print_ip(end2) );
#endif
				links_rm(o->links, end1, end2);

				pthread_mutex_lock( o->neighbours->lock );

				/* Is this a link to one of our neighbours? */
				if (end1 == o->local_ip && (sd= neighbours_contains(o->neighbours, end2))) {
					struct sockaddr_in *addr;
					addr= neighbours_rm(o->neighbours, sd);
					free( addr );
				}

				packet_size+= sizeof(link_name_t);
				leave->link_count++;
				pkt_link++;

				pkt_link->from= end2;
				pkt_link->to  = end1;

#ifdef ORTA_DEBUG
				printf( "Removing: %s -- ", print_ip(end2) );
				printf( "%s\n", print_ip(end1) );
#endif
				links_rm(o->links, end2, end1);

				/* Is this a link to one of our neighbours? */
                                if (end2 == o->local_ip && (sd= neighbours_contains(o->neighbours, end1))) {
                                        struct sockaddr_in *addr;
                                        addr= neighbours_rm(o->neighbours, sd);
                                        free( addr );
                                }

				packet_size+= sizeof(link_name_t);
				leave->link_count++;
				pkt_link++;

				pthread_mutex_unlock( o->neighbours->lock );
			}

			members_rm( o->members, leave->member );

			/* Flood new information */
			flood( o, leave, packet_size );

			/* Rebuild routing table */
			routing_build_table( o );

			free(leave);
		}
	}

	pthread_mutex_unlock( o->members->lock );
	pthread_mutex_unlock( o->links->lock );

	return TRUE;
}

/*****************************************************************************/
/* Functions for handling incoming data, new connections, & closed conn's    */
/*****************************************************************************/


/**
 * handle_connection:
 * 
 * Deals with an incoming connection.  In essence, remembers the new
 * socket descriptor. This function mirrors ctrl_init_add_neighbour,
 * in that this is what is called on the recieving side of that call.
 */
static int handle_connection( orta_t *o, uint32_t sd )
{
	int new_sd= 0;
	struct sockaddr_in addr;
	int addr_len= sizeof(struct sockaddr);

	struct linger linger = {1,1};

#ifdef ORTA_DEBUG
	printf("handle_connection: Entering handle_connection with sd == %d\n",
	       sd );
#endif

	if ( (new_sd= accept(sd, (struct sockaddr*)&addr, &addr_len) ) == -1){
		perror("accept");
		return -1;
	}
	/* Get the name of the connecting host; would see "0.0.0.0" otherwise*/
	getpeername( new_sd, (struct sockaddr *)&addr, &addr_len );

	if (setsockopt(new_sd, SOL_SOCKET, SO_LINGER, &linger,sizeof(linger)) == -1) {
		printf( "Couldn't set SO_LINGER on new socket.\n" );
        }

	FD_SET(new_sd, o->master);
	if ( sd > o->fdmax )
		o->fdmax= sd;

	return new_sd;
}


/**
 * send_state:
 *
 * Sends all link and member information to sd, the socket descriptor
 * for a connection to a new member.
 * 
 * Returns the volume of data, in bytes, sent to the other host.
 */
static uint32_t send_state( orta_t *o, uint32_t sd )
{
	int packet_length;
	int i= 0;

	/* malloc space for the packet itself */
	join_ok_packet_t* packet= (join_ok_packet_t*)malloc(PACKET_SIZE);
	packet->header.type= join_ok;

	/* Pointers into parts of the outgoing packet */
	member_data_t* member_data= (member_data_t*)(&packet->data);
	link_data_t* link_data;

	/* Temporary variables to walk data structures */
	member_t *member;
	link_from_t *tmp_node;
	link_to_t   *tmp_link;


	/* FIXME: Initial length of packet. This is a bit ugly. */
	packet_length= sizeof(join_ok_packet_t)-sizeof(member_data_t);

	/* Group up all known links to transmit to neighbours */
	pthread_mutex_lock( o->links->lock );
	pthread_mutex_lock( o->members->lock );

	packet->member_count= o->members->length;
	packet->link_count=   o->links->length;

	/* Bundle up and send list of members */
	/* 	printf( "send_state: Compiling member state.\n" ); */
	member= o->members->head;
	for ( i= 0; i < o->members->length; i++ ) {
		/*printf( "Looking at member %d: %u\n", i, member->member ); */
		(member_data+i)->ip_addr= member->member;
		member= member->next;
	}
	packet_length+= i*sizeof(member_data_t);

	/* The bundle up and send link state */
	/* printf( "send_state: Compiling link state.\n" ); */
	link_data= (link_data_t*)(member_data+i);

	for ( tmp_node= o->links->head; 
	      tmp_node != NULL; 
	      tmp_node= tmp_node->next_node ) {
		for (tmp_link= tmp_node->links; 
		     tmp_link != NULL; 
		     tmp_link= tmp_link->next_link) {

			link_data->from=   tmp_node->ip;
			link_data->to=     tmp_link->ip;
			link_data->weight= tmp_link->distance;

			link_data++;
		}
	}
	packet_length+= (o->links->length)*sizeof(link_data_t);

	/* printf( "send_state: Done compiling link state.\n" ); */

	pthread_mutex_unlock( o->members->lock );
	pthread_mutex_unlock( o->links->lock );

	/* Send the state packet */
	send( sd, packet, packet_length, 0 );

	free( packet );

	return packet_length;
}



static int parse_message( orta_t *orta, control_packet_header_t *packet, 
			  uint32_t buffer_length, uint32_t sd )
{
	int packet_length= 0;
	int fwd;

#ifdef PACKET_DEBUG
	printf( "Parse message:\n" );
	/*debug_packet( packet, buffer_length );*/
	printf( "--------------\n" );
#endif

	switch (packet->type) {

		/*************************************************************/
		/* Non-flooding packets first.                               */
		/*************************************************************/

	case join: {
		struct sockaddr_in* addr= 
		       (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
		int addrlen= sizeof(struct sockaddr);

#ifdef ORTA_DEBUG
		printf( "handle_control_data: Recieved join request on %d.\n", 
			sd );
#endif

		getpeername( sd, (struct sockaddr *)addr, &addrlen );

		/* FIXME: Simply allowing any connections. Good? Bad? */
		pthread_mutex_lock( orta->neighbours->lock );

#ifdef ORTA_DEBUG
		printf( "req_add_link: " );
		printf( "Recv'd join from: %s.\n", 
			print_ip( addr->sin_addr.s_addr ) );
#endif

		if ( neighbours_add( orta->neighbours, sd, addr ) ) {
			pthread_mutex_unlock( orta->neighbours->lock );
			packet_length= send_state( orta, sd );
		}
		else {
			pthread_mutex_unlock( orta->neighbours->lock );
			packet->type= join_deny;
			free( addr );

			send(sd, packet, sizeof(control_packet_header_t), 0);
			packet_length= sizeof(control_packet_header_t);
		}

		break;
	}

	case req_add_link: {
		struct sockaddr_in* addr= 
		       (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
		int addrlen= sizeof(struct sockaddr);

		getpeername( sd, (struct sockaddr *)addr, &addrlen );

#ifdef ORTA_DEBUG
 		printf("handle_control_data: Recv'd req_add_link on %d (%s)\n",
 			sd, print_ip( addr->sin_addr.s_addr) );
#endif

		pthread_mutex_lock( orta->links->lock );
		pthread_mutex_lock( orta->neighbours->lock );

		if ( !neighbours_add( orta->neighbours, sd, addr ) ) {
			packet->type= req_add_link_deny;

			send( sd, packet, sizeof(control_packet_header_t), 0 );
			free( addr );
		}
		else {
			packet->type= req_add_link_ok;
			packet->type= 4;

			send( sd, packet, sizeof(control_packet_header_t), 0 );
		}

		pthread_mutex_unlock( orta->neighbours->lock );
		pthread_mutex_unlock( orta->links->lock );

		packet_length= sizeof(control_packet_header_t);

		break;
	}

		/*************************************************************/
		/* Flooding packets next.                                    */
		/*************************************************************/

	case flood_new_link: {
		flood_new_link_t *new_link= (flood_new_link_t*)packet;
		packet_length= sizeof(flood_new_link_t);

		fwd= process_new_link_packet(orta, new_link, packet_length);

		if ( fwd ) {
#ifdef ORTA_DEBUG
			printf( "We need to flood the packet!\n" );
#endif
			fwd_flood( orta, packet, packet_length, sd );
		}

		break;
	}
	case flood_drop_links: {
		flood_drop_links_t *drop_links= (flood_drop_links_t*)packet;
		uint32_t pkt_len= sizeof(flood_drop_links_t);
		uint32_t link_len= sizeof(link_name_t);

		packet_length= pkt_len+(drop_links->link_count-1)*link_len;

		fwd= process_drop_link_packet(orta, drop_links, packet_length);

		if ( fwd )
			fwd_flood( orta, packet, packet_length, sd );

		break;
	}

	/* Recieved a refresh packet from a neighbour. */
	case flood_refresh: {
		refresh_packet_t *refresh= (refresh_packet_t*)packet;
		uint32_t pkt_len= sizeof(refresh_packet_t);
		uint32_t data_len= sizeof(refresh_data_t);

		packet_length= pkt_len+(refresh->link_count-1)*data_len;

		fwd= process_refresh_packet( orta, refresh, packet_length );

		if ( fwd )
			fwd_flood( orta, packet, packet_length, sd );

		break;
	}

	case flood_member_leave: {
		flood_member_leave_t *leave= (flood_member_leave_t*)packet;
		uint32_t pkt_len= sizeof(flood_member_leave_t);
		uint32_t data_len= sizeof(link_name_t);

		packet_length= pkt_len+(leave->link_count-1)*data_len;

		fwd= process_member_leave_packet(orta, leave, packet_length);

		if ( fwd )
			fwd_flood( orta, packet, packet_length, sd );

		break;
	}

	default: {
		sockaddr_in_t* addr= neighbours_get_addr(orta->neighbours, sd);

		printf( "---------------------------------\n" );
		printf( "---------------------------------\n" );
		printf( "---------------------------------\n" );
		printf( "---------------------------------\n" );
		printf( "Recieved abnormal packet from %d (%s)",
			sd, print_ip( addr->sin_addr.s_addr ));
		debug_packet( packet, buffer_length );
		printf( "---------------------------------\n" );
		printf( "---------------------------------\n" );
		printf( "---------------------------------\n" );
		printf( "---------------------------------\n" );
		break;
	}
	}

	return packet_length;
}

/**
 * handle_control_data:
 * Handler for incoming data. Checks packet header, and processes the packet 
 * appropriately.
 * This function deals with the TCP, control, sockets. UDP sockets are dealt 
 * with in handle_udp_data().
 */
static void handle_control_data( orta_t *orta, uint32_t sd )
{
	int nbytes, i, message_length;
	char* p;
	control_packet_header_t *packet= (control_packet_header_t*)malloc(PACKET_SIZE);

	/* If we have actual data, packet size is 1 or more.  If
	 * packet size is 0, the other end has closed the
	 * connection. Note just the connection has been closed; we
	 * cannot assume that the peer at the other end of the
	 * connection is dead. */
	if ((nbytes= recv( sd, packet, PACKET_SIZE, 0 )) <= 0) {
		struct sockaddr_in *addr;

		if ( sd == orta->fdmax )
			orta->fdmax--;
		FD_CLR( sd, orta->master );
		close( sd );

		pthread_mutex_lock( orta->neighbours->lock );
		addr= neighbours_rm( orta->neighbours, sd );
		if ( addr != NULL ) free(addr);
		pthread_mutex_unlock( orta->neighbours->lock );

		return;
	}

	message_length= nbytes;
	p= (char*)packet;
	while ( message_length > 0 ) {
		int offset;

		offset= parse_message( orta, (control_packet_header_t*)p, 
				       message_length, sd );
		message_length-= offset;
		p+= offset;
	}

	free( packet );
}



/**
 * ctrl_connection_listener:
 * Handles incoming data on a new port.
 */
void* ctrl_port_listener( void* d )
{
	struct connect_data* data= (struct connect_data*)d;
	int socket= data->sd;
	int port= data->port;
	orta_t *orta= data->orta;

	int i;
	int newfd;

	struct timeval timeout;
	int addrlen= sizeof(struct sockaddr);

	int listener_sd;
	listener_sd= bindTCP( port );

	fd_set read_fds; /* temp file descriptor list for select()*/


	/* listen*/
	if ( listen( listener_sd, 10 ) == -1 ) {
		perror("connection_listener: listen\n");
		exit(1);
	}

	/* add the listener to the master set*/
	if ( listener_sd > orta->fdmax )
		orta->fdmax= listener_sd;

	FD_SET(listener_sd, orta->master);


	/* main loop*/
	while( orta->alive ) {
		/* FIXME: */
		/* Essentially polling, to ensure we're looking at the
		 * maximum known file descriptor. Why? Because links
		 * can be added outwith the connection handler called
		 * within this code. */
		timeout.tv_sec=  0;
		timeout.tv_usec= 100000;

		read_fds= *(orta->master);

		/* Keep highest fdmax value */
		if ( orta->fdmax < listener_sd )
			orta->fdmax= listener_sd;

		if (select( orta->fdmax+1, &read_fds, NULL, NULL, &timeout) == -1) {
			perror("select");
			exit(1);
		}

		/* run through the existing connections looking for
		   data to read */
		for( i= 0; i <= orta->fdmax; i++ ) {
			if (FD_ISSET(i, &read_fds)) {
				if ( i == listener_sd ) {
					/* Dealing with a new connection */
					newfd= handle_connection( orta, i );
					if ( newfd > orta->fdmax )
						/* keep track of the maximum*/
						orta->fdmax = newfd;
				}
				else
					handle_control_data( orta, i );
			}
		}
	}

#ifdef ORTA_DEBUG
	printf( "Closed down ctrl listener\n" );
#endif
	pthread_exit(NULL);
}
