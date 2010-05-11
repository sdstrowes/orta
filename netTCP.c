#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/time.h>
#include <time.h>

#include <netdb.h> /* For memcpy()? */

#include "netTCP.h"

/**
 * connectTCP:
 * Attempts to connect to `addr', using port `tx_port' for transmission. If 
 * `tx_port' is set to zero, the host OS chooses port.
 * socket descriptor created and relevant struct sockaddr_in are passed back 
 * in `sd' and `addr' respectively.
 * Returns TRUE on success or FALSE otherwise.
 */
int connectTCP( char* dest, int tx_port, int *sd, sockaddr_in_t *addr )
{
	addr->sin_family     = AF_INET;
	addr->sin_port       = htons(tx_port);
	if ( !inet_aton( dest, &(addr->sin_addr)) ) {
		fprintf( stderr, "connectTCP: invalid address: %s", dest );
		return FALSE;
	}

	/* zero off the rest of the struct */
	memset( &(addr->sin_zero), '\0', 8 );

	if ( (*sd= socket( AF_INET, SOCK_STREAM, 0 )) == -1 ) {
		fprintf( stderr, "connectTCP: can't grab send socket.\n" );
		return FALSE;
	}

#ifdef ORTA_DEBUG
	printf( "connectTCP: Socketed.\n" );
#endif

	if ( connect(*sd, (sockaddr_t*)addr, sizeof(sockaddr_t)) == -1 ) {
		fprintf( stderr, "connectTCP: can't connect to %s (%d).\n", 
			 inet_ntoa( addr->sin_addr), tx_port );
		fprintf( stderr, "connectTCP: connect failed, closing socket.\n" );
		close(*sd);
		perror( "connectTCP" );
		return FALSE;
	}
	
#ifdef ORTA_DEBUG
	printf( "connectTCP: Connected.\n" );
#endif

	return TRUE;
}


/**
 * bindTCP:
 * Binds the port indicated and returns a socket descriptor to that port.
 * If the bind operation fails, returns a -1; else, returns the socket 
 * descriptor itself.
 */
int bindTCP( int rx_port )
{
	int yes= 1;
	int sd;

	sockaddr_in_t addr;

	if ( (sd= socket( AF_INET, SOCK_STREAM, 0 )) == -1 ) {
		fprintf( stderr, "listenTCP: can't grab recv socket.\n" );
		return -1;
	}

	/* FIXME: Allow reuse of port. No error checking here. */
	setsockopt( sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) );

#ifdef ORTA_DEBUG
	printf( "bindTCP: Grabbed socket %d\n", sd );fflush( stdout );
#endif

	/* Grab specified recieve port, and connect. */
	addr.sin_family     = AF_INET;
	addr.sin_port       = htons(rx_port);
	addr.sin_addr.s_addr= htonl(INADDR_ANY);
	/* zero the rest of the struct */
	memset( &(addr.sin_zero), '\0', 8 );

#ifdef ORTA_DEBUG
	printf( "bindTCP: binding on socket %d\n", sd );
#endif

	if ( bind( sd, (sockaddr_t*)&addr, sizeof(sockaddr_t) ) == -1 ) {
		fprintf( stderr, "listenTCP: can't bind port %d.\n", 
			 ntohs( addr.sin_port ) );
		return -1;
	}


	return sd;
}

