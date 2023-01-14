#include "linkedlist.h"

#include <stdlib.h>
#include <stdio.h>

node_linkedlist_t *new_node_list(void *data)
{
    node_linkedlist_t *new_node;

    new_node = (node_linkedlist_t *)malloc(sizeof(node_linkedlist_t));
    if (new_node == NULL)
        return NULL;

    new_node->data = data;
    new_node->next = NULL;

    return new_node;
}

int insert_list(node_linkedlist_t **list, void *data)
{
    node_linkedlist_t *node = new_node_list(data), *p;

    if (node == NULL)
        return -1;

    if (*list == NULL) {
        *list = node;
        return 0;
    }

    for (p = *list; p->next; p = p->next);
    p->next = node;

    return 0;
}

int insert_list_front(node_linkedlist_t **list, void *data)
{
    node_linkedlist_t *node = new_node_list(data);

    if (node == NULL)
        return -1;

    node->next = *list;
    *list = node;

    return 0;
}

node_linkedlist_t *delete_list(node_linkedlist_t **list, void *data)
{
    node_linkedlist_t *p = *list;

    if (p->data == data) {
        *list = p->next;
        return p;
    }

    for ( ; p->next; p = p->next) {
        if (p->next->data == data) {
            node_linkedlist_t *to_be_deleted = p->next;
            p->next = p->next->next;
            return to_be_deleted;
        }
    }

    return NULL;
}

node_linkedlist_t *pop_list(node_linkedlist_t **list)
{
    node_linkedlist_t *p = *list;

    if (p)
        *list = p->next;

    return p;
}

int empty_list(node_linkedlist_t *list)
{
    return list == NULL;
}
