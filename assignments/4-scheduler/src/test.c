#include <stdio.h>
#include <stdlib.h>
#include "priority_queue.h"

int main()
{
    int v[4];
    v[0] = 1;
    v[1] = 2;
    v[2] = 3;
    v[3] = 4;

    node_t *list = new_node(&v[0], 1);

    push(&list, &v[2], 3);
    printf("%d\n", *((int *)peek(list)));

    push(&list, &v[3], 4);
    printf("%d\n", *((int *)peek(list)));

    push(&list, &v[1], 2);
    printf("%d\n", *((int *)peek(list)));

    while (!empty(list)) {
        printf("*%d ", *((int *)peek(list)));
        pop(&list);
    }
    printf("\n");

    return 0;
}