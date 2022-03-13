#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

#define LINE_LEN 257

typedef struct Node_t {
	struct Node_t *next;
	char *data;
} Node_t;

void print_list(Node_t *head)
{
	printf("Dirs:\n");
	for (Node_t *p = head; p; p = p->next)
		printf("%s\n", p->data);
}

int setup_hashmap(Hashmap **h)
{
	*h = malloc(sizeof(Hashmap));
	if (*h == NULL)
		return 12;
	if (init_hashmap(*h, 20)) {
		free(*h);
		return 12;
	}
	return 0;
}

int add_arg_define(char *argv[], Hashmap *h, int *i)
{
	char *symbol, *mapping, *p;
	char *str;

	if (strlen(argv[*i]) == 2) {
		*i = *i + 1;
		str = argv[*i];
	} else {
		str = argv[*i] + 2;
	}

	p = strtok(str, "=");
	symbol = p;
	// printf("---%s---\n", symbol);

	p = strtok(NULL, "=");
	if (p != NULL)
		mapping = p;
	else
		mapping = "";

	// printf("+++%s+++\n", mapping);

	if (symbol && put(h, symbol, mapping))
		return 12;

	return 0;
}

int add_arg_outfile(char **outfile, char *argv[], int *i)
{
	if (*outfile != NULL)
		return -1; // TODO

	if (strlen(argv[*i]) == 2) {
		*i = *i + 1;
		*outfile = argv[*i];
	} else {
		*outfile = argv[*i] + 2;
	}
	return 0;
}

int setup_base_dir(char **base_dir, Hashmap **h, char *infile)
{
	if (infile == NULL) {
		*base_dir = strdup(".");
	} else {
		int last = -1;

		for (int i = strlen(infile) - 1; i >= 0 && last == -1; i--) {
			if (infile[i] == '/')
				last = i - 1;
		}
		if (last >= 0)
			*base_dir = strndup(infile, last + 1);
		else
			*base_dir = strdup(".");
	}

	if (*base_dir == NULL) {
		free_hashmap(*h);
		free(*h);
		return 12;
	}
	return 0;
}

int add_other_dir(char *dir, Node_t **dirs, Hashmap **h)
{
	Node_t *new_dir = malloc(sizeof(Node_t));

	if (new_dir == NULL) {
		free_hashmap(*h);
		free(*h);
		return 12;
	}

	new_dir->data = strdup(dir);
	new_dir->next = NULL;

	if (*dirs == NULL) {
		*dirs = new_dir;
	} else {
		Node_t *p;

		for (p = *dirs; p->next; p = p->next)
			;
		p->next = new_dir;
	}

	return 0;
}

int preprocess(char *infile, char *outfile, Hashmap *h,
				char *base_dir, Node_t *other_dirs)
{
	FILE *in, *out;
	char *line;
	ssize_t read;
	size_t len = LINE_LEN;

	line = calloc(LINE_LEN, sizeof(char));
	if (line == NULL)
		return 12;
	if (infile)
		in = fopen(infile, "r");
	else
		in = stdin;
	if (!in) {
		free(line);
		return -1;
	}

	if (outfile)
		out = fopen(outfile, "w");
	else
		out = stdout;
	if (!out) {
		free(line);
		if (in != stdin)
			fclose(in);
		return -1;
	}

	while ((read = getline(&line, &len, in)) != -1)
		fprintf(out, "%s", line);

	free(line);
	if (in != stdin)
		fclose(in);
	if (out != stdout)
		fclose(out);
	return 0;
}

int main(int argc, char *argv[])
{
	Hashmap *h;

	if (setup_hashmap(&h))
		return 12;

	char *infile = NULL, *outfile = NULL, *base_dir;
	Node_t *other_dirs = NULL;

	// dealing with args
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			if (infile == NULL)
				infile = argv[i];
			else if (outfile == NULL)
				outfile = argv[i];
			else
				return -1; // TODO

		} else if (argv[i][1] == 'D') {
			if (add_arg_define(argv, h, &i)) {
				free_hashmap(h);
				free(h);
				return 12;
			}

		} else if (argv[i][1] == 'I') {
			if (strlen(argv[i]) == 2) {
				i++;
				add_other_dir(argv[i], &other_dirs, &h);
			} else {
				add_other_dir(argv[i] + 2, &other_dirs, &h);
			}

		} else if (argv[i][1] == 'o') {
			if (add_arg_outfile(&outfile, argv, &i))
				return -1; // TODO

		} else {
			return -1; // TODO

		}
	}

	if (setup_base_dir(&base_dir, &h, infile)) {
		free_hashmap(h);
		free(h);
		return 12;
	}

	// printf("---%s---\n", infile);
	// printf("+++%s+++\n", outfile);

	// print_all(h);

	// print_list(other_dirs);

	// TODO: redenumeste
	int ret = preprocess(infile, outfile, h, base_dir, other_dirs);

	if (ret)
		return ret;

	free_hashmap(h);
	free(h);
	free(base_dir);
	for (Node_t *p = other_dirs; p; ) {
		Node_t *crt = p;

		p = p->next;
		free(crt->data);
		free(crt);
	}

	return 0;
}
