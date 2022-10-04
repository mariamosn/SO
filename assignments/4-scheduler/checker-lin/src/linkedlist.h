/*
 * Maria Moșneag
 * 333CA
 * Tema 4 SO
 */

#ifndef LINKEDLIST_H_
#define LINKEDLIST_H_

#include <stdio.h>
#include <stdlib.h>

typedef struct node_linkedlist {
	void *data;
	struct node_linkedlist *next;
} node_linkedlist_t;

node_linkedlist_t *new_node_list(void *data);

int insert_list(node_linkedlist_t **list, void *data);

int insert_list_front(node_linkedlist_t **list, void *data);

node_linkedlist_t *delete_list(node_linkedlist_t **list, void *data);

node_linkedlist_t *pop_list(node_linkedlist_t **list);

int empty_list(node_linkedlist_t *list);

#endif /* LINKEDLIST_H_ */
