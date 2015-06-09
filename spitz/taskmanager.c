/*
 * Copyright 2014 Ian Liu Rodrigues <ian.liu@ggaunicamp.com>
 * Copyright 2014 Alber Tabone Novo <alber.tabone@ggaunicamp.com>
 * Copyright 2014 Alexandre Luiz Brisighello Filho <albf.unicamp@gmail.com>
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
#include <sys/time.h>           // Wait some seconds in NO_TASK case.
#include "comm.h"
#include "log.h"
#include "journal.h"
#include "spitz.h"
#include <string.h>

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

// Get rand number in range.
int int_rand(int min, int max)
{
    int val = min + (min-max) * (double)rand() / (double)RAND_MAX + 0.5;
    return val; 
}

// Function responsible for the workers on current TM node.
void *worker(void *ptr)
{
    int my_rank = COMM_get_rank_id(); 
    int task_id, j_id=0;                                        // j_id = journal id for current thread.
    struct tm_thread_data *d = (struct tm_thread_data *) ptr;
    struct byte_array * task;
    struct result_node * result;
    uint64_t buffer;
    struct j_entry * entry;

    workerid = d->id;

    void* (*worker_new) (int, char **);
    worker_new = dlsym(d->handle, "spits_worker_new");

    void (*execute_pit) (void *, struct byte_array *, struct byte_array *);
    execute_pit = dlsym(d->handle, "spits_worker_run");

    void* (*worker_free) (void *);
    worker_free = dlsym(d->handle, "spits_worker_free");

    void *user_data = worker_new ? worker_new(d->argc, d->argv) : NULL;

    if(TM_KEEP_JOURNAL > 0) {
        j_id = JOURNAL_get_id(d->dia, 'W');
    }

    sem_wait (&d->tcount);                                      // wait for the first task to arrive.
    while (d->running) {
        pthread_mutex_lock(&d->tlock);                          // Get a new task.
        cfifo_pop(&d->f, &task);
        pthread_mutex_unlock(&d->tlock);

        // Warn the Task Manager about the new space available.
        sem_post(&d->sem);

        byte_array_unpack64(task, &buffer);
        task_id = (int) buffer;
        debug("[worker] Received TASK %d", task_id);
        
        //_byte_array_pack64(task, (uint64_t) task_id);           // Put it back, might use in execute_pit.
        result = (struct result_node *) malloc(sizeof(struct result_node));
        byte_array_init(&result->ba, 10);
        byte_array_pack64(&result->ba, task_id);                // Pack the ID in the result byte_array.
        byte_array_pack64(&result->ba, my_rank);

        if(TM_KEEP_JOURNAL > 0) {
            entry = JOURNAL_new_entry(d->dia, j_id);
            entry->action = 'P';
            gettimeofday(&entry->start, NULL);
        }

        debug("[--WORKER] task: %d", task);
        debug("[--WORKER] &result->ba: %d", &result->ba);
        execute_pit(user_data, task, &result->ba);              // Do the computation.

        if(TM_KEEP_JOURNAL > 0) {
            gettimeofday(&entry->end, NULL);
        }

        byte_array_free(task);                                  // Free memory used in task and pointer.
        free(task);                                             // For now, each pointer is allocated in master thread.

        debug("Appending task %d.", task_id);
        pthread_mutex_lock(&d->rlock);                          // Pack the result to send it later.
        result->next = d->results; 
        result->before = NULL;
        result->task_id = task_id;
        if(d->results != NULL) {
            d->results->before = result;
        }
        d->results = result;
        
        if(d->is_blocking_flush==1) {
            if(TM_NO_WAIT_FINAL_FLUSH > 0) {
                sem_post(&d->no_wait_sem);
            }
            else {
                d->bf_remaining_tasks--;
                if(d->bf_remaining_tasks==0) {
                    pthread_mutex_unlock(&d->bf_mutex);
                }
            }
        }
            
        pthread_mutex_unlock(&d->rlock);

        sem_wait (&d->tcount);                                  // wait for the next task to arrive.
    }

    if (worker_free) {
        worker_free(user_data);
    }

    //free(result);
    pthread_exit(NULL);
}

void vm_dump_journal(struct tm_thread_data *d) {
    char * filename, * j_info;

    //pthread_mutex_lock(&d->vm_dia);

    error("Starting dump");
    //filename = (char *) malloc (sizeof(char) * (strlen((char *) lib_path)+8));
    filename = (char *) malloc (sizeof(char) *15);
    filename[0] = 'v';
    filename[1] = 'm'; 
    filename[2] = '\0'; 
    //strcat(filename, (char *) lib_path);
    strcat(filename, ".tm.dia");

    j_info = JOURNAL_generate_info(d->dia, filename);
    debug(j_info);
    free(j_info);
    free(filename);

    error("End dump");
    //pthread_mutex_unlock(&d->vm_dia);
}

/* Send results to the committer, blocking or not.
 * Returns the number of tasks sent or -1 if found a connection problem. */
int flush_results(struct tm_thread_data *d, int min_results, enum blocking b, int j_id)
{
    int i, temp, len = 0;
    uint64_t buffer;
    struct result_node *aux, *n = d->results;
    struct j_entry * entry;
    struct byte_array * perm = NULL;
    enum message_type mtype;

    if(n) {
        len++;
        for (aux = n; aux->next; aux = aux->next) {
            len++;
        }
    }

    if (len < min_results && b == NONBLOCKING) {
        return 0;
    }

    else { 

        if(TM_ASK_TO_SEND_RESULT>0) {
            perm = (struct byte_array *) malloc (sizeof(struct byte_array));
            byte_array_init(perm , 10);
        }

        if (len >= min_results && b == NONBLOCKING) {

            /* DEBUG
            int i=0;
            for (i=0; i<max_clients; i++) {
               if(COMM_client_socket[i] == socket_manager) {
                   COMM_client_socket[i] = 0;
                   close(socket_manager);
               } 
            }
            int tm_retries = 3;
            if(COMM_connect_to_job_manager(COMM_addr_manager, &tm_retries)!=0) {
                info("Couldn't reconnect to the Job Manager. Closing Task Manager.");
            }
            else {
                info("Reconnected to the Job Manager.");
            }
             */  

            len = 0;
            while(aux) {
                pthread_mutex_lock(&d->rlock);
                n = aux;
                if(aux->before != NULL) {
                    aux->before->next = NULL;
                    aux = aux->before;
                }
                else {
                    d->results = NULL;
                    aux = NULL;
                }
                pthread_mutex_unlock(&d->rlock);

                if(TM_KEEP_JOURNAL > 0) {
                    entry = JOURNAL_new_entry(d->dia, j_id);
                    entry->action = 'S';
                    gettimeofday(&entry->start, NULL);
                }

                if(TM_ASK_TO_SEND_RESULT > 0) {
                    //debug("TM_ASK_TO_SEND_RESULT -> TASK_ID : %d", n->task_id);
                    byte_array_clear(perm);
                    buffer = (uint64_t) n->task_id;
                    //debug("TM_ASK_TO_SEND_RESULT -> TASK_ID(uint64_t) %" PRIu64 "\n", buffer);
                    byte_array_pack64(perm, buffer);
                    if(COMM_send_message(perm, MSG_OFFER_RESULT, socket_committer)<0) {
                        if(TM_KEEP_JOURNAL > 0) {
                            gettimeofday(&entry->end, NULL);

                            if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                error("Dumping VM journal");
                                vm_dump_journal(d);
                            }
                        }

                        error("Problem to send result to committer. Aborting flush_results.");

                        byte_array_free(perm);
                        free(perm);
                        return -1;
                    }

                    byte_array_clear(perm);
                    if(COMM_read_message(perm, &mtype, socket_committer)<0) {
                        if(TM_KEEP_JOURNAL > 0) {
                            gettimeofday(&entry->end, NULL);

                            if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                error("Dumping VM journal");
                                vm_dump_journal(d);
                            }
                        }

                        error("Problem receiving data from committer. Aborting flush_results.");

                        byte_array_free(perm);
                        free(perm);
                        return -1;
                    }
                    
                    byte_array_unpack64(perm, &buffer);
                    temp = (int) buffer;
                    //debug("TM_ASK_TO_SEND_RESULT -> temp: %d", temp);

                    if(temp > 0) {
                        if(COMM_send_message(&n->ba, MSG_RESULT, socket_committer)<0) {
                            if(TM_KEEP_JOURNAL > 0) {
                                gettimeofday(&entry->end, NULL);

                                if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                    error("Dumping VM journal");
                                    vm_dump_journal(d);
                                }
                            }

                            error("Problem sending result to committer. Aborting flush_results.");

                            byte_array_free(perm);
                            free(perm);
                            return -1;
                        }
                    }

                }
                else {
                    if(COMM_send_message(&n->ba, MSG_RESULT, socket_committer)<0) {
                        if(TM_KEEP_JOURNAL > 0) {
                            gettimeofday(&entry->end, NULL);

                            if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                error("Dumping VM journal");
                                vm_dump_journal(d);
                            }
                        }

                        error("Problem sending result to committer. Aborting flush_results.");

                        return -1;
                    }
                }

                if(TM_KEEP_JOURNAL > 0) {
                    gettimeofday(&entry->end, NULL);
                }

                byte_array_free(&n->ba);
                free(n);
            len++;
            }

            if(TM_ASK_TO_SEND_RESULT > 0) {
                byte_array_free(perm);
                free(perm);
            }

            return len;
        }

        else if (b == BLOCKING) {
            // Optional optimization. Will flush everything it got, as soon it arrives at the end.
            if(TM_NO_WAIT_FINAL_FLUSH > 0) {

                // If it's blocking and not yet complete.
                len = 0;
                n = d->results;

                pthread_mutex_lock(&d->rlock); 
                
                // Count and get a pointer to the last (older) result.
                if(n) {
                    len++;
                    for (aux = n; aux->next; aux = aux->next) {
                        len++;
                    }
                }
                d->is_blocking_flush=1;
                d->bf_remaining_tasks = min_results - len;
                pthread_mutex_unlock(&d->rlock);

                // Will send everyone.
                for(i=0; i<min_results; i++) {
                    // But first the ones already here (i < len). Don't have to wait. Then, wait (i >= len)
                    if(i >= len) {
                        sem_wait(&d->no_wait_sem);
                        for (aux = d->results; aux->next; aux = aux->next);
                    }

                    // Send message and update list, all standard.
                    n = aux;

                    if(TM_KEEP_JOURNAL > 0) {
                        entry = JOURNAL_new_entry(d->dia, j_id);
                        entry->action = 'S';
                        gettimeofday(&entry->start, NULL);
                    }

                    if(TM_ASK_TO_SEND_RESULT > 0) {
                        //debug("TM_ASK_TO_SEND_RESULT -> TASK_ID : %d", n->task_id);
                        buffer = (uint64_t) n->task_id;
                        byte_array_clear(perm);
                        //debug("TM_ASK_TO_SEND_RESULT -> TASK_ID(uint64_t) %" PRIu64 "\n", buffer);
                        byte_array_pack64(perm, buffer);
                        if(COMM_send_message(perm, MSG_OFFER_RESULT, socket_committer)<0) {
                            if(TM_KEEP_JOURNAL > 0) {
                                gettimeofday(&entry->end, NULL);

                                if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                    error("Dumping VM journal");
                                    vm_dump_journal(d);
                                }
                            }

                            error("Problem to send result to committer. Aborting flush_results.");

                            byte_array_free(perm);
                            free(perm);
                            return -1;
                        }

                        byte_array_clear(perm);
                        if(COMM_read_message(perm, &mtype, socket_committer)<0) {
                            if(TM_KEEP_JOURNAL > 0) {
                                gettimeofday(&entry->end, NULL);

                                if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                    error("Dumping VM journal");
                                    vm_dump_journal(d);
                                }
                            }

                            error("Problem receiving data from committer. Aborting flush_results.");

                            byte_array_free(perm);
                            free(perm);
                            return -1;
                        }
                        
                        byte_array_unpack64(perm, &buffer);
                        temp = (int) buffer;
                        //debug("TM_ASK_TO_SEND_RESULT -> temp : %d", temp);

                        if(temp > 0) {

                            if(COMM_send_message(&n->ba, MSG_RESULT, socket_committer)<0) {
                                if(TM_KEEP_JOURNAL > 0) {
                                    gettimeofday(&entry->end, NULL);

                                    if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                        error("Dumping VM journal");
                                        vm_dump_journal(d);
                                    }
                                }

                                error("Problem sending result to committer. Aborting flush_results.");

                                byte_array_free(perm);
                                free(perm);
                                return -1;
                            }
                        }

                    }
                    else {
                        if(COMM_send_message(&n->ba, MSG_RESULT, socket_committer)<0) {
                            if(TM_KEEP_JOURNAL > 0) {
                                gettimeofday(&entry->end, NULL);

                                if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                    error("Dumping VM journal");
                                    vm_dump_journal(d);
                                }
                            }

                            error("Problem sending result to committer. Aborting flush_results.");

                            return -1;
                        }
                    }

                    if(TM_KEEP_JOURNAL > 0) {
                        gettimeofday(&entry->end, NULL);
                    }

                    pthread_mutex_lock(&d->rlock);
                    if(aux->before != NULL) {
                        aux->before->next = NULL;
                        aux = aux->before;
                    }
                    else {
                        d->results = NULL;
                        aux = NULL;
                    }
                    pthread_mutex_unlock(&d->rlock);
                    byte_array_free(&n->ba);
                    free(n);
                }

            }
            else {
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
                
                for (aux = d->results; aux->next; aux = aux->next);

                len = 0;
                while(aux) {
                    pthread_mutex_lock(&d->rlock);
                    n = aux;
                    if(aux->before != NULL) {
                        aux->before->next = NULL;
                        aux = aux->before;
                    }
                    else {
                        d->results = NULL;
                        aux = NULL;
                    }
                    pthread_mutex_unlock(&d->rlock);

                    if(TM_KEEP_JOURNAL > 0) {
                        entry = JOURNAL_new_entry(d->dia, j_id);
                        entry->action = 'S';
                        gettimeofday(&entry->start, NULL);
                    }

                    if(TM_ASK_TO_SEND_RESULT > 0) {
                        //debug("TM_ASK_TO_SEND_RESULT -> TASK_ID : %d", n->task_id);
                        buffer = (uint64_t) n->task_id;
                        byte_array_clear(perm);
                        //debug("TM_ASK_TO_SEND_RESULT -> TASK_ID(uint64_t) %" PRIu64 "\n", buffer);
                        byte_array_pack64(perm, buffer);
                        if(COMM_send_message(perm, MSG_OFFER_RESULT, socket_committer)<0) {
                            if(TM_KEEP_JOURNAL > 0) {
                                gettimeofday(&entry->end, NULL);

                                if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                    error("Dumping VM journal");
                                    vm_dump_journal(d);
                                }
                            }

                            error("Problem to send result to committer. Aborting flush_results.");

                            byte_array_free(perm);
                            free(perm);
                            return -1;
                        }

                        byte_array_clear(perm);
                        if(COMM_read_message(perm, &mtype, socket_committer)<0) {
                            if(TM_KEEP_JOURNAL > 0) {
                                gettimeofday(&entry->end, NULL);

                                if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                    error("Dumping VM journal");
                                    vm_dump_journal(d);
                                }
                            }

                            error("Problem receiving data from committer. Aborting flush_results.");

                            byte_array_free(perm);
                            free(perm);
                            return -1;
                        }
                        
                        byte_array_unpack64(perm, &buffer);
                        temp = (int) buffer;
                        //debug("TM_ASK_TO_SEND_RESULT -> temp : %d", temp);

                        if(temp > 0) {

                            if(COMM_send_message(&n->ba, MSG_RESULT, socket_committer)<0) {
                                if(TM_KEEP_JOURNAL > 0) {
                                    gettimeofday(&entry->end, NULL);

                                    if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                        error("Dumping VM journal");
                                        vm_dump_journal(d);
                                    }
                                }

                                error("Problem sending result to committer. Aborting flush_results.");

                                byte_array_free(perm);
                                free(perm);
                                return -1;
                            }
                        }

                    }
                    else {
                        if(COMM_send_message(&n->ba, MSG_RESULT, socket_committer)<0) {
                            if(TM_KEEP_JOURNAL > 0) {
                                gettimeofday(&entry->end, NULL);

                                if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                                    error("Dumping VM journal");
                                    vm_dump_journal(d);
                                }
                            }

                            error("Problem sending result to committer. Aborting flush_results.");

                            return -1;
                        }
                    }

                    if(TM_KEEP_JOURNAL > 0) {
                        gettimeofday(&entry->end, NULL);
                    }

                    byte_array_free(&n->ba);
                    free(n);
                len++;
                }
            }

            if(TM_ASK_TO_SEND_RESULT > 0) {
                byte_array_free(perm);
                free(perm);
            }
            return len;
        }

        if(TM_ASK_TO_SEND_RESULT > 0) {
            byte_array_free(perm);
            free(perm);
        }
    }

    return 0;
}

// Function responsible for the flusher thread. Will make flushing parallel. 
void *flusher (void *ptr)
{
    struct tm_thread_data *d = (struct tm_thread_data *) ptr;
    int min_results, j_id=0;
    enum blocking b;
    int flushed_tasks, tm_retries;

    if(TM_KEEP_JOURNAL > 0) {
        j_id = JOURNAL_get_id(d->dia, 'F');
    }

    // Wait for new things to flush.
    sem_wait(&d->flusher_r_sem);
    while(d->flusher_min_results != -1) {
        // Get data to flush.
        min_results = d->flusher_min_results;
        b = d->flusher_b;
        pthread_mutex_unlock(&d->flusher_d_mutex);

        // Check if its exiting or not.
        if(b == BLOCKING) {
            min_results = d->tasks;
        }

        // Try to flush, update tasks counter if succedded.
        debug("FLUSHER: Flushing min_results : %d", min_results);
        debug("FLUSHER: d->tasks: %d", d->tasks);
        flushed_tasks = flush_results(d, min_results, b, j_id);
        if(flushed_tasks < 0) {
            info("Couldn't flush results. Is committer still alive?");
            tm_retries = TM_CON_RETRIES;
            if(COMM_connect_to_committer(&tm_retries)<0) {
                info("If it is, I just couldn't find it. Closing.");
                d->alive = 0;

                if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                    vm_dump_journal(d);
                }
            }
            else {
                info("Reconnected to the committer.");
            }
        }
        else {
            if(flushed_tasks > 0) {
                pthread_mutex_lock(&d->tasks_lock);
                d->tasks = d->tasks - flushed_tasks;
                pthread_mutex_unlock(&d->tasks_lock);
            }
            debug("I have sent %d tasks\n", flushed_tasks);
        }

        // Wait for new tasks to flush.
        sem_wait(&d->flusher_r_sem);
    }

    pthread_exit(NULL);
}

// Responsible for the thread that manages the Task Manager: receiving tasks,
// Sending results, requesting new tasks and etc.
void task_manager(struct tm_thread_data *d)
{
    int end = 0;                                                    // To indicate a true ending. Dead but fine. 
    enum message_type mtype;                                        // Type of received message.
    int min_results = TM_RESULT_BUFFER_SIZE;                        // Minimum of results to send at the same time. 
    enum blocking b = NONBLOCKING;                                  // Indicates if should block or not in flushing.
    int comm_return=0;                                              // Return values from send and read.
    int flushed_tasks;                                              // Return value from flush_results.
    int tm_retries;                                                 // Count the number of times TM tries to reconnect.
    int task_wait_max=1;                                            // Current max time of wait in CASE MSG_NO_TASK (sec)
    int wait;                                                       // wait(sec) in CASE MSG_NO_TASK
    int j_id=0;                                                     // Id from journal (if it exists).
    struct j_entry * entry;                                         // new entry for journal.

    // Data structure to exchange message between processes. 
    struct byte_array * ba;

    d->tasks = 0;                                                  // Tasks received and not committed.
    d->alive = 1;                                                  // Indicate if it still alive.
    srand (time(NULL));

    if(TM_KEEP_JOURNAL > 0) {
        j_id = JOURNAL_get_id(d->dia, 'M');
    }

    info("Starting task manager main loop");
    while (d->alive) {
        ba = (struct byte_array *) malloc(sizeof(struct byte_array));
        byte_array_init(ba, 100);

        debug("Sending READY message to JOB_MANAGER");

        if(TM_KEEP_JOURNAL > 0) {
            entry = JOURNAL_new_entry(d->dia, j_id);
            entry->action = 'R';
            gettimeofday(&entry->start, NULL);
        }

        comm_return = COMM_send_message(NULL, MSG_READY, socket_manager);
        if(comm_return < 0) {
            if(TM_KEEP_JOURNAL > 0) {
                gettimeofday(&entry->end, NULL);
            }


            if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                error("Dumping VM journal");
                vm_dump_journal(d);
            }

            error("Problem found sending message to Job Manager");
            mtype = MSG_EMPTY;
        }
        else {
            comm_return = COMM_read_message(ba, &mtype, socket_manager);
            if(TM_KEEP_JOURNAL > 0) {
                gettimeofday(&entry->end, NULL);
            }

            if(comm_return < 0) {
                error("Problem found to read message from Job Manager");
                mtype = MSG_EMPTY;

                if(COMM_get_actor_type() == VM_TASK_MANAGER) {
                    error("Dumping VM journal");
                    vm_dump_journal(d);
                }
            } 
        }

        switch (mtype) {
            case MSG_TASK:
                // Received at least one, mark to reuse id if connection problem occurs.
                if(received_one ==0 ) {
                    received_one = 1;
                }
                debug("waiting task buffer to free some space");
                sem_wait(&d->sem);

                pthread_mutex_lock(&d->tlock);
                cfifo_push(&d->f, &ba);
                pthread_mutex_unlock(&d->tlock);
                sem_post(&d->tcount);
                
                if(TM_FLUSHER_THREAD > 0) {
                    pthread_mutex_lock(&d->tasks_lock);
                    d->tasks++;
                    pthread_mutex_unlock(&d->tasks_lock);
                } 
                else {
                    d->tasks++;
                }

                break;
            case MSG_KILL:
                info("Got a KILL message");
                d->alive = 0;
                end = 1;
                b = BLOCKING;
                break;
            case MSG_EMPTY:
                COMM_close_connection(socket_manager);
                tm_retries = TM_CON_RETRIES;
                if(COMM_connect_to_job_manager(COMM_addr_manager, &tm_retries)!=0) {
                    info("Couldn't reconnect to the Job Manager. Closing Task Manager.");
                    d->alive = 0;
                }
                else {
                    info("Reconnected to the Job Manager.");
                }
                break;
            case MSG_NO_TASK:
                wait = int_rand(1, task_wait_max);
                debug("No task available, is it still loading? Sleeping for %d seconds", wait);
                sleep(wait);
                task_wait_max = task_wait_max * 2;
                if(task_wait_max > TM_MAX_SLEEP) {
                    task_wait_max = TM_MAX_SLEEP;
                }
                break;
            default:
                break;
        }

        if (d->alive || end) {
            debug("Trying to flush %d %s...", min_results, b == BLOCKING ? "blocking":"non blocking");
            if(TM_FLUSHER_THREAD > 0) {
                if((d->tasks >= min_results) || (b == BLOCKING)) {
                    pthread_mutex_lock(&d->flusher_d_mutex);
                    d->flusher_min_results = min_results;
                    d->flusher_b = b;
                    sem_post(&d->flusher_r_sem);
                }
            }
            else {
                if(b == BLOCKING) {
                    min_results = d->tasks;
                }

                flushed_tasks = 0;
                if(d->tasks >= min_results) {
                    flushed_tasks = flush_results(d, min_results, b, j_id);
                }

                if(flushed_tasks < 0) {
                    info("Couldn't flush results. Is committer still alive?");
                    tm_retries = TM_CON_RETRIES;
                    if(COMM_connect_to_committer(&tm_retries)<0) {
                        info("If it is, I just couldn't find it. Closing.");
                        d->alive = 0;
                    }
                    else {
                        info("Reconnected to the committer.");
                    }
                }
                else {
                    d->tasks = d->tasks - flushed_tasks; 
                    debug("I have sent %d tasks\n", flushed_tasks);
                }
            }
        }
    }

    info("Terminating task manager");
    byte_array_free(ba);
    free(ba);
}
