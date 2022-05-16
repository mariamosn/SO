#ifndef PQ_H_
#define PQ_H_

#include <stdio.h>
#include <stdlib.h>

typedef struct node {
    void *data;
    int priority;
    struct node *next;
} node_t;

node_t *new_node(void *data, int priority);

int push(node_t **pq, void *data, int priority);

node_t *pop(node_t **pq);

void *peek(node_t *pq);

int empty(node_t *pq);

#endif /* PQ_H_ */
