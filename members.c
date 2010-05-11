
#include "members.h"
#include "common_defs.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * members_init:
 * 
 */
int members_init( members_list_t **list )
{
	members_list_t *l= (members_list_t*)malloc(sizeof(members_list_t));

	if ( l ) {
		l->head= NULL;
		l->length= 0;

		l->lock= (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init( l->lock, NULL );

		*list= l;
		return TRUE;
	}
	return FALSE;
}

/**
 * members_add:
 * 
 * 
 */
int members_add( members_list_t *list, uint32_t member )
{
	member_t *item= (member_t*)malloc(sizeof(member_t));
	member_t *tempitem, *previtem;

	item->member= member;
	item->seq= 0;
	gettimeofday( &(item->tv), NULL );

	/* Empty */
	if ( !list->length ) {
		item->next= NULL;

		list->length= 1;
		list->head= item;

		return TRUE;
	}

	if ( member < (list->head->member) ) {
		item->next= list->head;

		list->length++;
		list->head= item;

		return TRUE;
	}

	tempitem= list->head;
	previtem= NULL;
	while ( tempitem != NULL && member > (tempitem->member) ) {
		previtem= tempitem;
		tempitem= tempitem->next;
	}

	if ( tempitem == NULL ) {
		item->next= NULL;
		item->member=  member;

		list->length++;
		previtem->next= item;
		return TRUE;
	}

	if ( member == (tempitem->member) ) {
		free( item );
		return FALSE;
	}

	item->next= tempitem;
	item->member= member;

	previtem->next= item;
	item->next= tempitem;
	list->length++;

	return TRUE;
}


/**
 * member_update:
 * 
 * 
 */
int member_update( member_t *member, uint32_t seq )
{
#ifdef ORTA_DEBUG
	printf( "#* seq:\t\t%d\n", seq );
	printf( "#* member->seq:\t%d\n", member->seq );
#endif

	/* Member was found, but sequence number is old or current. No
	 * update, return FALSE. */
	if ( seq <= member->seq )
		return FALSE;

	/* Update member sequence number and timestamp; return TRUE */
	member->seq= seq;
	gettimeofday( &(member->tv), NULL );
	return TRUE;
}


/**
 * members_update:
 *
 * 
 */
int members_update( members_list_t *list, uint32_t member, uint32_t seq )
{
	member_t *tmpitem;

	tmpitem= list->head;
	while ( tmpitem != NULL && tmpitem->member != member )
		tmpitem= tmpitem->next;

	/* Member was not found in the list */
	if ( tmpitem == NULL )
		return FALSE;

	return member_update( tmpitem, seq );
}


/**
 * members_rm:
 * 
 * 
 */
int members_rm( members_list_t *list, uint32_t member )
{
	member_t *tempitem, *previtem;

	tempitem= list->head;
	previtem= NULL;
	while ( tempitem != NULL && member != (tempitem->member) ) {
		previtem= tempitem;
		tempitem= tempitem->next;
	}

	if ( tempitem == NULL )
		return FALSE;

	if ( previtem == NULL )
		list->head= tempitem->next;
	else
		previtem->next= tempitem->next;

	list->length--;
	free( tempitem );

	return TRUE;
}


/**
 * members_get:
 * 
 * 
 */
member_t *members_get( members_list_t *list, uint32_t member_ip )
{
	member_t *tempitem;
	
	tempitem= list->head;
	while ( tempitem != NULL && member_ip != (tempitem->member) )
		tempitem= tempitem->next;

	return tempitem;
}


/**
 * members_contains:
 * 
 * 
 */
int members_contains( members_list_t *list, uint32_t member )
{
	member_t *tempitem;

	if ( !list->length )
		return FALSE;

	tempitem= list->head;
	while ( tempitem != NULL && member != (tempitem->member) ) {
		tempitem= tempitem->next;
	}
	if ( tempitem == NULL )
		return FALSE;

	return TRUE;
}


/**
 * members_clear:
 * 
 * 
 */
int members_clear( members_list_t *m )
{
	member_t *temp, *member= m->head;

	while ( member != NULL ) {
		temp= member->next;
		free( member );
		member= temp;
	}

	m->length= 0;
	m->head= NULL;
}

int members_length( members_list_t *list )
{
	return list->length;
}

/**
 * Places the IP numbers of 
 */
void members_ip_nums( members_list_t *list, int *array )
{
	int i= 0;
	member_t *member;

	member= list->head;
	while ( member != NULL ) {
		array[i]= member->member;

		member= member->next;
		i++;
	}
}


/**
 * members_destroy:
 * 
 * 
 */
int members_destroy( members_list_t **m )
{
	members_list_t *m_list= *m;

	members_clear( m_list );

	pthread_mutex_unlock( m_list->lock );
	pthread_mutex_destroy( m_list->lock );
	free( m_list->lock );
	free( m_list );

	*m= NULL;

	return TRUE;
}
