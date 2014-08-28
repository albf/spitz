/*
 * Copyright 2014 Ian Liu Rodrigues <ian.liu@ggaunicamp.com>
 * Copyright 2014 Alber Tabone Novo <alber.tabone@ggaunicamp.com>
 *
 * This file is part of spitz.
 *
 * spitz is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * spitz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with spitz.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#include "cfifo.h"
#include "barray.h"
#include "log.h"
//#include "message.h"
#include "jobmanager.h"
#include "comm.h"

#define SPITZ_VERSION "0.1.0"

int LOG_LEVEL = 0;
extern __thread int workerid;
extern int nworkers;

static int NTHREADS = 1;
static int FIFOSZ = 10;

struct result_node {
	struct byte_array ba;
	struct result_node *next;
};

struct thread_data {
	int id;
	struct cfifo f;
	pthread_mutex_t tlock;
	pthread_mutex_t rlock;
	sem_t sem;
	char running;
	struct result_node *results;
	void *handle;
	int argc;
	char **argv;
};

static int get_task_manager_id(void)
{
	int rank;
	//MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //return rank - 2;
    return (COMM_get_rank_id()-2); 
}

void run(int argc, char *argv[], char *so, struct byte_array *final_result)
{
	COMM_set_path(so);				// set lib path variable
	
	job_manager(argc, argv, so, final_result);
}

void committer(int argc, char *argv[], void *handle)
{
	int alive = 1, socket_serv=0;
	struct byte_array * ba;
	byte_array_init(ba, 100);
	size_t task_id;

	// TODO: use a better data structure (like AVL tree)
	size_t i, len = 0, cap = 10;
	size_t *committed = malloc(sizeof(size_t) * cap);

	void * (*setup) (int, char **);
	void (*commit_pit) (void *, struct byte_array *);
	void (*commit_job) (void *, struct byte_array *);

	*(void **)(&setup)      = dlsym(handle, "spits_setup_commit");
	*(void **)(&commit_pit) = dlsym(handle, "spits_commit_pit");
	*(void **)(&commit_job) = dlsym(handle, "spits_commit_job");
	//setup_free = dlsym("spits_setup_free_commit");
	void *user_data = setup(argc, argv);

	info("starting committer main loop");
	while (alive) {
		int already_committed;
		enum message_type type;
		ba = COMM_wait_request(&type, &socket_serv);
		
		switch (type) {
			case MSG_RESULT:
				byte_array_unpack64(ba, &task_id);
				debug("got a RESULT message for task %d", task_id);
				already_committed = 0;
				for (i = 0; i < len; i++)
					if (committed[i] == task_id)
						already_committed = 1;
				if (!already_committed) {
					if (len == cap) {
						cap *= 2;
						committed = realloc(committed, sizeof(size_t) * cap);
					}
					committed[len++] = task_id;

					commit_pit(user_data, ba);
					byte_array_clear(ba);
					byte_array_pack64(ba, task_id);
					COMM_send_message(ba, MSG_DONE, COMM_get_socket_manager());
				}
				break;
			case MSG_KILL:
				info("got a KILL message, committing job");
				byte_array_clear(ba);
				if (commit_job) {
					commit_job(user_data, ba);
					COMM_send_message(ba, MSG_RESULT, COMM_get_socket_manager());
				}
				alive = 0;
				break;
			default:
				break;
		}
	}

	//setup_free(user_data);
	info("terminating committer");
	byte_array_free(ba);
}

void *worker(void *ptr)
{
	size_t task_id;
	struct thread_data *d = ptr;
	struct byte_array task;

	workerid = d->id;

	void* (*worker_new) (int, char **);
	worker_new = dlsym(d->handle, "spits_worker_new");

	void (*execute_pit) (void *, struct byte_array *, struct byte_array *);
	execute_pit = dlsym(d->handle, "spits_worker_run");

	void *user_data = worker_new ? worker_new(d->argc, d->argv) : NULL;

	while (d->running) {
		pthread_mutex_lock(&d->tlock);
		int has_task = cfifo_pop(&d->f, &task);
		pthread_mutex_unlock(&d->tlock);

		if (!has_task)
			continue;

		sem_post(&d->sem);

		byte_array_unpack64(&task, &task_id);

		debug("[worker] got a TASK %d", task_id);
		struct result_node *result = malloc(sizeof(*result));
		byte_array_init(&result->ba, 10);
		byte_array_pack64(&result->ba, task_id);
		execute_pit(user_data, &task, &result->ba);
		byte_array_free(&task);

		pthread_mutex_lock(&d->rlock);
		result->next = d->results;
		d->results = result;
		pthread_mutex_unlock(&d->rlock);
	}

	void* (*worker_free) (void *);
	worker_free = dlsym(d->handle, "spits_worker_free");
	if (worker_free)
		worker_free(user_data);

	return NULL;
}

enum blocking {
	BLOCKING,
	NONBLOCKING
};

int flush_results(struct thread_data *d, int min_results, enum blocking b)
{
	int len = 0;
	struct result_node *aux, *n = d->results;

	for (aux = n; aux; aux = aux->next) {
		len++;
	}

	if (len <= min_results && b == NONBLOCKING)
		return 0;

	if (len > min_results && b == NONBLOCKING) {
		aux = n->next;
		n->next = NULL;
		n = aux;
		while (n) {
			COMM_send_message(&n->ba, MSG_RESULT, COMM_get_socket_committer());
			byte_array_free(&n->ba);
			aux = n->next;
			free(n);
			n = aux;
		}
		return len - 1;
	}

	if (b == BLOCKING) {
		while (len < min_results) {
			len = 0;
			n = d->results;
			for (aux = n; aux; aux = aux->next)
				len++;
		}
		while (n) {
			COMM_send_message(&n->ba, MSG_RESULT,COMM_get_socket_committer());
			byte_array_free(&n->ba);
			aux = n->next;
			free(n);
			n = aux;
		}
		return len;
	}

	return 0;
}

void task_manager(struct thread_data *d)
{
	int alive = 1;
	int tasks = 0;
	int min_results = 10;
	enum blocking b = NONBLOCKING;

	struct byte_array ba;
	byte_array_init(&ba, 100);

	info("starting task manager main loop");
	while (alive) {
		enum message_type type;
		struct byte_array task;

		debug("sending READY message to JOB_MANAGER");
		COMM_send_message(NULL, MSG_READY, COMM_get_socket_manager());
		COMM_read_message(&ba, &type, COMM_get_socket_manager());

		switch (type) {
			case MSG_TASK:
				debug("waiting task buffer to free some space");
				sem_wait(&d->sem);
				byte_array_init(&task, ba.len);
				byte_array_pack8v(&task, ba.ptr, ba.len);

				pthread_mutex_lock(&d->tlock);
				cfifo_push(&d->f, &task);
				pthread_mutex_unlock(&d->tlock);
				tasks++;
				break;
			case MSG_KILL:
				info("got a KILL message");
				alive = 0;
				min_results = tasks;
				b = BLOCKING;
				break;
			default:
				break;
		}

		if (alive) {
			debug("Trying to flush %d %s...", min_results, b == BLOCKING ? "blocking":"non blocking");
			tasks -= flush_results(d, min_results, b);
			debug("I have sent %d tasks\n", tasks);
		}
	}

	info("terminating task manager");
	byte_array_free(&ba);
}

void start_master_process(int argc, char *argv[], char *so)
{
	info("opening %s shared object", so);

	void *ptr = dlopen(so, RTLD_LAZY);
	if (!ptr) {
		error("could not open %s: %s", so, dlerror());
	}

	int (*spits_main) (int argc, char *argv[],
			void (*runner)(int, char **, char *, struct byte_array *));
	*(void **) (&spits_main) = dlsym(ptr, "spits_main");

	/* If there is no spits_main, execute this shared    */
	/* object, otherwise use the provided main function. */
	if (!spits_main) {
		info("%s does not have a spits_main, running directly", so);
		run(argc, argv, so, NULL);
	} else {
		info("%s has spits_main function, using it", so);
		spits_main(argc, argv, run);
	}
	//dlclose(ptr);

	/* Send zero to kill other processes */
	info("terminating spitz");
	unsigned long zero = 0;
//	MPI_Bcast(&zero, 1, MPI_UNSIGNED_LONG, JOB_MANAGER, MPI_COMM_WORLD);
}

void start_slave_processes(int argc, char *argv[])
{
	int rank;
	char *so;
	
    	rank = COMM_get_rank_id(); 
	so = COMM_get_path();
	
	while (strncmp(so, "NULL", 4) != 0) {
		info("received a module to run %s", so);

		void *handle = dlopen(so, RTLD_LAZY);
		if (!handle) {
			error("could not open %s", so);
			return;
		}

		if (rank == COMMITTER) {
			committer(argc, argv, handle);
		} else {
			pthread_t t[NTHREADS];

			struct thread_data d;
			cfifo_init(&d.f, sizeof(struct byte_array), FIFOSZ);
			sem_init(&d.sem, 0, FIFOSZ);
			pthread_mutex_init(&d.tlock, NULL);
			pthread_mutex_init(&d.rlock, NULL);
			d.running = 1;
			d.results = NULL;
			d.handle = handle;
			d.argc = argc;
			d.argv = argv;

			int i, tmid = get_task_manager_id();
			for (i = 0; i < NTHREADS; i++) {
				d.id = NTHREADS * tmid + i;
				pthread_create(&t[i], NULL, worker, &d);
			}
			task_manager(&d);
			d.running = 0;

			for (i = 0; i < NTHREADS; i++)
				pthread_join(t[i], NULL);
		}

		free(so);
		so = COMM_get_path();
	}
}

int main(int argc, char *argv[])
{
    int rank, size;
    enum actor type=atoi(argv[0]);

    if(type==JOB_MANAGER) {
        COMM_setup_job_manager_network(argc , argv);
        COMM_set_rank(0); 
    } 
    else {
        COMM_connect_to_job_manager(argv[1]);
        COMM_get_rank_id();
        
        if(type==COMMITTER) {		// The committer sets itself in the jm	
            COMM_set_committer();    
		}
        else						// Task Managers get the committer 
            COMM_get_committer();
    }
    
    size=3;
    
    //MPI_Init(&argc, &argv);
	//MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	//MPI_Comm_size(MPI_COMM_WORLD, &size);

	char *debug = getenv("SPITS_DEBUG_SLEEP");
	if (debug) {
		int amount = atoi(debug);
		pid_t pid = getpid();
		printf("Rank %d at pid %d\n", rank, pid);
		sleep(amount);
	}

	char *loglvl = getenv("SPITS_LOG_LEVEL");
	if (loglvl)
		LOG_LEVEL = atoi(loglvl);

	char *nthreads = getenv("SPITS_NUM_THREADS");
	if (nthreads)
		NTHREADS = atoi(nthreads);

	char *fifosz = getenv("SPITS_TMCACHE_SIZE");
	if (fifosz)
		FIFOSZ = atoi(fifosz);

	if (rank == 0 && LOG_LEVEL >= 1)
		printf("Welcome to spitz " SPITZ_VERSION "\n");

	if (rank == 0 && argc < 2) {
		fprintf(stderr, "Usage: SO_PATH\n");
		return EXIT_FAILURE;
	}

	nworkers = (size - 2) * NTHREADS;

	char *so = argv[1];

	/* Remove the first two arguments */
	argc -= 2;
	argv += 2;

	if (rank == JOB_MANAGER)
		start_master_process(argc, argv, so);
	else
		start_slave_processes(argc, argv);

	//MPI_Finalize();
	return 0;
}
