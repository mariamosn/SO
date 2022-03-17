#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

#define LINE_LEN 257
#define HASHMAP_SIZE 20
#define DEFINE_LEN 1000
#define EXIT_IF 7
#define SKIP 8

typedef struct Node_t {
	struct Node_t *next;
	char *data;
} Node_t;

void print_list(Node_t *head)
{
	Node_t *p;

	printf("Dirs:\n");
	for (p = head; p; p = p->next)
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
	int last, i;

	if (infile == NULL) {
		// *base_dir = strdup(".");
		*base_dir = calloc(2, sizeof(char));
		if (*base_dir)
			strncpy(*base_dir, ".", 1);
	} else {
		last = -1;

		for (i = strlen(infile) - 1; i >= 0 && last == -1; i--) {
			if (infile[i] == '/')
				last = i - 1;
		}
		if (last >= 0) {
			*base_dir = calloc(last + 2, sizeof(char));
			if (*base_dir)
				strncpy(*base_dir, infile, last + 1);
		} else {
			// *base_dir = strdup(".");
			*base_dir = calloc(2, sizeof(char));
			if (*base_dir)
				strncpy(*base_dir, ".", 1);
		}
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
	Node_t *new_dir = malloc(sizeof(Node_t)), *p;

	if (new_dir == NULL) {
		free_hashmap(*h);
		free(*h);
		return 12;
	}

	// new_dir->data = strdup(dir);
	new_dir->data = calloc(strlen(dir) + 1, sizeof(char));
	if (new_dir->data) {
		strncpy(new_dir->data, dir, strlen(dir));
	} else {
		free_hashmap(*h);
		free(*h);
		return 12;
	}
	new_dir->next = NULL;

	if (*dirs == NULL) {
		*dirs = new_dir;
	} else {
		for (p = *dirs; p->next; p = p->next)
			;
		p->next = new_dir;
	}

	return 0;
}

int process_line(char *line, char *base_dir, Node_t *other_dirs, FILE * in,
					FILE * out, Hashmap *h);

int process_include(char *line, char *base_dir, Node_t *other_dirs, FILE *out,
					Hashmap *h)
{
	char *file_to_include_name = line + 10, *path, *file_to_include;
	FILE *file_incl;
	int found, ret;
	Node_t *p;

	// file_to_include = strndup(file_to_include,
	//							strlen(file_to_include) - 2);
	file_to_include = calloc(strlen(file_to_include_name) - 1, sizeof(char));
	if (file_to_include == NULL)
		return 12;
	strncpy(file_to_include, file_to_include_name,
			strlen(file_to_include_name) - 2);

	path = calloc(LINE_LEN, sizeof(char));
	if (!path) {
		free(file_to_include);
		return 12;
	}
	strcpy(path, base_dir);
	strcat(path, "/");
	strcat(path, file_to_include);

	file_incl = fopen(path, "r");
	if (!file_incl) {
		found = 0;

		for (p = other_dirs; p && !found; p = p->next) {
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
		ret = process_line(line, base_dir, other_dirs, file_incl, out, h);

		if (ret) {
			free(file_to_include);
			free(path);
			fclose(file_incl);
			return ret;
		}
	}

	free(file_to_include);
	free(path);
	fclose(file_incl);

	return 0;
}

void change_line_inplace(char *line, Hashmap *h)
{
	int quote = 0, i, j;
	char new_line[LINE_LEN + 10] = {0}, var_candidate[LINE_LEN], *replace;

	if (!line)
		return;

	for (i = 0; i < (int) strlen(line); i++) {
		if (!((line[i] >= 'a' && line[i] <= 'z') ||
				(line[i] >= 'A' && line[i] <= 'Z') ||
				line[i] == '_') ||
				quote) {
			sprintf(new_line + strlen(new_line), "%c", line[i]);
			if (line[i] == '"')
				quote = 1 - quote;
		} else {
			for (j = i; j < (int) strlen(line); j++) {
				if (!((line[j] >= 'a' && line[j] <= 'z') ||
						(line[j] >= 'A' && line[j] <= 'Z') ||
						(line[j] >= '0' && line[j] <= '9') ||
						line[j] == '_')) {
					var_candidate[j - i] = '\0';
					replace = get(h, var_candidate);

					if (replace)
						sprintf(new_line + strlen(new_line), "%s%c", replace,
								line[j]);
					else
						sprintf(new_line + strlen(new_line), "%s%c",
								var_candidate, line[j]);
					i = j;
					break;
				}
				var_candidate[j - i] = line[j];
			}
			if (j == (int) strlen(line)) {
				var_candidate[j - i] = '\0';
				replace = get(h, var_candidate);

				if (replace)
					sprintf(new_line + strlen(new_line), "%s", replace);
				else
					sprintf(new_line + strlen(new_line), "%s", var_candidate);
				break;
			}
		}
	}
	strcpy(line, new_line);
}

int process_define(char *line, Hashmap *h, FILE *in)
{
	char *p, *key, *value, *val = calloc(DEFINE_LEN, sizeof(char));
	int done, ret;

	if (!val)
		return 12;

	p = strtok(line, " ");
	p = strtok(NULL, "\n ");
	key = calloc(strlen(p) + 1, sizeof(char));
	// key = strdup(strtok(NULL, "\n "));
	if (!key) {
		free(val);
		return 12;
	}
	strncpy(key, p, strlen(p));

	value = strtok(NULL, "\n");
	if (value && value[strlen(value) - 1] == '\\') {
		done = 0;
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
	change_line_inplace(value, h);
	ret = put(h, key, value);

	free(val);
	free(key);
	return ret;
}

void process_undef(char *line, Hashmap *h)
{
	char *p, *key;

	p = strtok(line, " ");
	key = strtok(NULL, "\n ");

	remove_ht_entry(h, key);
}

void skip_lines(char *line, FILE *in)
{
	int inner_ifs = 0;

	while (fgets(line, LINE_LEN, in)) {
		if (strncmp(line, "#if", 3) == 0) {
			inner_ifs++;
		} else if (strncmp(line, "#endif", 6) == 0) {
			if (inner_ifs)
				inner_ifs--;
			else
				break;
		}
	}
}

int process_if(char *line, Hashmap *h, FILE *in, FILE *out,
				char *base_dir, Node_t *other_dirs)
{
	int cond = 0, res;
	char *symbol, *value;

	// #ifdef
	if (line[3] == 'd') {
		symbol = strtok(line, " ");
		symbol = strtok(NULL, "\n ");
		cond = contains(h, symbol);

	// #ifndef
	} else if (line[3] == 'n') {
		symbol = strtok(line, " ");
		symbol = strtok(NULL, "\n ");
		cond = 1 - contains(h, symbol);

	// #if
	} else {
		symbol = strtok(line, " ");
		symbol = strtok(NULL, "\n ");
		value = get(h, symbol);

		if ((value && strcmp(value, "0") == 0) || strcmp(symbol, "0") == 0)
			cond = 0;
		else
			cond = 1;
	}

	if (!cond) {
		while (fgets(line, LINE_LEN, in)) {
			if (strncmp(line, "#elif", 5) == 0)
				return process_if(line + 2, h, in, out, base_dir, other_dirs);
			else if (strncmp(line, "#else", 5) == 0)
				break;
			else if (strncmp(line, "#endif", 6) == 0)
				return 0;
		}
	}

	res = 0;

	while (fgets(line, LINE_LEN, in) && res != EXIT_IF) {
		res = process_line(line, base_dir, other_dirs, in, out, h);
		if (res && res != EXIT_IF && res != SKIP)
			return res;
		else if (res == EXIT_IF)
			return 0;
		else if (res == SKIP)
			skip_lines(line, in);
	}

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

void change_line(char *line, FILE *out, Hashmap *h)
{
	int quote = 0, i, j;
	char var_candidate[LINE_LEN], *replace;

	for (i = 0; i < (int) strlen(line); i++) {
		if (!((line[i] >= 'a' && line[i] <= 'z') ||
				(line[i] >= 'A' && line[i] <= 'Z') ||
				line[i] == '_') ||
				quote) {
			fprintf(out, "%c", line[i]);
			if (line[i] == '"')
				quote = 1 - quote;
		} else {
			for (j = i; j < (int) strlen(line); j++) {
				if (!((line[j] >= 'a' && line[j] <= 'z') ||
						(line[j] >= 'A' && line[j] <= 'Z') ||
						(line[j] >= '0' && line[j] <= '9') ||
						line[j] == '_')) {
					var_candidate[j - i] = '\0';
					replace = get(h, var_candidate);

					if (replace)
						fprintf(out, "%s%c", replace, line[j]);
					else
						fprintf(out, "%s%c", var_candidate, line[j]);
					i = j;
					break;
				}
				var_candidate[j - i] = line[j];
			}
		}
	}
}

int process_line(char *line, char *base_dir, Node_t *other_dirs,
					FILE *in, FILE *out, Hashmap *h)
{
	if (line[0] == '#') {
		// #include
		if (line[1] == 'i' && line[2] == 'n' && line[9] == '"')
			return process_include(line, base_dir, other_dirs, out, h);

		// #define
		else if (line[1] == 'd')
			return process_define(line, h, in);

		// #undef
		else if (line[1] == 'u')
			process_undef(line, h);

		// if
		else if (line[1] == 'i' && line[2] == 'f')
			return process_if(line, h, in, out, base_dir, other_dirs);

		// #endif
		else if (line[1] == 'e' && line[2] == 'n')
			return EXIT_IF;

		// #else
		else if (line[1] == 'e' && line[2] == 'l')
			return SKIP;

	} else {
		change_line(line, out, h);
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
	char *infile = NULL, *outfile = NULL, *base_dir;
	Node_t *other_dirs = NULL, *p, *crt;
	int i, ret;

	if (setup_hashmap(&h))
		return 12;

	// dealing with args
	for (i = 1; i < argc; i++) {
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
	ret = preprocess(infile, outfile, h, base_dir, other_dirs);

	free_hashmap(h);
	free(h);
	free(base_dir);

	for (p = other_dirs; p; ) {
		crt = p;
		p = p->next;
		free(crt->data);
		free(crt);
	}

	return ret;
}
