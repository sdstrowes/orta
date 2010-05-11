
#ifndef __ORTA_CTRL_TCP_
#define __ORTA_CTRL_TCP_

#include <stdint.h>
#include <arpa/inet.h>  /* struct sockaddr_in */
/*#include "common_defs.h"*/
#include "orta_t.h"


/**
 * update_membership_for_app:
 *
 * Compiles an array of IP addresses (in 32bit int format) for the
 * application, if and only if the app has registered a callback for
 * this functionality to take place.
 */
void update_membership_for_app( orta_t *o );

/*void evaluate_add_link( orta_t *orta, uint32_t dest_ip, uint32_t weight );*/

void evaluate_drop_link( orta_t *orta );

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
int ctrl_join( orta_t *o, char *dest );

/**
 * ctrl_refresh_dispatcher:
 * Periodically sends refresh packets to overlay neighbours.
 */
void* ctrl_refresh_dispatcher( orta_t *overlay );


/**
 * ctrl_fix_partition:
 * 
 */
int ctrl_fix_partition( orta_t *overlay );

struct connect_data
{
	int sd;
        orta_t *orta;
        unsigned int port;
};


/**
 * ctrl_connection_listener:
 * Handles incoming data on a new port.
 */
void* ctrl_port_listener( void* d );

#endif
