#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

typedef struct Node_t {
    struct Node_t *next;
    char *data;
} Node_t;

void print_list(Node_t *head) {
    printf("Dirs:\n");
    for (Node_t *p = head; p; p = p -> next) {
        printf("%s\n", p -> data);
    }
}

int setup_hashmap(Hashmap **h) {
    *h = malloc(sizeof(Hashmap));
    if (*h == NULL) {
        return 12;
    }
    if (init_hashmap(*h, 20)) {
        free(*h);
        return 12;
    }
    return 0;
}

int setup_dirs(Node_t **dirs, Hashmap **h) {
    *dirs = malloc(sizeof(Node_t));
    if (*dirs == NULL) {
        free_hashmap(*h);
        free(*h);
        return 12;
    }
    (*dirs) -> data = strdup(".");
    if ((*dirs) -> data == NULL) {
        free_hashmap(*h);
        free(*h);
        return 12;
    }
    return 0;
}

int add_arg_define(char *argv[], Hashmap *h, int i) {
    char *symbol, *mapping, *p;
    char *str;

    if (strlen(argv[i]) == 2) {
        i++;
        str = argv[i];
    } else {
        str = argv[i] + 2;
    }

    p = strtok(str, "=");
    symbol = p;
    // printf("---%s---\n", symbol);

    p = strtok(NULL, "=");
    if (p != NULL) {
        mapping = p;
    } else {
        mapping = "";
    }

    // printf("+++%s+++\n", mapping);

    if (symbol) {
        if (put(h, symbol, mapping)) {
            return 12;
        }
    }

    return 0;
}

int add_arg_outfile(char **outfile, char *argv[], int *i) {
    if (*outfile != NULL) {
        return -1; // TODO
    }

    if (strlen(argv[*i]) == 2) {
        *i = *i + 1;
        *outfile = argv[*i];
    } else {
        *outfile = argv[*i] + 2;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    Hashmap *h;
    if (setup_hashmap(&h)) {
        return 12;
    }

    char *infile = NULL, *outfile = NULL;
    Node_t *dirs;
    if (setup_dirs(&dirs, &h)) {
        return 12;
    }
    

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            if (infile == NULL) {
                infile = argv[i];
            } else if (outfile == NULL) {
                outfile = argv[i];
            } else {
                return -1; // TODO
            }

        } else if (argv[i][1] == 'D') {
            if (add_arg_define(argv, h, i)) {
                free_hashmap(h);
                free(h);
                return 12;
            }

        } else if (argv[i][1] == 'I') {
            

        } else if (argv[i][1] == 'o') {
            if (add_arg_outfile(&outfile, argv, &i)) {
                return -1; // TODO
            }

        } else {
            return -1; // TODO

        }
    }

    // printf("---%s---\n", infile);
    // printf("+++%s+++\n", outfile);

    // print_all(h);

    free_hashmap(h);
    free(h);

    return 0;
}