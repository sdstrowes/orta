
#ifndef __ORTA_DATA_
#define __ORTA_DATA_

#include "orta_t.h"
#include "orta_data_packets.h"

typedef struct _packet_holder
{
	char* data;
	uint32_t len;
} packet_holder_t;


int route( orta_t *orta, uint32_t channel, char *buffer, int buflen, int ttl );

void handle_data( orta_t *orta, data_packet_t *packet );

#endif

