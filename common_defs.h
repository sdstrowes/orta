#ifndef __COMMON_DEFS_
#define __COMMON_DEFS_

#define TCP_PORT 5100


#define TRUE 1
#define FALSE 0


/* MIN_WEIGHT is simply the minimum value with which a link should be
 * marked; links weighing less than this in reality should advertise
 * MIN_WEIGHT instead. This introduces some stability for peers close
 * together on a LAN. */
#define MIN_WEIGHT 3000

/* Large value defined for Infinity; large, definitely positive
 * integer. Larger than any reasonable latency is likely to be. */
#define INFINITY 0x0fffffff


/* FIXME: Pretty arbitrary */
#define DEFAULT_DIST 2000000


/* Allow a defined error margin to help stop peers repeatedly
 * adding/removing the same link. */
#define THRESHOLD_BUFFER 0.6

/* Keyboard savers */
typedef struct sockaddr_in sockaddr_in_t;
typedef struct sockaddr sockaddr_t;

#endif
