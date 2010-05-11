
#ifndef __ORTA_
#define __ORTA_

#include <sys/time.h>
#include <time.h>
#include <stdint.h>

struct orta;
typedef struct orta orta_t;


/**
 * orta_addr_valid:
 * addr: string representation of IPv4 network address.
 *
 * Returns TRUE if addr is valid, FALSE otherwise.
 **/
int orta_addr_valid( const char *addr );


/**
 * orta_host_addr:
 * 
 * Return value: character string containing network address of local
 * machine.
 **/
const char *orta_host_addr( );


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
orta_t* orta_init(uint16_t udp_rx_port, uint16_t udp_tx_port, int ttl);


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
		     uint16_t udp_tx_port, int ttl);


/**
 * orta_connect: 
 * 
 * Attempts to connect to a peer already in the peer-group using
 * `addr', which should be a character string containing an IPv4
 * network address.
 */
int orta_connect( orta_t *o, const char *dest );


/**
 * orta_register_channel:
 * 
 * Registers a new data channel at this host for the purposes of
 * recieving UDP data on that particular channel. Channels can be seen
 * as an emulation of system ports.
 */
int orta_register_channel( orta_t *o, uint32_t channel );


/**
 * orta_disconnect:
 * 
 * Closes all connections into peer-group cleanly, and clears state
 * held within the orta_t struct..
 */
int orta_disconnect( orta_t *o );


/**
 * orta_destroy:
 * 
 *  Destroys the state used by `orta' and frees up any occupied
 *  memory.
 **/
void orta_destroy( orta_t **o );


/**
 * orta_recv:
 * 
 * Blocks until data is available on the 'channel' specified.
 *
 * Returns the number of bytes read with the data placed in 'buffer',
 * or -1 if an error occurred.
 **/
int orta_recv( orta_t *o, uint32_t channel, char *buffer, int buflen );

/**
 * orta_recv:
 * 
 * Blocks until data is available on channel 0.
 *
 * Returns the number of bytes read with the data placed in 'buffer',
 * or -1 if an error occurred.
 **/
int orta_recv_0( orta_t *o, char *buffer, int buflen );

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
int orta_recv_timeout( orta_t *o, uint32_t channel, char *buffer, 
		       int buflen, struct timeval *timeout );


/**
 * orta_send:
 * 
 * Sends `buflen' bytes from buffer into the specified Orta channel,
 * to be redistributed by the Orta overlay.
 * 
 * Returns the amount of data sent.
 **/
int orta_send( orta_t *o, uint32_t channel, char *buffer, int buflen );

/**
 * orta_send:
 * 
 * Sends `buflen' bytes from buffer into Orta channel 0, to be
 * redistributed by the Orta overlay.
 * 
 * Returns the amount of data sent.
 **/
int orta_send_0( orta_t *o, char *buffer, int buflen );



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
int orta_select( orta_t *o, struct timeval *timeout, int* channels, int *count );


/**
 * orta_set_update_membership_func
 * 
 * Sets the callback for Orta to call when group membership changes.
 */
void orta_set_update_membership_callback( orta_t *o, void *u_m );

#endif
