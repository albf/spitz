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
    pthread_mutex_t tlock;                                          // lock responsible for the fifo of tasks
    pthread_mutex_t rlock;
    sem_t tcount;                                                   // number of tasks available
    sem_t sem;
    char running;
    struct result_node *results;
    void *handle;
    int argc;
    char **argv;
};

void run(int argc, char *argv[], char *so, struct byte_array *final_result)
{
    COMM_set_path(so);				                                // set lib path variable
    
    job_manager(argc, argv, so, final_result);
}

void committer(int argc, char *argv[], void *handle)
{
    int isFinished=0;                                               // Indicate if it's finished.
    int socket_serv=0;                                              // Socket of the requester, return by COMM_wait_request.
    enum message_type type;                                         // Type of received message.
   
    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc(sizeof(struct byte_array));
    byte_array_init(ba, 100);

    // Changed: Indexing by task id, 0 for not committed and 1 for already committed.
    // Once a new task arrive, if it's more them actual cap, realloc using it's id*2
    size_t i, cap = 10;                                             // Initial capacity.
    size_t *committed = malloc(sizeof(size_t) * cap);               // List indexed by the task id. 1 for committed, 0 for not yet.
    size_t task_id;                                                 // Task id of received result.
    
    // Loads the user functions.
    void * (*setup) (int, char **);
    void (*commit_pit) (void *, struct byte_array *);
    void (*commit_job) (void *, struct byte_array *);

    *(void **)(&setup)      = dlsym(handle, "spits_setup_commit");  // Loads the user functions.
    *(void **)(&commit_pit) = dlsym(handle, "spits_commit_pit");
    *(void **)(&commit_job) = dlsym(handle, "spits_commit_job");
    //setup_free = dlsym("spits_setup_free_commit");
    void *user_data = setup(argc, argv);

    for(i=0; i<cap; i++) {                                          // initialize the task array
        committed[i]=0;
    }
        
    info("Starting committer main loop");
    while (1) {
        ba = COMM_wait_request(&type, &socket_serv, ba);
        
        switch (type) {
            case MSG_RESULT:
                byte_array_unpack64(ba, &task_id);
                debug("Got a RESULT message for task %d", task_id);
                
                if(task_id>cap) {                                   // if id higher them actual cap
                    cap=2*task_id;
                    committed = realloc(committed, sizeof(size_t)*cap);

                    for(i=cap/2; i<cap; i++)
                        committed[i]=0;
                }

                if (committed[task_id] == 0) {                      // if not committed yet
                    committed[task_id] = 1;
                    commit_pit(user_data, ba);
                    byte_array_clear(ba);
                    byte_array_pack64(ba, task_id);
                    COMM_send_message(ba, MSG_DONE, socket_manager);
                }

                break;
            case MSG_KILL:
                info("Got a KILL message, committing job");
                byte_array_clear(ba);
                if (commit_job) {
                    commit_job(user_data, ba);
                    COMM_send_message(ba, MSG_RESULT,  sd); 
                }
                isFinished = 1;
                break;
            case MSG_NEW_CONNECTION:
                COMM_create_new_connection();
                break;
            case MSG_CLOSE_CONNECTION:  ;
                uint64_t socket_cl;
                _byte_array_unpack64(ba, &socket_cl);
                COMM_close_connection((int)socket_cl);
                break;
            default:
                break;
        }

        // If he is the only member alive and the work is finished.
        if ((COMM_get_alive() == 1) && (isFinished==1)) {
            info("All workers disconnected, time to die");
            break;
        }
    }

    //setup_free(user_data);
    free(committed);
    info("Terminating committer");
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

    sem_wait (&d->tcount);                                          // wait for the first task to arrive.
    while (d->running) {
        pthread_mutex_lock(&d->tlock);
        cfifo_pop(&d->f, &task);
        pthread_mutex_unlock(&d->tlock);

        sem_post(&d->sem);

        byte_array_unpack64(&task, &task_id);

        debug("[worker] Got a TASK %d", task_id);
        struct result_node *result = malloc(sizeof(*result));
        byte_array_init(&result->ba, 10);
        byte_array_pack64(&result->ba, task_id);
        execute_pit(user_data, &task, &result->ba);
        byte_array_free(&task);

        pthread_mutex_lock(&d->rlock);
        result->next = d->results;
        d->results = result;
        pthread_mutex_unlock(&d->rlock);

        sem_wait (&d->tcount);                                  // wait for the next task to arrive.
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
            COMM_send_message(&n->ba, MSG_RESULT, socket_committer);
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
            COMM_send_message(&n->ba, MSG_RESULT,socket_committer);
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
    int alive = 1;                                                  // Indicate if it still alive.
    enum message_type type;                                         // Type of received message.
    int tasks = 0;                                                  // Tasks received and not committed.
    int min_results = 10;                                           // Minimum of results to send at the same time. 
    enum blocking b = NONBLOCKING;

    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc(sizeof(struct byte_array));
    byte_array_init(ba, 100);

    struct byte_array task;                                         // Used to pass tasks to other threads

    info("Starting task manager main loop");
    while (alive) {

        debug("Sending READY message to JOB_MANAGER");
        COMM_send_message(NULL, MSG_READY, socket_manager);
        ba = COMM_read_message(ba, &type, socket_manager);

        switch (type) {
            case MSG_TASK:
                debug("waiting task buffer to free some space");
                sem_wait(&d->sem);
                byte_array_init(&task, ba->len);
                byte_array_pack8v(&task, ba->ptr, ba->len);

                pthread_mutex_lock(&d->tlock);
                cfifo_push(&d->f, &task);
                pthread_mutex_unlock(&d->tlock);
                sem_post(&d->tcount);
                
                tasks++;
                break;
            case MSG_KILL:
                info("Got a KILL message");
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

    info("Terminating task manager");
    byte_array_free(ba);
    COMM_disconnect_from_committer();                               // Disconnect from the committer.
    COMM_disconnect_from_job_manager();                             // Disconnect from the manager.
}

void start_master_process(int argc, char *argv[], char *so)
{
    info("opening %s shared object", so);

    void *ptr = dlopen(so, RTLD_LAZY);
    if (!ptr) {
        error("could not open %s: %s", so, dlerror());
    }

    int (*spits_main) (int argc, char *argv[],                      // Loads user functions.                   
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

    // Warned everyone, just closes the program. 
    info("terminating spitz");
}

void start_slave_processes(int argc, char *argv[])
{
    lib_path = COMM_get_path();
    
    while (strncmp(lib_path, "NULL", 4) != 0) {
        info("received a module to run %s", lib_path);

        void *handle = dlopen(lib_path, RTLD_LAZY);
        if (!handle) {
            error("could not open %s", lib_path);
            return;
        }

        if (my_rank == COMMITTER) {
            committer(argc, argv, handle);
        } else {
            pthread_t t[NTHREADS];

            struct thread_data d;
            cfifo_init(&d.f, sizeof(struct byte_array), FIFOSZ);
            sem_init(&d.sem, 0, FIFOSZ);
            sem_init (&d.tcount, 0, 0);
            pthread_mutex_init(&d.tlock, NULL);
            pthread_mutex_init(&d.rlock, NULL);
            d.running = 1;
            d.results = NULL;
            d.handle = handle;
            d.argc = argc;
            d.argv = argv;

            int i, tmid = my_rank;
            for (i = 0; i < NTHREADS; i++) {
                d.id = NTHREADS * tmid + i;
                pthread_create(&t[i], NULL, worker, &d);
            }

            task_manager(&d);

            d.running = 0;                      // Finish the running threads
            for(i=0; i< NTHREADS; i++) {
                sem_post(&d.tcount);
            }

            for (i = 0; i < NTHREADS; i++) {    // Join them all
                pthread_join(t[i], NULL);
            }
        }

        free(lib_path);
        
        // --- ONE RUN FOR NOW
        //lib_path = COMM_get_path();
        break;                                  // One run, so get out
    }
}

int main(int argc, char *argv[])
{
    int size;
    enum actor type=atoi(argv[1]);

    if(type==JOB_MANAGER) {
        COMM_setup_job_manager_network(argc , argv);
    } 
    else {
        COMM_connect_to_job_manager(argv[2]);
        
        if(type==COMMITTER) { 		                                // The committer sets itself in the jm
            COMM_setup_committer_network();
        }
        else {						                                // Task Managers get the committer 
            COMM_connect_to_committer();
        }
    }
    
    char *debug = getenv("SPITS_DEBUG_SLEEP");
    if (debug) {
        int amount = atoi(debug);
        pid_t pid = getpid();
        printf("Rank %d at pid %d\n", my_rank, pid);
        sleep(amount);
    }

    char *loglvl = getenv("SPITS_LOG_LEVEL");
    if (loglvl) {
        LOG_LEVEL = atoi(loglvl);
    }

    char *nthreads = getenv("SPITS_NUM_THREADS");
    if (nthreads) {
        NTHREADS = atoi(nthreads);
    }

    char *fifosz = getenv("SPITS_TMCACHE_SIZE");
    if (fifosz) {
        FIFOSZ = atoi(fifosz);
    }

    if (type == JOB_MANAGER && LOG_LEVEL >= 1) {
        printf("Welcome to spitz " SPITZ_VERSION "\n");
    }

    if (type == JOB_MANAGER && argc < 2) {
        fprintf(stderr, "Usage: SO_PATH\n");
        return EXIT_FAILURE;
    }

    nworkers = (size - 2) * NTHREADS;

    char *so = argv[3];

    /* Remove the first two arguments */
    argc -= 4;
    argv += 4;

    if (type == JOB_MANAGER) {
        start_master_process(argc, argv, so);
    }
    else {
        start_slave_processes(argc, argv);
    }

    return 0;
}