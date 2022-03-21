/*
 * Maria Moșneag
 * 333CA
 */

#ifndef __HASHMAP_H
#define __HASHMAP_H

#define _CRT_SECURE_NO_WARNINGS

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

/*
 * Funcția alocă memorie pentru un hashmap și îl inițializează.
 * @h = pointer către hashmap
 * @hmax = numărul de bucket-uri
 */
int init_hashmap(Hashmap *h, int hmax);

/*
 * Funcția adaugă o nouă pereche în hashmap.
 * @h = pointer către hashmap
 * @key = cheia
 * @value = valoarea
 * Obs.: În cazul în care cheia există deja în h,
 * valoarea este actualizată.
 */
int put(Hashmap *h, char *key, char *value);

/*
 * Funcția întoarce un pointer la valoarea asociată unei chei,
 * respectiv NULL dacă cheia nu există.
 * @h = pointer către hashmap
 * @key = cheia
 */
char *get(Hashmap *h, char *key);

/*
 * Funcția întoarce 1 dacă cheia există, respectiv 0 în caz contrar.
 * @h = pointer către hashmap
 * @key = cheia
 */
int contains(Hashmap *h, char *key);

/*
 * Funcția șterge perechea ce are o anumită cheie, dacă aceasta există.
 * @h = pointer către hashmap
 * @key = cheia
 */
void remove_ht_entry(Hashmap *h, char *key);

/*
 * Funcția eliberează memoria asociată bucket-urilor și perechilor.
 * @h = pointer către hashmap
 */
void free_hashmap(Hashmap *h);

#endif
