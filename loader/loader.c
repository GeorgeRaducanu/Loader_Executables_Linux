/*
 * Loader Implementation
 *
 * 2022, Operating Systems
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "exec_parser.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

static so_exec_t *exec;
static int file_descriptor;
//declaram unistd.h pentru a nu
//avea eroarea de "implicit declaration of functions
// la lseek getpagesize si read

static void segv_handler(int signum, siginfo_t *info, void *context)
{
	/* TODO - actual loader implementation */

	if (signum != SIGSEGV) {
		raise(SIGSEGV);
		//default handler-ul
	}

	//micunelete ce ne vor ajuta mai tarziu
	//pt retinerea segmentului in care apare buseala
	so_seg_t *remember;
	int ok = 0;

	for (int i = 0; i < exec->segments_no; ++i) {
		//parcurg toate segmentele
		so_seg_t *aux = exec->segments + i;
		//aux segmentul curent
		//adresa busita este (void * )info->si_addr
		//verific daca aux->vaddr <= x < aux->vaddr + aux->memory_size
		//unde x adresa busita
		if (aux->vaddr <= (int)info->si_addr && (int)info->si_addr < aux->vaddr + aux->mem_size) {
			//adresa busita e in segmentul meu de date
			remember = aux;
			ok = 1;
		}
	}

	int a = ((int)info->si_addr - remember->vaddr)/getpagesize();
	
	if (ok == 0) {
		//nu am gasit remember;
		//apelez default handler-ul
		raise(SIGSEGV);
		//iarasi default handler-ul
	}

	if (ok == 1 && remember->data != NULL) {
		//am gasit remember si pagina a fost mapata
		//trb sa gasesc nr paginii din segmentul bulit
		// folosesc getpagesize()
		if (((char *)remember->data)[a] == 1)
			raise(SIGSEGV); //default handler
	}

	//calculez cat de mult copiez din executabil
	// sau pe biti PROT_READ | PROT_WRITE | PROT_EXEC
	// ca sa le am pe toate intr-un singur loc
	int prot_or = PROT_READ | PROT_EXEC | PROT_WRITE;
	//flags MAP_SHARED | MAP_ANONYMUS | MAP_FIXED
	int map_flags = MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED;
	void *address = mmap(((void *)remember->vaddr + a * getpagesize()), getpagesize(), prot_or, map_flags, file_descriptor, 0);

	if (remember->data == NULL) {
		//daca nu e alocat remember->data il aloc eu si zeroizez
		remember->data = (char *)malloc((remember->mem_size)/getpagesize() * sizeof(char));
		memset(remember->data, 0, (remember->mem_size)/getpagesize() * sizeof(char));
	}
	((char *)remember->data)[a] = 1;
	//trebuie sa aflu cat vreau sa citesc
	// minimul dintre getpagesize() si cat mai am pana la finalul segmentului ca nr de pagini
	//  remember->file_size - getpagesize() * a;
	int mini = getpagesize();

	if (remember->file_size - getpagesize() * a < mini)
		mini = remember->file_size - getpagesize() * a;

	//am gasit minimul
	if (getpagesize() * a < remember->file_size) {
		lseek(file_descriptor, a * getpagesize() + remember->offset, SEEK_SET);
		//lseek repozitioneaza offset ul
		read(file_descriptor, address, mini);
		//al 3-lea parametru la read e minimul gasit
	}
	//punem protectie pe zona de memorie
	mprotect(address, getpagesize(), remember->perm);
}

int so_init_loader(void)
{
	int rc;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	rc = sigaction(SIGSEGV, &sa, NULL);
	if (rc < 0) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);
	file_descriptor = open(path, O_RDONLY);
	//avem nevoie si de file descriptor pt lseek
	//file_descriptor ul este dupa apel un ne intreg, ne-negativ,
	// index catre entry ul in tabelul de descriptori de fisiere

	if (!exec)
		return -1;

	so_start_exec(exec, argv);

	return -1;
}
