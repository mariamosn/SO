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

int process_include(char *line, char *base_dir, Node_t *other_dirs, FILE *out)
{
	char *file_to_include = line + 10;
	size_t len = LINE_LEN;
	ssize_t read;

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

	while ((read = getline(&line, &len, file_incl)) != -1)
		fprintf(out, "%s", line);

	free(file_to_include);
	free(path);
	fclose(file_incl);

	return 0;
}

int process_define(char *line, Hashmap *h, FILE *in)
{
	ssize_t read;
	size_t len = LINE_LEN;

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
		// ceva magie aici
		int done = 0;

		strcpy(val, value);
		val[strlen(val) - 1] = '\0';

		while (!done && (read = getline(&line, &len, in)) != -1) {
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

	while ((read = getline(&line, &len, in)) != -1) {
		if (line[0] == '#') {
			// #include
			if (line[1] == 'i' && line[2] == 'n' && line[9] == '"') {
				int ret = process_include(line, base_dir, other_dirs, out);

				if (ret) {
					free(line);
					fclose(in);
					fclose(out);
					return ret;
				}

			// #define
			} else if (line[1] == 'd') {
				int ret = process_define(line, h, in);

				if (ret) {
					free(line);
					fclose(in);
					fclose(out);
					return 12;
				}

			}
		} else {
			fprintf(out, "%s", line);
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

	// print_all(h);

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
