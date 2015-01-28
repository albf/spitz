/* 
 * File:   taskmanager.c
 * Author: alexandre
 *
 * Created on October 5, 2014, 12:51 AM
 */

#include "taskmanager.h"

#ifdef _WIN32
#include <windows.h>
#elif MACOS
#include <sys/param.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif  

#include <dlfcn.h>
#include <pthread.h>
#include "comm.h"
#include "log.h"
#include "spitz.h"

extern __thread int workerid;

// received_one indicates if actual node received at least one task. 
// This is used to reuse the id (and info) when a node reconnects to the JM.
// If the node gets disconnected with received_one = 0, it will get a new id.
int received_one;

// Get the number of cores using system functions. 
int get_number_of_cores() {
#ifdef WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif MACOS
    int nm[2];
    size_t len = 4;
    uint32_t count;
 
    nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);
 
    if(count < 1) {
    nm[1] = HW_NCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);
    if(count < 1) { count = 1; }
    }
    return count;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

// Function responsible for the workers on current TM node.
void *worker(void *ptr)
{
    int my_rank = COMM_get_rank_id(); 
    size_t task_id;
    struct thread_data *d = ptr;
    struct byte_array task;
    struct result_node *result;

    workerid = d->id;

    void* (*worker_new) (int, char **);
    worker_new = dlsym(d->handle, "spits_worker_new");

    void (*execute_pit) (void *, struct byte_array *, struct byte_array *);
    execute_pit = dlsym(d->handle, "spits_worker_run");

    void *user_data = worker_new ? worker_new(d->argc, d->argv) : NULL;

    sem_wait (&d->tcount);                                      // wait for the first task to arrive.
    while (d->running) {
        pthread_mutex_lock(&d->tlock);                          // Get a new task.
        cfifo_pop(&d->f, &task);
        pthread_mutex_unlock(&d->tlock);

        // Warn the Task Manager about the new space available.
        sem_post(&d->sem);

        byte_array_unpack64(&task, &task_id);
        debug("[worker] Received TASK %d", task_id);
        
        _byte_array_pack64(&task, (uint64_t) task_id);          // Put it back, might use in execute_pit.
        result = malloc(sizeof(*result));
        byte_array_init(&result->ba, 10);
        byte_array_pack64(&result->ba, task_id);                // Pack the ID in the result byte_array.
        byte_array_pack64(&result->ba, my_rank);
        execute_pit(user_data, &task, &result->ba);             // Do the computation.
        byte_array_free(&task);

        pthread_mutex_lock(&d->rlock);                          // Pack the result to send it later.
        result->next = d->results;
        d->results = result;
        
        if(d->is_blocking_flush==1) {
            d->bf_remaining_tasks--;
            if(d->bf_remaining_tasks==0) {
                pthread_mutex_unlock(&d->bf_mutex);
            }
        }
            
        pthread_mutex_unlock(&d->rlock);

        sem_wait (&d->tcount);                                  // wait for the next task to arrive.
    }

    void* (*worker_free) (void *);
    worker_free = dlsym(d->handle, "spits_worker_free");
    if (worker_free) {
        worker_free(user_data);
    }

    //free(result);

    return NULL;
}

/* Send results to the committer, blocking or not.
 * Returns the number of tasks sent or -1 if found a connection problem. */
int flush_results(struct thread_data *d, int min_results, enum blocking b)
{
    int len = 0;
    struct result_node *aux, *n = d->results;

    for (aux = n; aux; aux = aux->next) {
        len++;
    }

    if (len <= min_results && b == NONBLOCKING) {
        return 0;
    }

    if (len > min_results && b == NONBLOCKING) {

        close(socket_manager);
        int tm_retries = 3;
        if(COMM_connect_to_job_manager(COMM_addr_manager, &tm_retries)!=0) {
            info("Couldn't reconnect to the Job Manager. Closing Task Manager.");
        }
        else {
            info("Reconnected to the Job Manager.");
        }
 

        
        aux = n->next;
        n->next = NULL;
        n = aux;
        while (n) {
            if(COMM_send_message(&n->ba, MSG_RESULT, socket_committer)<0) {
                error("Problem to send result to committer. Aborting flush_results.");
                return -1;
            }
            byte_array_free(&n->ba);
            aux = n->next;
            free(n);
            n = aux;
        }
        return len - 1;
    }

    if (b == BLOCKING) {
        
        if(len<min_results) {
            // If it's blocking and not yet complete.
            len = 0;
            n = d->results;

            pthread_mutex_lock(&d->rlock); 
            for (aux = n; aux; aux = aux->next) {
                len++;
            }
            d->is_blocking_flush=1;
            d->bf_remaining_tasks = min_results - len;
            pthread_mutex_unlock(&d->rlock);

            pthread_mutex_lock(&d->bf_mutex);
        }
        
        while (n) {
            if(COMM_send_message(&n->ba, MSG_RESULT,socket_committer)<0) {
                error("Problem to send result to committer. Aborting flush_results.");
                return -1;
            }
            byte_array_free(&n->ba);
            aux = n->next;
            free(n);
            n = aux;
        }
        return len;
    }

    return 0;
}

// Responsible for the thread that manages the Task Manager: receiving tasks,
// Sending results, requesting new tasks and etc.
void task_manager(struct thread_data *d)
{
    int alive = 1;                                                  // Indicate if it still alive.
    enum message_type type;                                         // Type of received message.
    int tasks = 0;                                                  // Tasks received and not committed.
    int min_results = 10;                                           // Minimum of results to send at the same time. 
    enum blocking b = NONBLOCKING;                                  // Indicates if should block or not in flushing.
    int comm_return=0;                                              // Return values from send and read.
    int flushed_tasks;                                              // Return value from flush_results.
    int tm_retries;

    // Data structure to exchange message between processes. 
    struct byte_array * ba = (struct byte_array *) malloc(sizeof(struct byte_array));
    byte_array_init(ba, 100);

    struct byte_array task;                                         // Used to pass tasks to other threads

    info("Starting task manager main loop");
    while (alive) {

        debug("Sending READY message to JOB_MANAGER");
        comm_return = COMM_send_message(NULL, MSG_READY, socket_manager);
        if(comm_return < 0) {
            error("Problem found to send message to Job Manager");
            type = MSG_EMPTY;
        }
        else {
            comm_return = COMM_read_message(ba, &type, socket_manager);
            if(comm_return < 0) {
                error("Problem found to read message from Job Manager");
                type = MSG_EMPTY;
            } 
        }

        switch (type) {
            case MSG_TASK:
                // Received at least one, mark to reuse id if connection problem occurs.
                if(received_one ==0 ) {
                    received_one = 1;
                }
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
            case MSG_EMPTY:
                COMM_close_connection(socket_manager);
                tm_retries = TM_CON_RETRIES;
                if(COMM_connect_to_job_manager(COMM_addr_manager, &tm_retries)!=0) {
                    info("Couldn't reconnect to the Job Manager. Closing Task Manager.");
                    alive = 0;
                }
                else {
                    info("Reconnected to the Job Manager.");
                }
                break;
            default:
                break;
        }

        if (alive) {
            debug("Trying to flush %d %s...", min_results, b == BLOCKING ? "blocking":"non blocking");
            flushed_tasks = flush_results(d, min_results, b);
            if(flushed_tasks < 0) {
                info("Couldn't flush results. Is committer still alive?");
                tm_retries = TM_CON_RETRIES;
                if(COMM_connect_to_committer(&tm_retries)<0) {
                    info("If it is, I just couldn't find it. Closing.");
                    alive = 0;
                }
                else {
                    info("Reconnected to the committer.");
                }
            }
            else {
                tasks -= flushed_tasks; 
                debug("I have sent %d tasks\n", flushed_tasks);
            }
            
        }
    }

    info("Terminating task manager");
    byte_array_free(ba);
    free(ba);
    COMM_close_all_connections();
}