/*
 * Maria Moșneag
 * 333CA
 */

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

#define LINE_LEN 257
#define HASHMAP_SIZE 100
#define DEFINE_LEN 1000
#define EXIT_IF 7
#define SKIP 8

typedef struct Node_t {
	struct Node_t *next;
	char *data;
} Node_t;

/*
 * Funcția inițializează hahsmap-ul.
 */
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

/*
 * Funcția parsează o directivă de tip define (-D[ ]<symbol>[=<mapping>])
 * transmisă prin intermediul argumentelor cu care este rulat executabilul.
 * @argv = lista de argumente
 * @h = hashmap-ul ce conține asocierile specifice directivelor de tip define
 * @i = indexul primului argument din cadrul define-ului curent
 */
int add_arg_define(char *argv[], Hashmap *h, int *i)
{
	char *symbol, *mapping, *p, *str;

	if (strlen(argv[*i]) == 2) {
		*i = *i + 1;
		str = argv[*i];
	} else {
		str = argv[*i] + 2;
	}

	/*
	 * symbol = simbolul ce va fi înlocuit
	 */
	symbol = strtok(str, "=");

	/*
	 * mapping = valoarea cu care va fi înlocuit simbolul
	 */
	p = strtok(NULL, "=");
	if (p != NULL)
		mapping = p;
	else
		mapping = "";

	if (symbol && put(h, symbol, mapping))
		return 12;

	return 0;
}

/*
 * Funcția parsează informațiile referitoare la fișierul de output, transmise
 * ca argumente (-o[ ]<outfile>).
 * @outfile = denumirea fișierului de output
 * @argv = lista de argumente
 * @i = indexul primului argument din cadrul directivei curente
 */
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

/*
 * Funcția stabilește directorul în care vor fi
 * căutate fișierele prima dată, pe baza căii
 * către fișierul de input; în cazul în care input-ul se ia de la
 * stdin, directorul de bază va fi cel curent (adică .).
 * @base_dir = path-ul către directorul de bază
 * @infile = path-ul către fișierul de input,
 * respectiv NULL dacă input-ul se ia de la stdin
 */
int setup_base_dir(char **base_dir, char *infile)
{
	int last, i;

	if (infile == NULL) {
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
			*base_dir = calloc(2, sizeof(char));
			if (*base_dir)
				strncpy(*base_dir, ".", 1);
		}
	}

	if (*base_dir == NULL)
		return 12;

	return 0;
}

/*
 * Funcția adaugă alte directoare în lista de directoare
 * în care se va realiza căutarea fișierelor,
 * pe baza argumentelor (-I[ ]<dir>)
 * @dir = path-ul către directorul ce trebuie adăugat în listă
 * @dirs = lista de directoare
 * @h = hashmap-ul cu asocierile specifice directivelor de tip define
 */
int add_other_dir(char *dir, Node_t **dirs, Hashmap **h)
{
	Node_t *new_dir = malloc(sizeof(Node_t)), *p;

	if (new_dir == NULL) {
		free_hashmap(*h);
		free(*h);
		return 12;
	}

	new_dir->data = calloc(strlen(dir) + 1, sizeof(char));
	if (new_dir->data) {
		strncpy(new_dir->data, dir, strlen(dir));
	} else {
		free_hashmap(*h);
		free(*h);
		free(new_dir);
		for (p = *dirs; p;) {
			Node_t *to_del = p;

			p = p->next;
			free(to_del->data);
			free(to_del);
		}
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

/*
 *
 */
int process_line(char *line, char *base_dir, Node_t *other_dirs, FILE * in,
			FILE * out, Hashmap *h);

/*
 * Funcția procesează o linie din fișierul de input de tip
 * "#include [...]" și scrie output-ul corespunzător în
 * fișierul de ieșire.
 * @line = linia curentă, cea care trebuie să fie procesată
 * @base_dir = primul director în care se va căuta
 * fișierul ce trebuie inclus
 * @other_dirs = lista cu directoarele în care se va căuta
 * fișierul ce trebuie inclus în cazul în care acesta nu
 * se găsește în directorul de bază
 * @out = fișierul de output
 * @h = hashmap-ul cu asocierile specifice directivelor de tip define
 */
int process_include(char *line, char *base_dir, Node_t *other_dirs, FILE *out,
			Hashmap *h)
{
	char *file_to_include_name = line + 10, *path, *file_to_include;
	FILE *file_incl;
	int found, ret;
	Node_t *p;

	file_to_include = calloc(strlen(file_to_include_name) - 1,
					sizeof(char));
	if (file_to_include == NULL)
		return 12;
	strncpy(file_to_include, file_to_include_name,
		strlen(file_to_include_name) - 2);

	/*
	 * se caută fișierul în directorul de bază
	 */
	path = calloc(LINE_LEN, sizeof(char));
	if (!path) {
		free(file_to_include);
		return 12;
	}
	strcpy(path, base_dir);
	strcat(path, "/");
	strcat(path, file_to_include);

	file_incl = fopen(path, "r");

	/*
	 * dacă fișierul nu se găsește în directorul de bază,
	 * se încearcă apoi, rând pe rând,
	 * celelalte directoare disponibile
	 */
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

		/*
		 * dacă fișierul nu este găsit nicăieri,
		 * se întoarce un cod de eroare
		 */
		if (!found) {
			free(file_to_include);
			free(path);
			return -1;
		}
	}

	/*
	 * se citesc și se prelucrează pe rând liniile
	 * din fișierul ce trebuie inclus
	 */
	while (fgets(line, LINE_LEN, file_incl)) {
		ret = process_line(line, base_dir, other_dirs, file_incl, out,
					h);

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

/*
 * Funcția verifică dacă un anumit caracter reprezintă finalul
 * unui cuvânt ce ar putea să reprezinte o cheie în hashmap-ul
 * de asocieri specifice define-ului.
 * @c = caracterul curent
 */
int ending_char(char c)
{
	if (c >= 'a' && c <= 'z')
		return 0;

	if (c >= 'A' && c <= 'Z')
		return 0;

	if (c >= '0' && c <= '9')
		return 0;

	if (c == '_')
		return 0;

	return 1;
}

/*
 * Funcția modifică pe baza asocierilor din hashmap,
 * înlocuind dacă este cazul cheile cu valorile asociate;
 * modificările nu sunt scrise direct în fișierul de output.
 * @line = linia curentă, ce trebuie prelucrată
 * @h = hashmap-ul cu asocierile specifice directivelor de tip define
 */
void change_line_inplace(char *line, Hashmap *h)
{
	int quote = 0, i, j;
	char new_line[LINE_LEN + 10] = {0}, var_candidate[LINE_LEN], *replace;

	if (!line)
		return;

	/*
	 * se formează, caracter cu caracter, "cuvintele" ce ar
	 * putea reprezenta chei în hashmap-ul h și,
	 * dacă este cazul, acestea sunt înlocuite în
	 * cardul liniei curente de valoarea asociată
	 */
	for (i = 0; i < (int)strlen(line); i++) {
		/*
		 * dacă caracterul curent nu poate face parte dintr-o cheie
		 * sau ne aflăm în interiorul unui string (adică între
		 * ghilimele), caracterul nu este modificat
		 */
		if (!((line[i] >= 'a' && line[i] <= 'z') ||
			(line[i] >= 'A' && line[i] <= 'Z') ||
			line[i] == '_') ||
			quote) {
			sprintf(new_line + strlen(new_line), "%c", line[i]);
			if (line[i] == '"')
				quote = 1 - quote;
		} else {
			/*
			 * se formează o potențială cheie
			 */
			for (j = i; j < (int)strlen(line); j++) {
				/*
				 * când ajungem la finalul cheii,
				 * verificăm dacă aceasta trebuie
				 * să fie înlocuită cu maparea
				 * corespunzătoare
				 */
				if (ending_char(line[j])) {
					var_candidate[j - i] = '\0';
					replace = get(h, var_candidate);

					if (replace)
						sprintf(new_line +
							strlen(new_line),
							"%s%c",
							replace,
							line[j]);
					else
						sprintf(new_line +
							strlen(new_line),
							"%s%c",
							var_candidate,
							line[j]);
					i = j;
					break;
				}
				var_candidate[j - i] = line[j];
			}

			/*
			 * mă asigur că și ultimul cuvânt de pe
			 * linie este verificat
			 */
			if (j == (int)strlen(line)) {
				var_candidate[j - i] = '\0';
				replace = get(h, var_candidate);

				if (replace)
					sprintf(new_line + strlen(new_line),
						"%s", replace);
				else
					sprintf(new_line + strlen(new_line),
						"%s", var_candidate);
				break;
			}
		}
	}

	strcpy(line, new_line);
}

/*
 * Funcția procesează o linie de tipul "#define [...]".
 * @line = linia curentă
 * @h = hashmap-ul cu asocierile specifice directivelor de tip define
 * @in = fișierul de intrare
 */
int process_define(char *line, Hashmap *h, FILE *in)
{
	char *p, *key, *value, *val = calloc(DEFINE_LEN, sizeof(char));
	int done, ret;

	if (!val)
		return 12;

	/*
	 * se parsează cheia
	 */
	p = strtok(line, " ");
	p = strtok(NULL, "\n ");
	key = calloc(strlen(p) + 1, sizeof(char));
	if (!key) {
		free(val);
		return 12;
	}
	strncpy(key, p, strlen(p));

	/*
	 * se parsează valoarea asociată cheii, ținându-se cont și
	 * de cazul în care valoarea este reprezentată
	 * pe mai multe linii
	 */
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

	/*
	 * se verifică dacă în cadrul valorii curente există chei
	 * definite anterior și, dacă este cazul, se prelucrează
	 * valorea corespunzător
	 */
	change_line_inplace(value, h);

	/*
	 * se adaugă asocierea dintre cheie și valoare în hashmap
	 */
	ret = put(h, key, value);

	free(val);
	free(key);
	return ret;
}

/*
 * Funcția procesează linii de tipul "#undef [...]", ștergând asocierea
 * corespunzătoare unei anumite chei din hashmap.
 * @line = linia ce trebuie să fie procesată
 * @h = hashmap-ul cu asocierile specifice directivelor de tip define
 */
void process_undef(char *line, Hashmap *h)
{
	char *p, *key;

	p = strtok(line, " ");
	key = strtok(NULL, "\n ");

	remove_ht_entry(h, key);
}

/*
 * Funcția "sare" peste liniile din fișier corespunzătoare unei
 * ramuri de directive de tipul #if, #elif, #ifdef, #ifndef
 * pentru care condiția necesară nu este îndeplinită.
 * @line = buffer-ul în care se vor citi liniile sărite
 * @in = fișierul de intrare
 */
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

/*
 * Funcția prelucrează o linie de tipul "#if[...]".
 * @line = linia ce trebuie să fie prelucreată
 * @h = hashmap-ul cu asocierile specifice directivelor de tip define
 * @in = fișierul de intrare
 * @out = fișierul de ieșire
 * @base_dir = directorul de bază în care sunt căutate fișierele
 * @other_dirs = lista de directoare în care sunt căutate fișierele
 */
int process_if(char *line, Hashmap *h, FILE *in, FILE *out, char *base_dir,
		Node_t *other_dirs)
{
	int cond = 0, res;
	char *symbol, *value;

	/*
	 * linie de tipul #ifdef
	 */
	if (line[3] == 'd') {
		symbol = strtok(line, " ");
		symbol = strtok(NULL, "\n ");

		/*
		 * condiția este reprezentată de verificarea existenței în
		 * hashmap a symbolului (cheii) din cadrul directivei
		 */
		cond = contains(h, symbol);

	/*
	 * linie de tipul #ifndef
	 */
	} else if (line[3] == 'n') {
		symbol = strtok(line, " ");
		symbol = strtok(NULL, "\n ");

		/*
		 * condiția este reprezentată de verificarea absenței din
		 * hashmap a symbolului (cheii) din cadrul directivei
		 */
		cond = 1 - contains(h, symbol);

	/*
	 * linie de tipul #if
	 */
	} else {
		symbol = strtok(line, " ");
		symbol = strtok(NULL, "\n ");
		value = get(h, symbol);

		/*
		 * condiția este reprezentată de verificarea faptului că
		 * valoarea la care se mapează cheia din cadrul
		 * directivei este diferită de "0"
		 */
		if ((value && strcmp(value, "0") == 0) ||
			strcmp(symbol, "0") == 0)
			cond = 0;
		else
			cond = 1;
	}

	/*
	 * dacă condiția nu este îndeplinită, se sar linii până ajungem
	 * la o ramură pe care condiția să devină adevărată
	 */
	if (!cond) {
		while (fgets(line, LINE_LEN, in)) {
			if (strncmp(line, "#elif", 5) == 0)
				return process_if(line + 2, h, in, out,
							base_dir, other_dirs);
			else if (strncmp(line, "#else", 5) == 0)
				break;
			else if (strncmp(line, "#endif", 6) == 0)
				return 0;
		}
	}

	res = 0;

	/*
	 * se procesează linii până la finalul blocului condițional curent
	 */
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

/*
 * Funcția inițializează resursele necesare procesului de prepocesare.
 * @line = buffer-ul în care se vor realiza citirile
 * @infile = path-ul fișierului de intrare
 * @outfile = path-ul fișierului de ieșire
 * @in = fișierul de intrare
 * @out = fișierul de ieșire
 */
int init_preprocess(char **line, char *infile, char *outfile, FILE **in,
			FILE **out)
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

/*
 * Funcția este asemănătoare funcției change_line_inplace,
 * doar că de această dată, rezultatele obținute sunt
 * scrise direct în fișierul de output.
 * @line = linia ce trebuie prelucrată
 * @out = fișierul de ieșire
 * @h = hashmap-ul cu asocierile specifice directivelor de tip define
 */
void change_line(char *line, FILE *out, Hashmap *h)
{
	int quote = 0, i, j;
	char var_candidate[LINE_LEN], *replace;

	for (i = 0; i < (int)strlen(line); i++) {
		if (!((line[i] >= 'a' && line[i] <= 'z') ||
			(line[i] >= 'A' && line[i] <= 'Z') ||
			line[i] == '_') ||
			quote) {
			fprintf(out, "%c", line[i]);
			if (line[i] == '"')
				quote = 1 - quote;
		} else {
			for (j = i; j < (int)strlen(line); j++) {
				if (ending_char(line[j])) {
					var_candidate[j - i] = '\0';
					replace = get(h, var_candidate);

					if (replace)
						fprintf(out, "%s%c", replace,
							line[j]);
					else
						fprintf(out, "%s%c",
							var_candidate, line[j]);
					i = j;
					break;
				}
				var_candidate[j - i] = line[j];
			}
		}
	}
}

/*
 * Funcția realizează prelucrarea liniei curente, cu ajutorul funcțiilor
 * auxiliare corespunzătoare.
 * @line = linia ce trebuie prelucrată
 * @base_dir = directorul de bază în care sunt căutate fișierele
 * @other_dirs = lista cu celelalte directoare în care sunt căutate fișierele
 * @in = fișierul de intrare
 * @out = fișierul de ieșire
 * @h = hashmap-ul cu asocierile specifice directivelor de tip define
 */
int process_line(char *line, char *base_dir, Node_t *other_dirs, FILE *in,
			FILE *out, Hashmap *h)
{
	/*
	 * linie ce conține o directivă
	 */
	if (line[0] == '#') {
		/*
		 * line de tipul #include
		 */
		if (line[1] == 'i' && line[2] == 'n' && line[9] == '"')
			return process_include(line, base_dir, other_dirs, out,
						h);

		/*
		 * linie de tipul #define
		 */
		else if (line[1] == 'd')
			return process_define(line, h, in);

		/*
		 * linie de tipul #undef
		 */
		else if (line[1] == 'u')
			process_undef(line, h);

		/*
		 * linie de tipul #if
		 */
		else if (line[1] == 'i' && line[2] == 'f')
			return process_if(line, h, in, out, base_dir,
						other_dirs);

		/*
		 * linie de tipul #endif
		 */
		else if (line[1] == 'e' && line[2] == 'n')
			return EXIT_IF;

		/*
		 * linie de tipul #else
		 */
		else if (line[1] == 'e' && line[2] == 'l')
			return SKIP;
	} else {
		/*
		 * linie ce nu conține o directivă
		 */
		change_line(line, out, h);
	}

	return 0;
}

/*
 * Funcția realizează preprocesarea fișierului pornind de la informațiile
 * extrase din argumentele cu care este rulat executabilul.
 * @infile = path-ul fișierului de intare
 * @outfile = path-ul fișierului de ieșire
 * @h = hashmap-ul cu asocierile specifice directivelor de tip define
 * @base_dir = directorul de bază în care sunt căutate fișierele
 * @other_dirs = lista cu celelalte directoare în care sunt căutate fișierele
 */
int preprocess(char *infile, char *outfile, Hashmap *h, char *base_dir,
		Node_t *other_dirs)
{
	FILE *in, *out;
	char *line;

	/*
	 * se inițializează resursele folosite
	 */
	int ret = init_preprocess(&line, infile, outfile, &in, &out);

	if (ret)
		return ret;

	/*
	 * se prelucrează rând pe rând liniile din fișierul de input
	 */
	while (fgets(line, LINE_LEN, in)) {
		ret = process_line(line, base_dir, other_dirs, in, out, h);
		if (ret) {
			free(line);
			if (in != stdin)
				fclose(in);
			if (out != stdout)
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

/*
 * Funcția extrage informațiile utile din argumentele cu care este
 * rulat executabilul.
 * @argc = numărul de argumente
 * @argv = lista de argumente
 * @infile = path-ul fișierului de intrare
 * @outfile = path-ul fișierului de ieșire
 * @h = hashmap-ul cu asocierile specifice directivelor de tip define
 * @other_dirs = lista cu celelalte directoare în care sunt
 * căutate fișierele
 */
int parse_args(int argc, char *argv[], char **infile, char **outfile,
		Hashmap **h, Node_t **other_dirs)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			/*
			 * informații legate de fișierul de input
			 */
			if (*infile == NULL)
				*infile = argv[i];
			/*
			 * informații legate de fișierul de output
			 */
			else if (*outfile == NULL)
				*outfile = argv[i];
			else
				return -1;
		/*
		 * informații legate de o directivă de tip define
		 */
		} else if (argv[i][1] == 'D') {
			if (add_arg_define(argv, *h, &i)) {
				free_hashmap(*h);
				free(*h);
				return 12;
			}
		/*
		 * informații legate de directoarele în care sunt căutate
		 * fișierele
		 */
		} else if (argv[i][1] == 'I') {
			if (strlen(argv[i]) == 2) {
				i++;
				if (add_other_dir(argv[i], other_dirs, h))
					return 12;
			} else {
				if (add_other_dir(argv[i] + 2, other_dirs, h))
					return 12;
			}
		/*
		 * informații legate de fișierul de output
		 */
		} else if (argv[i][1] == 'o') {
			if (add_arg_outfile(outfile, argv, &i))
				return -1;
		} else {
			return -1;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	Hashmap *h;
	char *infile = NULL, *outfile = NULL, *base_dir;
	Node_t *other_dirs = NULL, *p, *crt;
	int ret;

	/*
	 * se inițializează hashmap-ul
	 */
	if (setup_hashmap(&h))
		return 12;

	/*
	 * se parsează informația transmisă prin intermediul argumentelor
	 */
	ret = parse_args(argc, argv, &infile, &outfile, &h, &other_dirs);
	if (ret)
		return ret;

	/*
	 * inițializarea resurselor adiționale
	 */
	if (setup_base_dir(&base_dir, infile)) {
		free_hashmap(h);
		free(h);
		return 12;
	}

	/*
	 * realizarea preprocesării
	 */
	ret = preprocess(infile, outfile, h, base_dir, other_dirs);

	/*
	 * eliberarea resurselor alocate
	 */
	free_hashmap(h);
	free(h);
	free(base_dir);

	for (p = other_dirs; p;) {
		crt = p;
		p = p->next;
		free(crt->data);
		free(crt);
	}

	return ret;
}
