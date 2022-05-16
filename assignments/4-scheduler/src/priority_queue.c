#include "priority_queue.h"

node_t *new_node(void *data, int priority)
{
    node_t *new_node;

    new_node = (node_t *)malloc(sizeof(node_t));
    if (new_node == NULL)
        return NULL;

    new_node->data = data;
    new_node->priority = priority;
    new_node->next = NULL;

    return new_node;
}

int push(node_t **pq, void *data, int priority)
{
    node_t *head = (*pq);
    node_t *node = new_node(data, priority);

    if (node == NULL)
        return -1;

    if ((*pq)->priority < priority) {
        node->next = *pq;
        *pq = node;
        return 0;
    }
    
    while (head->next &&
            head->next->priority > priority) {
        head = head->next;
    }

    node->next = head->next;
    head->next = node;
    return 0;
}

node_t *pop(node_t **pq)
{
    node_t *to_be_deleted = *pq;

    (*pq) = (*pq)->next;
    return to_be_deleted;
}

void *peek(node_t *pq)
{
    return (pq)->data;
}

int empty(node_t *pq)
{
    return pq == NULL;
}
