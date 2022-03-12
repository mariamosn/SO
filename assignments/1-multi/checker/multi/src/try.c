#include <stdio.h>
#include <stdlib.h>

#include "hashmap.h"

int main() {
    Hashmap *h;
    h = malloc(sizeof(Hashmap));
    if (h == NULL) {
        return 12;
    }

    printf("%d\n", init_hashmap(h, 20));
    puts("Done1");

    print_all(h);
    puts("done2");

    for (int i = 1; i <= 2; i++) {
        char *k, *v;
        size_t bufsize = 32;
        k = malloc(bufsize * sizeof(char));
        v = malloc(bufsize * sizeof(char));

        printf("Key:\n");
        scanf("%s", k);

        printf("Value:\n");
        scanf("%s", v);

        put(h, k, v);
    }
    print_all(h);
    puts("done3");

    printf("---%s---\n", get(h, "pizza"));
    printf("---%s---\n", get(h, "pancake"));
    printf("%d\n", contains(h, "hidden"));

    free_hashmap(h);
    free(h);
    
    print_all(h);

    return 0;
}