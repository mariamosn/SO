#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

#define LINE_LEN 257
#define HASHMAP_SIZE 20
#define DEFINE_LEN 1000

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
	if (init_hashmap(*h, HASHMAP_SIZE)) {
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

	p = strtok(NULL, "=");
	if (p != NULL)
		mapping = p;
	else
		mapping = "";

	if (symbol && put(h, symbol, mapping))
		return 12;

	return 0;
}

int add_arg_outfile(char **outfile, char *argv[], int *i)
{
	if (*outfile != NULL)
		return -1;

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

int process_include(char *line, char *base_dir, Node_t *other_dirs, FILE *out)
{
	char *file_to_include = line + 10;

	file_to_include = strndup(file_to_include,
								strlen(file_to_include) - 2);
	if (file_to_include == NULL)
		return 12;

	FILE *file_incl;
	char *path = calloc(LINE_LEN, sizeof(char));

	if (!path) {
		free(file_to_include);
		return 12;
	}
	strcpy(path, base_dir);
	strcat(path, "/");
	strcat(path, file_to_include);

	file_incl = fopen(path, "r");
	if (!file_incl) {
		int found = 0;

		for (Node_t *p = other_dirs; p && !found; p = p->next) {
			strcpy(path, p->data);
			strcat(path, "/");
			strcat(path, file_to_include);

			file_incl = fopen(path, "r");
			if (file_incl)
				found = 1;
		}
		if (!found) {
			free(file_to_include);
			free(path);
			return -1;
		}
	}

	while (fgets(line, LINE_LEN, file_incl)) {
		// TODO : si astea trebuie preprocesate cumva
		fprintf(out, "%s", line);
	}

	free(file_to_include);
	free(path);
	fclose(file_incl);

	return 0;
}

int process_define(char *line, Hashmap *h, FILE *in)
{
	char *p, *key, *value, *val = calloc(DEFINE_LEN, sizeof(char));

	if (!val)
		return 12;

	p = strtok(line, " ");
	key = strdup(strtok(NULL, " "));
	if (!key) {
		free(val);
		return 12;
	}

	value = strtok(NULL, "\n");
	if (value[strlen(value) - 1] == '\\') {
		int done = 0;

		strcpy(val, value);
		val[strlen(val) - 1] = '\0';

		while (!done && fgets(line, LINE_LEN, in)) {
			for (p = line; *p == ' '; p++)
				;
			if (p[strlen(p) - 2] == '\\') {
				p[strlen(p) - 2] = ' ';
				p[strlen(p) - 1] = '\0';
			} else {
				p[strlen(p) - 1] = '\0';
				done = 1;
			}
			strcat(val, p);
		}

		value = val;
	}

	int ret = put(h, key, value);

	free(val);
	free(key);
	return ret;
}

void process_undef(char *line, Hashmap *h, FILE *in)
{
	ssize_t read;
	size_t len = LINE_LEN;

	char *p, *key;

	p = strtok(line, " ");
	key = strtok(NULL, "\n ");

	remove_ht_entry(h, key);
}

int process_if(char *line, Hashmap *h, FILE *in, FILE *out)
{
	int cond = 0;

	// #ifdef
	if (line[3] == 'd') {
		char *symbol;

		symbol = strtok(line, " ");
		symbol = strtok(NULL, "\n ");
		cond = contains(h, symbol);

	// #ifndef
	} else if (line[3] == 'n') {
		char *symbol;

		symbol = strtok(line, " ");
		symbol = strtok(NULL, "\n ");
		cond = 1 - contains(h, symbol);

	// #if
	} else {
		char *symbol, *value;

		symbol = strtok(line, " ");
		symbol = strtok(NULL, "\n ");
		value = get(h, symbol);

		if (value && strcmp(value, "0") == 0)
			cond = 0;
		else
			cond = 1;
	}

	// TODO : use cond to decide what to do next

	return 0;
}

int init_preprocess(char **line, char *infile, char *outfile,
					FILE **in, FILE **out)
{
	*line = calloc(LINE_LEN, sizeof(char));
	if (*line == NULL)
		return 12;
	if (infile)
		*in = fopen(infile, "r");
	else
		*in = stdin;
	if (!(*in)) {
		free(*line);
		return -1;
	}

	if (outfile)
		*out = fopen(outfile, "w");
	else
		*out = stdout;
	if (!(*out)) {
		free(*line);
		if (*in != stdin)
			fclose(*in);
		return -1;
	}

	return 0;
}

int process_line(char *line, char *base_dir, Node_t *other_dirs,
					FILE *in, FILE *out, Hashmap *h)
{
	if (line[0] == '#') {
		// #include
		if (line[1] == 'i' && line[2] == 'n' && line[9] == '"')
			return process_include(line, base_dir, other_dirs, out);

		// #define
		else if (line[1] == 'd')
			return process_define(line, h, in);

		// #undef
		else if (line[1] == 'u')
			process_undef(line, h, in);

		// if
		else if (line[1] == 'i' && line[2] == 'f')
			return process_if(line, h, in, out);

	} else {
		fprintf(out, "%s", line);
	}

	return 0;
}

int preprocess(char *infile, char *outfile, Hashmap *h,
				char *base_dir, Node_t *other_dirs)
{
	FILE *in, *out;
	char *line;

	int ret = init_preprocess(&line, infile, outfile, &in, &out);

	if (ret)
		return ret;

	while (fgets(line, LINE_LEN, in)) {
		ret = process_line(line, base_dir, other_dirs, in, out, h);

		if (ret) {
			free(line);
			fclose(in);
			fclose(out);
			return ret;
		}
	}

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
				return -1;

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
				return -1;

		} else {
			return -1;
		}
	}

	if (setup_base_dir(&base_dir, &h, infile)) {
		free_hashmap(h);
		free(h);
		return 12;
	}

	// TODO: redenumeste
	int ret = preprocess(infile, outfile, h, base_dir, other_dirs);

	free_hashmap(h);
	free(h);
	free(base_dir);

	for (Node_t *p = other_dirs; p; ) {
		Node_t *crt = p;

		p = p->next;
		free(crt->data);
		free(crt);
	}

	return ret;
}
