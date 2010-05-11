
#ifndef __ORTA_T_
#define __ORTA_T_

#include <stdlib.h>
#include <pthread.h>

#include "links.h"
#include "members.h"
#include "neighbours.h"
#include "routing_table.h"

struct orta
{
	/* Alive marker. Set false when shutting down */
	int alive;
	/* Connected marker. Set to false when not connected. */
	int connected;

	/* Thread descriptors */
	pthread_t ctrl_recv_thread;
	pthread_t ctrl_udp_thread;
	pthread_t ctrl_sched_thread;

	/* List containing links */
	links_t *links;
	/* List of known members */
	members_list_t *members;
	/* Table storing socket descriptor:info (FIXME) pairs */
	neighbours_list_t *neighbours;
	/* Table storing data about ping times to overlay neighbours */
	route_table_t *route;

	/* Local IP addr */
	uint32_t local_ip;
	/* Local sequence number */
	uint32_t local_seq;

	/* List of queues, indexed by channel number */
	linked_list_t *data_queues;
	pthread_cond_t data_arrived;


	/* Read set for connected TCP ports. */
	int fdmax;
	fd_set *master;

	/* UDP socket for sending/recieving */
	int udp_sd;
	uint16_t udp_rx_port;
	uint16_t udp_tx_port;

	/* Callback to inform application of current group
	   membership */
	void (*update_membership) (int* ip_list, int len);
};


#endif
