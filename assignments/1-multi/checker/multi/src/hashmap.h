#ifndef __HASHMAP_H
#define __HASHMAP_H

typedef struct Pair {
    char *key;
    char *value;
} Pair;

typedef struct Node {
    struct Node *next;
    Pair *data;
} Node;

typedef struct LinkedList {
    Node *head;
} LinkedList;

typedef struct Hashmap {
    LinkedList *buckets;
    int hmax;
} Hashmap;

int init_hashmap(Hashmap *h, int hmax);

int put(Hashmap *h, char *key, char *value);

char* get(Hashmap *h, char *key);

int contains(Hashmap *h, char *key);

void remove_ht_entry(Hashmap *h, char *key);

void free_hashmap(Hashmap *h);

void print_all(Hashmap *h);

#endif
