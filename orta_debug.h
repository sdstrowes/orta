
#ifndef __MESH_DEBUG_
#define __MESH_DEBUG_

#include "orta_t.h"
#include <stdint.h>

void debug_packet( void* packet, int packet_length );

char* print_ip( uint32_t ip );

void print_links( orta_t *orta );

void print_state( orta_t *orta );

#endif

