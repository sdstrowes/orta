#ifndef __DIJKSTRA_H_
#define __DIJKSTRA_H_

#include "linked_list.h"
#include "links.h"

#include "common_defs.h" /* For TRUE/FALSE def. */


void dijkstra_print_links( links_t *links );

void shortest_paths( uint32_t start_node, links_t *links, linked_list_t *out );

void shortest_path_graph( uint32_t start_node, links_t *links, links_t *out );

#endif
