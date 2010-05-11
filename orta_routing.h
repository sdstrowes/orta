
#ifndef __MESH_ROUTING_
#define __MESH_ROUTING_

#include <stdint.h>

#include "routing_table.h"
/*#include "orta.h"*/
#include "common_defs.h"

#include "orta_t.h"

/**
 *
 */
int routing_add_route( route_table_t *routes, 
		       uint32_t source, 
		       uint32_t outbound_link );


/**
 * Removes a given link from 
 */
int routing_drop_route( route_table_t *routes, 
			uint32_t source, 
			uint32_t outbound_link );


/**
 * Constructs a routing table from the information found in "links"
 */
int routing_build_table( orta_t *m );


#endif
