#include <stdlib.h>
#include <string.h>
#include "orta.h"

#define MAXHOSTNAMELEN     256

#include "linked_list.h"
#include "routing_table.h"
#include "neighbours.h"
#include "netTCP.h"

/*#include "orta_debug.h"*/
#include "orta_ctrl_tcp.h"
#include "orta_ctrl_udp.h"
#include "orta_data.h"

#include "orta_control_packets.h"
#include "orta_t.h"

#include <stdio.h>
#include <sys/time.h>   /* For gettimeofday */
#include <signal.h>     /* signal, SIGPIPE, etc */
#include <netdb.h>      /* struct hostent */
#include <netinet/in.h> /* For struct sockaddr_in */
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>

void* outgoing_data( void *o )
{
	orta_t *orta= (orta_t*)o;
	struct timeval time;

	uint32_t refresh_cycle_s= 30;
	uint32_t drop_link_cycle_s= 20;
	uint32_t random_ping_cycle_s= 10;
	uint32_t repartition_cycle_s= 15;
	uint32_t ping_delay_us= 300000;
#ifdef DEBUG_PRINT_STATE
	uint32_t debug_cycle_s= 5;
#endif
	uint32_t last_debug= 0;

	uint32_t last_fix_partition;
	uint32_t last_drop_link;
	uint32_t last_refresh;
	uint32_t last_rnd_ping;

	gettimeofday( &time, NULL );
	last_fix_partition= time.tv_sec;
	last_drop_link= time.tv_sec;
	last_refresh= time.tv_sec;
	last_rnd_ping= time.tv_sec;

	while (orta->connected) {
		gettimeofday( &time, NULL );

		/* Periodically check for members we haven't heard
		 * from in a while, suggesting a partition in the
		 * overlay */
		if ( time.tv_sec-last_fix_partition > repartition_cycle_s ) {
			last_fix_partition= time.tv_sec;
			ctrl_fix_partition( orta );
		}

		/* Periodically inform neighbours of current link
		 * states */
		if ( time.tv_sec-last_refresh > refresh_cycle_s ) {
			last_refresh= time.tv_sec;
			ctrl_refresh_dispatcher( orta );
		}

		/* Randomly ping somebody if we haven't pinged anybody
		 * in a while */
		if ( time.tv_sec - last_rnd_ping > random_ping_cycle_s ) {
			last_rnd_ping= time.tv_sec;
			random_ping(orta);
		}

#ifdef DEBUG_PRINT_STATE
		if ( time.tv_sec - last_debug > debug_cycle_s ) {
			printf( "------------------------------------------------------------------------------\n" );
			printf( "REFERENCE TIME: %u.%u\n",
				time.tv_sec, 
				time.tv_usec );
			printf( "outgoing_data: Current state is:\n" );
			print_state( orta );
			printf( "------------------------------------------------------------------------------\n" );

			last_debug= time.tv_sec;
		}
#endif

		/* Check to see if we have any poor quality links we
		 * should drop. */
		if ( time.tv_sec - last_drop_link > drop_link_cycle_s ) {
			evaluate_drop_link( orta );
			last_drop_link= time.tv_sec;
		}

		pinger( orta );

		usleep( ping_delay_us );
	}

#ifdef ORTA_DEBUG
	printf( "Outgoing scheduler has ended.\n" );
#endif
	pthread_exit(NULL);
}


/**
 * orta_addr_valid:
 * addr: string representation of IPv4 network address.
 *
 * Returns TRUE if addr is valid, FALSE otherwise.
 **/
int orta_addr_valid( const char *addr )
{
	struct in_addr addr4;
	struct hostent *h;

	if (inet_pton(AF_INET, addr, &addr4)) {
		return TRUE;
	} 

	h = gethostbyname(addr);
	if (h != NULL) {
		return TRUE;
	}

	return FALSE;
}

/**
 * orta_init:
 * 
 * addr: character string containing an IPv4 network address.
 * udp_rx_port: receive port.
 * udp_tx_port: transmit port.
 * ttl: time-to-live value for transmitted packets.
 *
 * Creates overlay state for use on connecting to an existing overlay, 
 * or to allow incoming connections from other hosts.
 *
 * Returns: either a valid orta_t*, or NULL.
 **/
orta_t* orta_init(uint16_t udp_rx_port, uint16_t udp_tx_port, int ttl)
{
	return orta_init_if( NULL, udp_rx_port, udp_tx_port, ttl );
}

/**
 * orta_init_if:
 * 
 * addr: character string containing an IPv4 network address.
 * iface: character string containing an interface name.
 * rx_port: receive port.
 * tx_port: transmit port.
 * ttl: time-to-live value for transmitted packets.
 * 
 * Creates overlay state for use on connecting to an existing overlay,
 * or to allow incoming connections from other hosts.
 * 
 * Returns: either a valid orta_t*, or NULL.
 **/
orta_t* orta_init_if(const char *iface, uint16_t udp_rx_port, 
		     uint16_t udp_tx_port, int ttl)
{
	orta_t *orta= (orta_t*)malloc(sizeof(orta_t));

	struct connect_data* d= 
		(struct connect_data*)malloc(sizeof(struct connect_data));

	/* FIXME: Constant in code */
	char localhostname[MAXHOSTNAMELEN];
	int sd;

	sockaddr_in_t tmp_addr;
	uint32_t udp_buffer_size= 210944;

	/* FIXME: Ignore broken pipes... */
	signal(SIGPIPE, SIG_IGN);

	/* Determine local IP address */
	if ( !inet_aton( orta_host_addr(), &(tmp_addr.sin_addr) ) ) {
		fprintf( stderr, "orta_init: Failed to determine local IP.\n");
		return NULL;
	}
	orta->local_ip= tmp_addr.sin_addr.s_addr;

	/* Initialise list of neighbours */
	if ( !neighbours_init( &(orta->neighbours) ) ) {
		fprintf(stderr, 
			"orta_init: Failed to initialise neighbours table.\n");
		return NULL;
	}
	/* FIXME: Convert all to adjacency list? */
	/* Initialise list of links in overlay */
	if ( !links_init( &(orta->links) ) ) {
		fprintf(stderr,
			"orta_init: Failed to initialise links table.\n");
		return NULL;
	}
	/* Initialise members list */
	if ( !members_init( &(orta->members) ) ) {
		fprintf(stderr,
			"orta_init: Failed to initialise members table.\n");
		return NULL;
	}
	/* Initialise routing table */
	if ( !routing_table_init( &(orta->route) ) ) {
		fprintf(stderr,
			"orta_init: Failed to initialise routing table.\n");
		return NULL;
	}
	/* Initialise data queues list */
	if ( !list_init( &(orta->data_queues) ) ) {
		fprintf(stderr,
			"orta_init: Failed to initialise data queues.\n");
		return NULL;
	}
	/* Add the default queue to the list */
	orta_register_channel( orta, 0 );
	pthread_cond_init( &(orta->data_arrived), NULL );

	/* master file descriptor list*/
	orta->fdmax= 0;
	orta->master= (fd_set*)malloc(sizeof(fd_set));
	FD_ZERO( orta->master );

	/* Create and bind socket for use with UDP traffic */
	orta->udp_sd= socket( AF_INET, SOCK_DGRAM, 0 );
	if ( setsockopt(orta->udp_sd, SOL_SOCKET, SO_SNDBUF, &udp_buffer_size, sizeof(udp_buffer_size)) == -1 )
		perror( "orta_init_if" );
	if ( setsockopt(orta->udp_sd, SOL_SOCKET, SO_RCVBUF, &udp_buffer_size, sizeof(udp_buffer_size)) == -1 )
		perror( "orta_init_if" );

	orta->udp_rx_port= udp_rx_port;
	orta->udp_tx_port= udp_tx_port;
	if ( orta->udp_sd < 0 ) {
		fprintf(stderr, "orta_init: Cannot open UDP socket!\n");
		exit(1);
	}
        tmp_addr.sin_family= AF_INET;
        tmp_addr.sin_port= htons(udp_rx_port);
        tmp_addr.sin_addr.s_addr= INADDR_ANY;
        memset(&(tmp_addr.sin_zero), '\0', 8);

	if (bind(orta->udp_sd, (sockaddr_t*)&tmp_addr, sizeof(sockaddr_t)) < 0)
		perror( "handle_udp_data" );

#ifdef ORTA_DEBUG
	printf( "orta_init: This host is %s.\n", print_ip(orta->local_ip) );
	printf( "orta_init: Grabbed socket %d on port %d for UDP traffic.\n", 
		orta->udp_sd, 
		ntohs(tmp_addr.sin_port) );
#endif


	/* Set sequence number to 0 */
	orta->local_seq= 0;

	/* Set alive and connected flags; alive is set to FALSE to tell 
	 * threads to stop for shutting down; connected is set FALSE to tell 
	 * threads to not attempt to send data etc. */
	orta->alive= TRUE;
	orta->connected= FALSE;

	/* Add ourselves as a known group member */
	members_add( orta->members, orta->local_ip );

	/*FIXME: make prettier. Initially -1, set only if connection created.*/
	d->sd= -1;
	d->orta= orta;
	d->port= TCP_PORT;
	pthread_create(&orta->ctrl_recv_thread, NULL, &ctrl_port_listener, d);
 	pthread_create(&orta->ctrl_udp_thread,  NULL, &handle_udp_data, orta);

#ifdef ORTA_DEBUG
	/* FIXME */
	printf( "Warning: orta_init_if is still ignoring *iface\n" );
#endif

	orta->update_membership= NULL;

	return orta;
}

/**
 * orta_register_channel:
 * 
 * Registers a new data channel at this host for the purposes of
 * recieving UDP data on that particular channel. Channels can be seen
 * as an emulation of system ports.
 */
int orta_register_channel( orta_t *orta, uint32_t channel )
{
	queue_t *queue;
	queue_init( &queue );

	pthread_mutex_lock( orta->data_queues->lock );

	list_add( orta->data_queues, channel, queue );

	pthread_mutex_unlock( orta->data_queues->lock );
}


/**
 * orta_connect: 
 * 
 * Attempts to connect to a peer already in the peer-group using
 * `addr', which should be a character string containing an IPv4
 * network address.
 */
int orta_connect( orta_t *orta, const char *dest )
{
	int hostname_len= 80;

	struct hostent *host_ent;
	char hostname[hostname_len];
	struct in_addr iaddr;

	if ( dest == NULL ) {
		orta->connected= TRUE;
		pthread_create(&orta->ctrl_sched_thread, NULL, &outgoing_data, 
			       (void*)orta);

		return TRUE;
	}

	host_ent= gethostbyname(dest);
	assert(host_ent->h_addrtype == AF_INET);
	memcpy(&iaddr.s_addr, host_ent->h_addr, sizeof(iaddr.s_addr));
	strncpy(hostname, (const char *)inet_ntoa(iaddr), hostname_len);

	if ( !orta_addr_valid( hostname ) ) {
		orta->connected= FALSE;
		return FALSE;
	}

	if ( !ctrl_join( orta, hostname ) ) {
		orta->connected= FALSE;
		return FALSE;
	}

	orta->connected= TRUE;
	pthread_create(&orta->ctrl_sched_thread, NULL, &outgoing_data, 
		       (void*)orta);


	return TRUE;
}


/**
 * orta_disconnect:
 * 
 * Closes all connections into peer-group cleanly, and clears state
 * held within the orta_t struct..
 */
int orta_disconnect( orta_t *orta )
{
	uint16_t sd;
	struct sockaddr_in *addr;
	member_t *member;
	route_t  *route;
	int i;

#ifdef ORTA_DEBUG
	printf( "Called orta_disconnect()\n" );fflush(stdout);
#endif

	/* Set disconnected */
	orta->connected= FALSE;

	/* Send 'leave' packet to all neighbours */
	ctrl_leave_group( orta );

#ifdef ORTA_DEBUG
	printf( "Locking links.\n" );fflush(stdout);
#endif
	pthread_mutex_lock( orta->links->lock );
#ifdef ORTA_DEBUG
	printf( "Locking members.\n" );fflush(stdout);
#endif
	pthread_mutex_lock( orta->members->lock );
#ifdef ORTA_DEBUG
	printf( "Locking neighbours.\n" );fflush(stdout);
#endif
	pthread_mutex_lock( orta->neighbours->lock );
#ifdef ORTA_DEBUG
	printf( "Locking routing table.\n" );fflush(stdout);
#endif
	pthread_mutex_lock( orta->route->lock );
#ifdef ORTA_DEBUG
	printf( "orta_disconnect: Locked everything down.\n" );fflush(stdout);
#endif

	orta->fdmax= 0;
	FD_ZERO( orta->master );

        /* Clear members list */
	members_clear( orta->members );
#ifdef ORTA_DEBUG
	printf( "orta_disconnect: Cleared members.\n" );fflush(stdout);
#endif

	/* Clear neighbours table and close off connections */
	neighbours_clear( orta->neighbours );
#ifdef ORTA_DEBUG
	printf( "orta_disconnect: Cleared neighbours.\n" );fflush(stdout);
#endif

	/* Clear link state (links-destroy clears list and frees memory) */
	links_clear( orta->links );
#ifdef ORTA_DEBUG
	printf( "orta_disconnect: Cleared links.\n" );fflush(stdout);
#endif

	/* Clear routing table */
	routing_table_clear( orta->route );
#ifdef ORTA_DEBUG
	printf( "orta_disconnect: Cleared routing table.\n" );fflush(stdout);
#endif

	pthread_mutex_unlock( orta->route->lock );
	pthread_mutex_unlock( orta->neighbours->lock );
	pthread_mutex_unlock( orta->members->lock );
	pthread_mutex_unlock( orta->links->lock );

#ifdef ORTA_DEBUG
	printf( "orta_disconnect: Unlocked everything.\n" );

	/* FIXME: debug code */
	printf( "orta_disconnect: Have now disconnected from the overlay.\n" );
	printf( "orta_disconnect: Current state is:" );
	print_state( orta );
	printf( "orta_disconnect: Done.\n" );
#endif

	return TRUE;
}


/**
 * orta_destroy:
 * 
 *  Destroys the state used by `orta' and frees up any occupied
 *  memory.
 **/

void orta_destroy( orta_t **o )
{
	orta_t *orta= *o;

#ifdef ORTA_DEBUG
	printf( "orta_destroy: Entering...\n" );
#endif

	orta->alive= FALSE;

	pthread_join( orta->ctrl_recv_thread, NULL );
	pthread_join( orta->ctrl_sched_thread, NULL );
#ifdef ORTA_DEBUG
	printf( "orta_destroy: FIXME: Would now join ctrl_udp_thread\n" );
#endif
/* 	pthread_join( orta->ctrl_udp_thread, NULL ); */

#ifdef ORTA_DEBUG
	printf( "orta_destroy: Threads have finished.\n" );
#endif

	/* Lock out other threads */
	pthread_mutex_lock( orta->links->lock );
	pthread_mutex_lock( orta->members->lock );
	pthread_mutex_lock( orta->neighbours->lock );
	pthread_mutex_lock( orta->route->lock );
#ifdef ORTA_DEBUG
	printf( "orta_destroy: Locked everything down; freeing...\n" );
#endif

	links_destroy( &(orta->links) );
	members_destroy( &(orta->members) );
	neighbours_destroy( &(orta->neighbours) );
	routing_table_destroy( &(orta->route) );

#ifdef ORTA_DEBUG
	printf( "orta_destroy: Done.\n" );
#endif

	free( orta );
	*o= NULL;
}


/**
 * orta_recv:
 * 
 * Blocks until data is available on the 'channel' specified.
 *
 * Returns the number of bytes read with the data placed in 'buffer',
 * or -1 if an error occurred.
 **/
int orta_recv( orta_t *m, uint32_t channel, char *buffer, int buflen )
{
	queue_t *channel_queue;
	uint32_t data_len;
	char *tempdata;
	packet_holder_t *ph;

	pthread_mutex_lock( m->data_queues->lock );
	if (list_get( m->data_queues, channel, (void**)&channel_queue ) ) {
		pthread_mutex_unlock( m->data_queues->lock );

		pthread_mutex_lock( channel_queue->lock );
		if ( !channel_queue->length )
			pthread_cond_wait( channel_queue->flag, 
					   channel_queue->lock );

		/* Dequeue and copy packet */
		ph= queue_dequeue( channel_queue );

		data_len= (ph->len<buflen) ? ph->len : buflen;

		memcpy( buffer, ph->data, data_len );

		/* Free memory */
		free(ph->data);
		free(ph);

		pthread_mutex_unlock( channel_queue->lock );

		/*printf( "orta_recv: Recv'd %d bytes.\n", data_len );*/

		return data_len;
	}
	else {
		pthread_mutex_unlock( m->data_queues->lock );
		
		return -1;
	}
}

/**
 * orta_recv:
 * 
 * Blocks until data is available on channel 0.
 *
 * Returns the number of bytes read with the data placed in 'buffer',
 * or -1 if an error occurred.
 **/
int orta_recv_0(orta_t *m, char *buffer, int buflen)
{
	return orta_recv( m, 0, buffer, buflen );
}



/**
 * tv_gt:
 * Returns t1 < t2 (ie: 1 is less then 2).
 * Obviously based on tv_gt in rtp.c.
 */
static int tv_less( struct timeval t1, struct timeval t2 )
{
	if (t1.tv_sec < t2.tv_sec) {
		return TRUE;
	}
	if (t1.tv_sec > t2.tv_sec) {
		return FALSE;
	}
	assert(t1.tv_sec == t2.tv_sec);
	return t1.tv_usec < t2.tv_usec;
}


/**
 * tv_add:
 *
 * Obviously based on tv_add in rtp.c.
 */
static void tv_add( struct timeval *time, uint32_t offset )
{
	time->tv_usec += offset;
	
	if (time->tv_usec > 1000000) {
		time->tv_sec++;
		time->tv_usec -= 1000000;
	}
}


/**
 * orta_recv_timeout:
 * 
 * Waits the required length of time specified by 'timeout' for data
 * to appear on the channel number identified by 'channel'. If data is
 * present at the time of the call or any other time before timeout,
 * it is copied into 'buffer'. If 'buflen' is less than the size of
 * the returned data, then the rest of the data in that packet will be
 * discarded.
 * 
 * Actual amount of data placed in buffer is returned from this call,
 * or -1 if the operation was not successful (ie: timed out).
 */
int orta_recv_timeout( orta_t *m, uint32_t channel, char *buffer, 
		       int buflen, struct timeval *timeout )
{
	struct timespec wake_time;
	struct timeval now;
	int timedout;

	queue_t *channel_queue;
	uint32_t data_len;
	char *tempdata;
	packet_holder_t *ph;

	/* Get channel */
	pthread_mutex_lock( m->data_queues->lock );
	if (list_get( m->data_queues, channel, (void**)&channel_queue ) ) {
		pthread_mutex_unlock( m->data_queues->lock );

		pthread_mutex_lock( channel_queue->lock );
		if ( !channel_queue->length ) {
			/* Add timeout to current time; pthread call takes 
			 * absolute time, not relative time. */
			gettimeofday( &now, NULL );
			wake_time.tv_sec= now.tv_sec+timeout->tv_sec;
			wake_time.tv_nsec+= (now.tv_usec+timeout->tv_usec)*1000;
			if ( wake_time.tv_nsec > 1000000000 ) {
				wake_time.tv_sec++;
				wake_time.tv_nsec-= 1000000000;
			}

			timedout= pthread_cond_timedwait( channel_queue->flag, 
							  channel_queue->lock, 
							  &wake_time );

			/* Timeout! */
			if ( timedout ) {
				pthread_mutex_unlock( channel_queue->lock );
				return -1;
			}
		}

		/* Something's in the queue; dequeue and copy... */
		ph= queue_dequeue( channel_queue );

		data_len= (ph->len<buflen) ? ph->len : buflen;

		memcpy( buffer, ph->data, data_len );

		/* ... and free up memory */
		free(ph->data);
		free(ph);

		pthread_mutex_unlock( channel_queue->lock );

		return data_len;
	}


	pthread_mutex_unlock( channel_queue->lock );
	return -1;
}


/**
 * orta_send:
 * 
 * Sends `buflen' bytes from buffer into the specified Orta channel,
 * to be redistributed by the Orta overlay.
 * 
 * Returns the amount of data sent.
 **/
int orta_send( orta_t *m, uint32_t channel, char *buffer, int buflen )
{
	/*printf( "orta_send: Sending %d bytes.\n", buflen );*/

	/* FIXME: ttl buried in code */
	return route( m, channel, buffer, buflen, 16 );
}


/**
 * orta_send:
 * 
 * Sends `buflen' bytes from buffer into Orta channel 0, to be
 * redistributed by the Orta overlay.
 * 
 * Returns the amount of data sent.
 **/
int orta_send_0(orta_t *orta, char *buffer, int buflen )
{
	return orta_send( orta, 0, buffer, buflen );
}

/**
 * orta_select:
 * 
 * Waits for time `timeout' for incoming data to appear on a channel,
 * placing the a list of waiting channels of length `count' into
 * `channels'.
 * 
 * Return value: TRUE if data is waiting, FALSE if no data is waiting
 * after the timeout period..
 **/
int orta_select(orta_t *m, struct timeval *timeout, int* channels, int *count )
{
	struct timespec wake_time;
	struct timeval now;

	int* ch_out;

	queue_t *channel_queue;
	list_item_t *temp;

	pthread_mutex_lock( m->data_queues->lock );

	/* Lock down all queues */
	for ( temp= m->data_queues->head; temp != NULL; temp= temp->next ) {
		channel_queue= (queue_t*)temp->data;
		pthread_mutex_lock( channel_queue->lock );
	}

	/* Search for a non-empty queue */
	ch_out= channels;
	*count= 0;
	for ( temp= m->data_queues->head; temp != NULL; temp= temp->next ) {
		channel_queue= (queue_t*)temp->data;
		if ( channel_queue->length ) {
			*ch_out= temp->key;
			ch_out++;
			(*count)++;
		}
	}

	/* Unlock all */
	temp= m->data_queues->tail;
	for ( ; temp != NULL; temp= temp->prev ) {
		channel_queue= (queue_t*)temp->data;
		pthread_mutex_unlock( channel_queue->lock );
	}

	/* If all are empty */
	if ( ch_out == channels ) {
		int timedout;

		/* Add timeout to current time; pthread call takes absolute 
		 * time, not relative time. */
		gettimeofday( &now, NULL );
		wake_time.tv_sec= now.tv_sec+timeout->tv_sec;
		wake_time.tv_nsec+= (now.tv_usec+timeout->tv_usec)*1000;
		if ( wake_time.tv_nsec > 1000000000 ) {
			wake_time.tv_sec++;
			wake_time.tv_nsec-= 1000000000;
		}

		timedout= pthread_cond_timedwait( &(m->data_arrived), 
						  m->data_queues->lock, 
						  &wake_time );

		/* The wait timed out. Unlock things and return 0 */
		if ( timedout ) {
			pthread_mutex_unlock( m->data_queues->lock );
			return 0;
		}

		/* Data arrived somewhere. Look for the queue with data, 
		 * and return that channel number to the calling process */
		ch_out= channels;
		temp= m->data_queues->head;
		for ( ; temp != NULL; temp= temp->next ) {
			channel_queue= (queue_t*)temp->data;
			pthread_mutex_lock( channel_queue->lock );
			if ( channel_queue->length ) {
				*ch_out= temp->key;
				ch_out++;
				(*count)++;
			}
			pthread_mutex_unlock( channel_queue->lock );
		}
	}
	pthread_mutex_unlock( m->data_queues->lock );

	return 1;
}

/**
 * orta_host_addr:
 * Return value: character string containing the network address
 * associated the local host.
 **/
const char *orta_host_addr( )
{
	static char    		 hname[MAXHOSTNAMELEN];
	struct hostent 		*hent;
	struct in_addr  	 iaddr;
	
	if (gethostname(hname, MAXHOSTNAMELEN) != 0) {
		perror( "orta_host_addr" );
		abort();
	}
	hent = gethostbyname(hname);
	if (hent == NULL) {
		fprintf( stderr, "Can't resolve IP address for %s", hname );
		return NULL;
	}
	assert(hent->h_addrtype == AF_INET);
	memcpy(&iaddr.s_addr, hent->h_addr, sizeof(iaddr.s_addr));
	strncpy(hname, inet_ntoa(iaddr), MAXHOSTNAMELEN);
	return (const char*)hname;
}


/**
 * orta_set_update_membership_func
 * 
 * Sets the callback for Orta to call when group membership changes.
 */
void orta_set_update_membership_callback( orta_t *o, void *u_m )
{
	o->update_membership= u_m;
	update_membership_for_app( o );
}
