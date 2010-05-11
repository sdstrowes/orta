
#ifndef __netTCP
#define __netTCP


#include "common_defs.h"


/**
 * connectTCP:
 * Attempts to connect to `addr', using port `tx_port' for transmission. If 
 * `tx_port' is set to zero, the host OS chooses port.
 * socket descriptor created and relevant struct sockaddr_in are passed back 
 * in `sd' and `addr' respectively.
 * Returns TRUE on success or FALSE otherwise.
 */
int connectTCP( char* dest, int tx_port, int *sd, sockaddr_in_t *addr );


/**
 * bindTCP:
 * Binds the port indicated and returns a socket descriptor to that port.
 * If the bind operation fails, returns a -1; else, returns the socket 
 * descriptor itself.
 */
int bindTCP( int rx_port );


#endif
