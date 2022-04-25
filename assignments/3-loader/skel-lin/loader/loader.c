/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "exec_parser.h"
#include "utils.h"

#define MAPPED 1
#define NOT_MAPPED 0

static so_exec_t *exec;
static struct sigaction old_sa;
int fd;

int in_segm_check(char *address)
{
	int i;

	for (i = 0; i < exec->segments_no; i++) {
		char *segm_start, *segm_end;

		segm_start = (char *)exec->segments[i].vaddr;
		segm_end = segm_start + exec->segments[i].mem_size;

		if (address >= segm_start && address < segm_end)
			return i;
	}

	return -1;
}

void init_aux_data(int *page_sz, int *file_sz, int *mem_sz,
			int *segm_start_addr, int *segm_perm,
			int *page_start, int *page_end,
			int *file_off, char *address, int segm_no,
			int *page_no_in_segm)
{
	int segm_off;

	*page_sz = getpagesize();
	*file_sz = exec->segments[segm_no].file_size;
	*mem_sz = exec->segments[segm_no].mem_size;
	*segm_start_addr = exec->segments[segm_no].vaddr;
	segm_off = exec->segments[segm_no].offset;
	*segm_perm = exec->segments[segm_no].perm;

	*page_no_in_segm = ((int)address - *segm_start_addr) / *page_sz;
	*page_start = *page_no_in_segm * *page_sz;
	*page_end = (*page_no_in_segm + 1) * *page_sz;
	*file_off = segm_off + *page_start;
}

static void sigsegv_handler(int sig, siginfo_t *info, void *ucontext)
{
	int ret, file_sz, mem_sz;
	int segm_no, segm_start_addr, segm_perm, file_off;
	int page_sz, page_start, page_end, page_no_in_segm;
	char *address, *mmap_res;

	address = info->si_addr;

	/*
	 * Dacă page fault-ul nu este într-un segment cunoscut,
	 * se rulează handler-ul default.
	 */
	ret = in_segm_check(address);
	if (ret == -1) {
		old_sa.sa_handler(sig);
		return;
	}
	segm_no = ret;

	/*
	 * Dacă page fault-ul este generat într-o pagină deja
	 * mapată (segmentul nu are permisiunile necesare),
	 * se rulează handler-ul default.
	 */
	if (info->si_code != SEGV_MAPERR) {
		old_sa.sa_handler(sig);
		return;
	}

	/*
	 * Pagina este mapată la adresa aferentă.
	 */
	init_aux_data(&page_sz, &file_sz, &mem_sz, &segm_start_addr,
			&segm_perm, &page_start, &page_end, &file_off,
			address, segm_no, &page_no_in_segm);

	if (page_start < file_sz) {
		mmap_res = mmap((void *)segm_start_addr + page_start, page_sz,
				segm_perm, MAP_PRIVATE | MAP_FIXED,
				fd, file_off);
		DIE(mmap_res == MAP_FAILED, "mmap error");

		if (page_end >= file_sz && page_end < mem_sz)
			memset((void *)segm_start_addr + file_sz, 0,
					page_end - file_sz);
		else if (page_end >= file_sz)
			memset((void *)segm_start_addr + file_sz, 0,
					mem_sz - file_sz);
	} else {
		mmap_res = mmap((void *)segm_start_addr + page_start, page_sz,
				segm_perm,
				MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
		DIE(mmap_res == MAP_FAILED, "mmap error");
	}

	((char *)(exec->segments[segm_no].data))[page_no_in_segm] = MAPPED;
}

int so_init_loader(void)
{
	/* initialize on-demand loader */
	struct sigaction sa;
	int ret;

	memset(&sa, 0, sizeof(sa));

	sa.sa_sigaction = sigsegv_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;

	ret = sigaction(SIGSEGV, &sa, &old_sa);
	DIE(ret == -1, "sigaction error");

	return 0;
}

void allocate_aux_data()
{
	int i, page_sz;

	page_sz = getpagesize();

	for (i = 0; i < exec->segments_no; i++) {
		if (exec->segments[i].mem_size % page_sz > 0)
			exec->segments[i].data =
				calloc(exec->segments[i].mem_size / page_sz + 1,
					sizeof(char));
		else
			exec->segments[i].data =
				calloc(exec->segments[i].mem_size / page_sz,
					sizeof(char));
		DIE(exec->segments[i].data == NULL, "calloc error");
	}
}

void free_exec()
{
	int i, j, ret;
	int pages_in_segm, page_sz, mem_sz, segm_start_addr, page_start;

	page_sz = getpagesize();

	for (i = 0; i < exec->segments_no; i++) {
		mem_sz = exec->segments[i].mem_size;
		pages_in_segm = mem_sz / page_sz +
				(int)(mem_sz % page_sz != 0);

		for (j = 0; j < pages_in_segm; j++) {
			if (((char *)exec->segments[i].data)[j] == MAPPED) {
				segm_start_addr = exec->segments[i].vaddr;
				page_start = j * page_sz;
				ret = munmap((void *)segm_start_addr +
						page_start, page_sz);
				DIE(ret == -1, "munmap error");
			}

		}

		free(exec->segments[i].data);
	}

	free(exec->segments);
	free(exec);
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	fd = open(path, O_RDONLY, 0644);
	DIE(fd < 0, "opening file error");

	allocate_aux_data();

	so_start_exec(exec, argv);

	close(fd);
	free_exec();

	return -1;
}
