#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

unsigned int hash_function_string(void *a)
{
	/*
	 * Credits: http://www.cse.yorku.ca/~oz/hash.html
	 */
	unsigned char *puchar_a = (unsigned char *) a;
	unsigned long hash = 5381;
	int c;

	while ((c = *puchar_a++))
		hash = ((hash << 5u) + hash) + c; /* hash * 33 + c */

	return hash;
}

int init_hashmap(Hashmap *h, int hmax)
{
	h->buckets = malloc(hmax * sizeof(LinkedList));
	if (!h->buckets)
		return 12;

	for (int i = 0; i < hmax; i++)
		h->buckets[i].head = NULL;

	h->hmax = hmax;
	return 0;
}

int put(Hashmap *h, char *key, char *value)
{
	int index = hash_function_string(key) % h->hmax;
	LinkedList *bucket = &(h->buckets[index]);
	Node *node_before = NULL;

	for (Node *p = bucket->head; p; p = p->next) {
		Pair *entry = p->data;

		if (strcmp(entry->key, key) == 0) {
			free(entry->value);
			entry->value = strdup(value);
			if (entry->value == NULL)
				return 12;
			return 0;
		}
		node_before = p;
	}

	Node *new_node = malloc(sizeof(Node));

	if (new_node == NULL)
		return 12;
	new_node->next = NULL;

	Pair *entry = malloc(sizeof(Pair));

	if (entry == NULL) {
		free(new_node);
		return 12;
	}

	entry->key = strdup(key);
	if (entry->key == NULL) {
		free(new_node);
		free(entry);
		return 12;
	}

	if (value)
		entry->value = strdup(value);
	else
		entry->value = strdup("");
	if (entry->value == NULL) {
		free(new_node);
		free(entry->key);
		free(entry);
		return 12;
	}

	new_node->data = entry;
	if (node_before)
		node_before->next = new_node;
	else
		bucket->head = new_node;

	return 0;
}

char *get(Hashmap *h, char *key)
{
	int index = hash_function_string(key) % h->hmax;
	LinkedList *bucket = &(h->buckets[index]);

	if (bucket->head == NULL)
		return NULL;

	for (Node *p = bucket->head; p; p = p->next) {
		Pair *entry = p->data;

		if (strcmp(entry->key, key) == 0)
			return entry->value;
	}

	return NULL;
}

int contains(Hashmap *h, char *key)
{
	if (get(h, key) == NULL)
		return 0;
	return 1;
}

void remove_ht_entry(Hashmap *h, char *key)
{
	int index = hash_function_string(key) % h->hmax;
	LinkedList *bucket = &(h->buckets[index]);

	if (bucket->head == NULL)
		return;

	if (strcmp(bucket->head->data->key, key) == 0) {
		Node *to_remove = bucket->head;

		bucket->head = to_remove->next;
		free(to_remove->data->key);
		free(to_remove->data->value);
		free(to_remove->data);
		free(to_remove);
		return;
	}

	for (Node *p = bucket->head; p->next; p = p->next) {
		Pair *entry = p->next->data;

		if (strcmp(entry->key, key) == 0) {
			Node *to_remove = p->next;

			p->next = to_remove->next;
			free(to_remove->data->key);
			free(to_remove->data->value);
			free(to_remove->data);
			free(to_remove);
			return;
		}
	}
}

void free_hashmap(Hashmap *h)
{
	for (int i = 0; i < h->hmax; i++) {
		LinkedList *bucket = &(h->buckets[i]);

		for (Node *p = bucket->head; p; ) {
			free(p->data->key);
			free(p->data->value);
			free(p->data);
			Node *prev = p;

			p = p->next;
			free(prev);
		}
	}
	free(h->buckets);
}

void print_all(Hashmap *h)
{
	if (h == NULL) {
		printf("NULL\n");
		return;
	}
	for (int i = 0; i < h->hmax; i++) {
		LinkedList *bucket = &(h->buckets[i]);

		printf("bucket %d: ", i);
		for (Node *p = bucket->head; p; p = p->next)
			printf("(%s, %s) ", p->data->key, p->data->value);
		printf("\n");
	}
}
