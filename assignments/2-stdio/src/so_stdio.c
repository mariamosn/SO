/*
 * Maria Moșneag
 * 333CA
 */

#include "so_stdio.h"
#include "xfile.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>  /* open */
#include <sys/stat.h>	/* open */
#include <sys/wait.h>
#include <fcntl.h>  /* O_CREAT, O_RDONLY */
#include <unistd.h>  /* close, lseek, read, write */
#include <errno.h>

#define BUF_LEN 4096

#define READ 1
#define WRITE 2

/*
 * tip de date care descrie un fișier
 */
typedef struct _so_file {
	/*
	 * file descriptor-ul fișierului
	 */
	int fd;
	/*
	 * buffer-ul asociat structurii pentru a eficientiza numărul de
	 * scrieri și de citiri
	 */
	char buffer[BUF_LEN];
	/*
	 * indexul primei poziții libere din buffer
	 */
	int buf_index;
	/*
	 * cantitatea curentă de date valide disponibile
	 */
	int buf_size;
	/*
	 * flag-ul memorează modul în care a fost deschis fișierul
	 */
	int mod;
	/*
	 * flag-ul este
	 * 0 în cazul în care nu au existat erori în cadrul operațiilor
	 * efectuate anterior asupra acestui fișier și
	 * o valoare diferită de 0 altfel
	 */
	int error;
	/*
	 * flag-ul are valoarea READ dacă ultima operație efectuată asupra
	 * fișierului a fost una de citire și WRITE în cazul în care ultima
	 * operație a fost o scriere
	 */
	int last;
	/*
	 * flag-ul este 0 dacă încă nu s-a ajuns la finalul fișierului și
	 * o valoare diferită de 0 altfel
	 */
	int eof;
	pid_t pid;

} SO_FILE;

/*
 * funcție similară fopen din libc, deschide un fișier
 * @pathname	= calea către fișier
 * @mode	= modul în care va fi deschis fișierul
 * @return	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 *		  sau NULL în caz de eroare
 */
SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int fd = -1, mod;
	SO_FILE *result;

	/*
	 * verifică modul în care va fi deschis fișierul
	 */
	if (strncmp(mode, "r+", 2) == 0) {
		fd = open(pathname, O_RDWR);
		mod = READ | WRITE;

	} else if (strncmp(mode, "r", 1) == 0) {
		fd = open(pathname, O_RDONLY);
		mod = READ;

	} else if (strncmp(mode, "w+", 2) == 0) {
		fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
		mod = READ | WRITE;

	} else if (strncmp(mode, "w", 1) == 0) {
		fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		mod = WRITE;

	} else if (strncmp(mode, "a+", 2) == 0) {
		fd = open(pathname, O_RDWR | O_CREAT | O_APPEND, 0644);
		mod = READ | WRITE;

	} else if (strncmp(mode, "a", 1) == 0) {
		fd = open(pathname, O_WRONLY | O_CREAT | O_APPEND, 0644);
		mod = WRITE;
	}

	if (fd < 0)
		return NULL;

	/*
	 * inițializează structura SO_FILE
	 */
	result = malloc(sizeof(SO_FILE));
	if (result == NULL) {
		close(fd);
		return NULL;
	}

	result->fd = fd;
	result->buf_index = 0;
	result->buf_size = 0;
	result->mod = mod;
	result->error = 0;
	result->last = 0;
	result->eof = 0;

	return result;
}

/*
 * funcție similară fclose din libc, închide un fișier
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= 0 în caz de succes
 *		  sau SO_EOF în caz de eroare
 */
int so_fclose(SO_FILE *stream)
{
	int res;

	/*
	 * înainte de a închide fișierul, în cazul în care ultima operație
	 * efectuată a fost o scriere, se va face flush buffer-ului intern
	 */
	res = so_fflush(stream);
	if (res == SO_EOF && stream->last == WRITE) {
		stream->error = SO_EOF;
		free(stream);
		return SO_EOF;
	}

	/*
	 * se închide fișierul
	 */
	res = close(stream->fd);
	if (res == -1) {
		stream->error = SO_EOF;
		free(stream);
		return SO_EOF;
	}

	/*
	 * se eliberează memoria asociată structurii SO_FILE
	 */
	free(stream);
	return 0;
}

/*
 * funcția întoarce file descriptor-ul asociat unui fișier
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= file descriptor-ul asociat fișierului
 */
int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

/*
 * funcție similară fflush din libc, forțează scrierea datelor din buffer-ul
 * intern în fișier în cazul în care ultima operație a fost una de scriere
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= 0 în caz de succes
 *		  sau SO_EOF în caz de eroare
 */
int so_fflush(SO_FILE *stream)
{
	int bytes_written;

	/*
	 * verifică dacă ultima operație a fost o scriere
	 */
	if (stream->last != WRITE)
		return SO_EOF;

	/*
	 * scrie conținutul buffer-ului intern în fișier
	 */
	bytes_written = xwrite(stream->fd, stream->buffer, stream->buf_index);
	if (bytes_written == -1) {
		stream->error = SO_EOF;
		return SO_EOF;
	}

	/*
	 * invalidează datele din buffer
	 */
	stream->buf_index = 0;
	stream->buf_size = 0;

	return 0;
}

/*
 * funcție similară fseek din libc, mută cursorul fișierului
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @offset	= offset-ul cu care va fi mutat fișierul față de "reper"
 * @whence	= "reperul" față de care se mută cursorul
 * @return	= 0 în caz de succes
 *		  sau -1 în caz de eroare
 */
int so_fseek(SO_FILE *stream, long offset, int whence)
{
	int res;

	/*
	 * dacă ultima operație a fost o citire, se invalidează datele din
	 * buffer
	 */
	if (stream->last == READ) {
		stream->buf_index = 0;
		stream->buf_size = 0;

	/*
	 * dacă ultima operație a fost o scriere, se face flush
	 */
	} else if (stream->last == WRITE) {
		res = so_fflush(stream);
		if (res == SO_EOF) {
			stream->error = SO_EOF;
			return -1;
		}
	}

	/*
	 * se mută cursorul la poziția corespunzătoare
	 */
	res = lseek(stream->fd, offset, whence);
	if (res == -1) {
		stream->error = errno;
		return -1;
	}

	return 0;
}

/*
 * funcție similară ftell din libc, întoarce poziția curentă
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= poziția curentă în cadrul fișierului (offset-ul față de
 *		  începutul fișierului)
 *		  sau -1 în caz de eroare
 */
long so_ftell(SO_FILE *stream)
{
	long crt_off;

	/*
	 * determină poziția curentă reală, aceasta depinzând însă de starea
	 * buffer-ului intern
	 */
	crt_off = lseek(stream->fd, 0, SEEK_CUR);
	if (crt_off == -1) {
		stream->error = SO_EOF;
		return -1;
	}

	/*
	 * se determină poziția curentă, ținând cont de offset-ul din cadrul
	 * buffer-ului intern;
	 * dacă ultima operație a fost o scriere, înseamnă că în buffer se află
	 * caractere în plus, care nu au apucat încă să fie scrise în fișier;
	 * dacă ultima operație a fost o citire, înseamnă că în buffer se află
	 * mai multe caractere, care nu au fost încă citite
	 */
	if (stream->last == WRITE)
		crt_off += stream->buf_index;
	else if (stream->last == READ)
		crt_off -= (stream->buf_size - stream->buf_index);

	return crt_off;
}

/*
 * funcție similară fread din libc, citește mai multe elemente
 * @ptr		= un pointer către zona de memorie în care vor fi stocate datele
 *		  citite
 * @size	= dimensiunea unui element citit
 * @nmemb	= numărul de elemente citite
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= numărul de elemente citite în caz de succes
 *		  sau 0 în caz de eroare
 */
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	long long i;
	int crt;

	/*
	 * elementele sunt citite byte cu byte
	 */
	for (i = 0; i < size * nmemb; i++) {
		crt = so_fgetc(stream);
		if (crt == SO_EOF) {
			if (!stream->eof) {
				stream->error = SO_EOF;
				return 0;
			}
			break;
		}
		((char *)ptr)[i] = (char)crt;
	}

	stream->last = READ;

	return i / size;
}

/*
 * funcție similară fwrite din libc, scrie mai multe elemente
 * @ptr		= un pointer către zona de memorie de unde vor fi luate datele
 *		  scrise
 * @size	= dimensiunea unui element citit
 * @nmemb	= numărul de elemente citite
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= numărul de elemente scrise în caz de succes
 *		  sau 0 în caz de eroare
 */
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	long long i;
	int crt, ret;
	unsigned char c;

	/*
	 * elementele sunt scrise byte cu byte
	 */
	for (i = 0; i < size * nmemb; i++) {
		c = ((unsigned char *)ptr)[i];
		crt = (int)c;
		ret = so_fputc(crt, stream);
		if (ret == SO_EOF) {
			stream->error = SO_EOF;
			return 0;
		}
	}

	stream->last = WRITE;

	return nmemb;
}

/*
 * funcție similară fgetc din libc, citește un byte din fișier
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= caracterul citit, extins la int în caz de succes
 *		  sau SO_EOF în caz de eroare
 */
int so_fgetc(SO_FILE *stream)
{
	int res = 0, bytes_read = 0;

	/*
	 * verifică modul în care a fost deschis fișierul
	 */
	if ((stream->mod & READ) == 0) {
		stream->error = SO_EOF;
		return SO_EOF;
	}

	/*
	 * dacă nu mai sunt date disponibile în buffer, acesta este repopulat
	 */
	if (stream->buf_index >= stream->buf_size || stream->buf_index < 0) {
		bytes_read = read(stream->fd, stream->buffer, BUF_LEN);
		if (bytes_read < 0) {
			stream->error = SO_EOF;
			return SO_EOF;
		} else if (bytes_read == 0) {
			/*
			 * dacă am ajuns la finalul fișierului, se setează
			 * flag-ul corespunzător din SO_FILE
			 */
			stream->eof = 1;
			return SO_EOF;
		}

		stream->buf_index = 0;
		stream->buf_size = bytes_read;
	}

	/*
	 * se determină caracterul citit și se actualizează buffer-ul
	 * intern în mod corespunzător
	 */
	res = (int)((unsigned char) stream->buffer[stream->buf_index]);
	stream->buf_index++;
	stream->last = READ;

	return res;
}

/*
 * funcție similară fputc din libc, scrie un element
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= caracterul scris extins la int în caz de succes
 *		  sau SO_EOF în caz de eroare
 */
int so_fputc(int c, SO_FILE *stream)
{
	int ret;

	/*
	 * verifică permisiunile
	 */
	if ((stream->mod & WRITE) == 0) {
		stream->error = SO_EOF;
		return SO_EOF;
	}

	/*
	 * dacă buffer-ul este plin, se face flush
	 */
	if (stream->buf_index == BUF_LEN || stream->buf_index < 0) {
		ret = so_fflush(stream);
		if (ret == SO_EOF && stream->last == WRITE) {
			stream->error = SO_EOF;
			return SO_EOF;
		}
	}

	/*
	 * se introduce caracterul în buffer-ul intern
	 */
	stream->buffer[stream->buf_index] = (char)c;
	stream->buf_index++;
	stream->last = WRITE;

	return c;
}

/*
 * funcție similară feof din libc, verifică dacă s-a ajuns la finalul fișierului
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= 0 dacă nu s-a ajuns la finalul fișierului
 *		  sau o valoare nenulă altfel
 */
int so_feof(SO_FILE *stream)
{
	return stream->eof;
}

/*
 * funcție similară ferror din libc, verifică dacă s-a întâlnit vreo eroare în
 * urma unei operații efectuate cu fișierul
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= 0 dacă nu s-a întâlnit vreo eroare
 *		  sau o valoare nenulă altfel
 */
int so_ferror(SO_FILE *stream)
{
	return stream->error;
}

/*
 * funcție auxiliară care gestionează acțiunile proceselor după crearea
 * procesului copil
 * @pid		= pid-ul procesului
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @filedes	= file descriptorii pipe-ului
 * @command	= comanda ce va fi executată de procesul copil
 * @return	= -1 în caz de eroare, 0 altfel
 */
int init_proc(pid_t pid, SO_FILE *stream, int filedes[2], const char *command)
{
	/*
	 * eroare apărută la fork
	 */
	if (pid == -1) {
		free(stream);
		close(filedes[0]);
		close(filedes[1]);
		return -1;

	/*
	 * procesul copil
	 */
	} else if (pid == 0) {
		if (stream->mod & READ) {
			dup2(filedes[1], 1);
			close(filedes[0]);
		} else {
			dup2(filedes[0], 0);
			close(filedes[1]);
		}

		if (execl("/bin/sh", "sh", "-c", command, NULL) == -1) {
			free(stream);
			if (stream->mod & READ)
				close(filedes[1]);
			else
				close(filedes[0]);
			return -1;
		}

	/*
	 * procesul părinte
	 */
	} else {
		if (stream->mod & READ)
			close(filedes[1]);
		else
			close(filedes[0]);

		stream->pid = pid;
	}

	return 0;
}

/*
 * funcție similară popen din libc, lansează un proces nou
 * @command	= comanda care va fi executată de procesul nou creat
 * @type	= tipul fișierului întors
 * @return	= un pointer către o structură de tip SO_FILE
 *		  sau NULL în caz de eroare
 */
SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *stream;
	pid_t pid;
	int filedes[2];

	/*
	 * inițializează pipe
	 */
	if (pipe(filedes))
		return NULL;

	/*
	 * inițializează SO_FILE
	 */
	stream = (SO_FILE *)calloc(1, sizeof(SO_FILE));
	if (stream == NULL) {
		close(filedes[0]);
		close(filedes[1]);
		return NULL;
	}

	/*
	 * modificări pe baza tipului fișierului
	 */
	if (strncmp("r", type, 1) == 0) {
		stream->mod = READ;
		stream->fd = filedes[0];
	} else if (strncmp("w", type, 1) == 0) {
		stream->mod = WRITE;
		stream->fd = filedes[1];
	} else {
		close(filedes[0]);
		close(filedes[1]);
		free(stream);
		return NULL;
	}

	/*
	 * pornește proces copil
	 */
	pid = fork();
	if (init_proc(pid, stream, filedes, command) == -1)
		return NULL;

	return stream;
}

/*
 * funcție similară pclose din libc, așteaptă terminarea procesului și
 * eliberează memoria ocupată de SO_FILE
 * @stream	= un pointer către o structură de tip SO_FILE care conține
 *		  datele fișierului
 * @return	= codul de ieșire al procesului sau -1 în caz de eroare
 */
int so_pclose(SO_FILE *stream)
{
	pid_t pid = stream->pid;
	int ret, status;

	if (so_fflush(stream) == SO_EOF && stream->last == WRITE) {
		stream->error = SO_EOF;
		close(stream->fd);
		free(stream);
		return SO_EOF;
	}

	close(stream->fd);
	free(stream);

	/*
	 * așteaptă terminarea proccesului lansat cu so_popen
	 */
	ret = waitpid(pid, &status, 0);
	if (ret == -1 || !WIFEXITED(status))
		return -1;

	return status;
}
