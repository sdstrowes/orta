#ifndef __DATA_PACKET_TYPES
#define __DATA_PACKET_TYPES

#include "orta_control_packets.h"

/**
 * packet_header acts as a convenient (ie: readable) way of grabbing the type 
 * of incoming control packet; the packet can then be cast to the appropriate 
 * type afterward.
 */
typedef struct _data_packet_header
{
	enum control_type type;
	uint16_t channel;
} data_packet_header_t;


typedef struct _data_header
{
	data_packet_header_t header;
	uint32_t source;
	uint32_t ttl;
	uint32_t datalen;
} data_header_t;

typedef struct _data_packet
{
	data_packet_header_t header;
	uint32_t source;
	uint32_t ttl;
	uint32_t datalen;
	char data;
} data_packet_t;

#endif
