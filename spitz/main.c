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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/stat.h>
#include "cfifo.h"
#include "barray.h"
#include "log.h"
#include "jobmanager.h"
#include "committer.h"
#include "taskmanager.h"
#include "monitor.h"
#include "comm.h"
#include "registry.h"
#include "journal.h"
#include "spitz.h"
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

#define SPITZ_VERSION "0.1.0"
#define NON_BUFFERED_STDOUT 1  

int LOG_LEVEL = 0;

void * jm_create_thread(void *ptr) {
    int i;
	struct jm_thread_data * td = ptr;
    spitz_ctor_t ctor;

    debug("Starting to load shared data in a separated thread.");
    ctor = dlsym(td->handle, "spits_job_manager_new");
    td->user_data = ctor((td->argc), (td->argv));

    debug("Shared data loading done.");
    td->is_done_loading = 1;

    if((JM_GEN_THREADS > 0)&&(JM_GEN_BUFFER > 0)) {
        for(i=0; i < JM_GEN_THREADS; i++) {
            sem_post(&td->create_done_unlock);        
        }
    }

    pthread_exit(NULL);
}

void run(int argc, char *argv[], char *so, struct byte_array *final_result)
{
    int i=0;
    pthread_t * t; 
    pthread_t * t_loading; 
    lib_path = strcpy(malloc(sizeof(char)*strlen(so)+1), so);   // set lib path variable
    debug("Lib_path : %s", lib_path);                           // lib_path
    char * info, * filename, *j_info;                           // used to print registry info

    struct jm_thread_data td;

    if(JM_VM_RESTORE > 0) {
        i=1;
    }

    t = (pthread_t *) malloc(sizeof (pthread_t) * (JM_SEND_THREADS + JM_GEN_THREADS+i)); 
    t_loading = (pthread_t *) malloc(sizeof (pthread_t)); 

    // start task fifo list. 
    td.request_list = (struct request_FIFO *) malloc (sizeof(struct request_FIFO));
    td.request_list->first = NULL; 
    td.request_list->last = NULL;
    
    // starts semaphores
    pthread_mutex_init(&td.lock, NULL);
    sem_init(&td.num_requests, 0, 0);
    pthread_mutex_init(&td.tc_lock, NULL);    
    pthread_mutex_init(&td.tl_lock, NULL);    
    pthread_mutex_init(&td.gc_lock, NULL);    

    // Creates JM_GEN related variables.
    if(JM_GEN_THREADS > 0) {
        pthread_mutex_init(&td.gen_region_lock, NULL);    
        sem_init(&td.gen_request, 0, JM_GEN_BUFFER);
        sem_init(&td.gen_completed, 0, 0);
        td.gen_kill = 0;                                            // It starts alive, right?
    }

    // Creates JM_VM_HEALER related variables.
    if(JM_VM_RESTORE > 0) {
        td.vm_h_kill = 0;
        sem_init(&td.vm_lost, 0, 0);
    }
    
    // Start task counter and lock.
    td.task_counter = 0;
    td.all_generated = 0;                                           // No, not all tasks was generated at start. 
    td.is_finished = 0;
    td.num_tasks_total = 0; 
    td.is_num_tasks_total_found = 0; 
    td.g_counter = 0;

    // Start tasks list.
    td.tasks = (struct task_list *) malloc (sizeof (struct task_list));
    td.tasks->home = NULL;
    td.tasks->head = NULL;

    td.handle = dlopen(so, RTLD_LAZY); 
    if (!td.handle) {
        error("Could not open %s", so);
        return;
    }

    // initialize shared user data. 
    td.is_done_loading = 0;
    if((JM_GEN_THREADS > 0)&&(JM_GEN_BUFFER > 0)) {
        sem_init(&td.create_done_unlock, 0, 0);
    }
    td.argc = argc;
    td.argv = argv;
    pthread_create(t_loading, NULL, jm_create_thread, &td);

    // Create journal, if used, for the job manager.
    if(JM_KEEP_JOURNAL > 0) {
        i = 1 + JM_SEND_THREADS + JM_GEN_THREADS;
        if(JM_VM_RESTORE > 0) {
            i++;
        }
        td.dia = (struct journal *) malloc (sizeof(struct journal));
        debug("Creating journal with %d ids.", i);
        JOURNAL_init(td.dia, i);
    }

    // Create extra-thread(s)
    for (i=0; i < JM_SEND_THREADS; i++) {
        pthread_create(&t[i], NULL, jm_worker, &td);
    }

    // Create generate thread if gen is not parallel. 
    for (i=0; i < JM_GEN_THREADS; i++) {
        pthread_create(&t[JM_SEND_THREADS+i], NULL, jm_gen_worker, &td);
    }

    // Create thread responsible for the VM restore.
    if(JM_VM_RESTORE > 0) {
        pthread_create(&t[JM_SEND_THREADS+JM_GEN_THREADS], NULL, jm_vm_healer, &td);
    }

    // Initialize registry structures if applicable. 
    if(JM_KEEP_REGISTRY > 0) {
        pthread_mutex_init(&td.registry_lock, NULL);    
        td.registry_capacity = 1024;
        td.registry = (struct task_registry **) calloc(1024, sizeof(struct task_registry *));
        td.abs_zero = (struct timeval *) malloc(sizeof(struct timeval));
        gettimeofday(td.abs_zero, NULL);
    }
    
    job_manager(argc, argv, so, final_result, &td);

    // Exit VM_HEALER thread.
    if(JM_VM_RESTORE > 0) {
        td.vm_h_kill = 1;
        sem_post(&td.vm_lost);
    }

    for (i = 0; i < (JM_SEND_THREADS + JM_GEN_THREADS); i++) {    // Join them all
        pthread_join(t[i], NULL);
    }

    if(JM_VM_RESTORE > 0) {
        pthread_join(t[JM_SEND_THREADS + JM_GEN_THREADS], NULL);
    }

    if((JM_SAVE_REGISTRY > 0) || ((JM_KEEP_REGISTRY > 0)&&(JM_SAVE_REGISTRY > 0))) {
        filename = strcpy(malloc((sizeof(char)*strlen(so)+1)+5), so);
        if((JM_KEEP_REGISTRY > 0)&&(JM_SAVE_REGISTRY > 0)) {
            filename = strcat(filename, ".reg");
            debug("Saving registry in file: %s.", filename);
            info = REGISTRY_generate_info(&td, filename);
            debug(info);
            free(info);
            REGISTRY_free(&td);
        }   

        filename = strcpy(filename, so);
        if(JM_SAVE_LIST > 0) {
            filename = strcat(filename, ".list");
            debug("Saving list in file: %s.", filename);
            info = LIST_generate_info(COMM_ip_list, filename); 
            debug(info);
            free(info);
        }   
        free(filename);
    }
    
    // Clean journal memory
    if(JM_KEEP_JOURNAL > 0) {
        if(JM_SAVE_JOURNAL > 0) {
            filename = (char *) malloc (sizeof(char)*(strlen(so)+8));
            filename[0] = '\0';
            strcat(filename, (char *) so);
            strcat(filename, ".jm.dia");

            j_info = JOURNAL_generate_info(td.dia, filename);
            debug(j_info);
            free(j_info);
            free(filename);
        }
        else {
            j_info = JOURNAL_generate_info(td.dia, NULL);
            debug(j_info);
            free(j_info);
        }
        JOURNAL_free(td.dia);
    }
            
    LIST_free_data(COMM_ip_list);
    free(lib_path);
    free(t);
    free(t_loading);
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
    
    if(ptr) {
        dlclose(ptr);
    }

    // Warned everyone, just closes the program. 
    info("Terminating SPITZ");
}

// Start of a slave process (task managers or committer)
void start_slave_processes(int argc, char *argv[])
{
    struct byte_array * lib_path = (struct byte_array *) malloc(sizeof(struct byte_array));
    struct byte_array * ba_binary = (struct byte_array *) malloc(sizeof(struct byte_array));
    struct byte_array * ba_hash = (struct byte_array *) malloc(sizeof(struct byte_array));
    struct byte_array * ba_hash_jm = (struct byte_array *) malloc(sizeof(struct byte_array));
    enum message_type mtype;
    int msg_return;
    int is_binary_correct=0;
    struct stat buffer;
    void *handle = NULL;
    int i, tmid;                    // Auxiliary
    pthread_t * t;                  // Worker threads
    pthread_t * t_flusher;          // Flusher thread (Task Manager)
    pthread_t * t_read;             // Reader thread (Committer)
    int NTHREADS = 1;               // Number of threads
    struct tm_thread_data d;
    struct cm_thread_data cd;
    struct result_node * aux;
    char * j_info, * filename;

    // Request and get the path from the job manager. If get disconnected, retry.
    do {
        msg_return = COMM_send_message(NULL, MSG_GET_PATH, socket_manager);
        if(msg_return < 0) {
            error("Problem sending GET_PATH to Job Manager.");
        }
        else {
            byte_array_init(lib_path, 0);
            msg_return = COMM_read_message(lib_path, &mtype, socket_manager);

            if(msg_return < 0) {
                error("Problem reading GET_PATH from Job Manager.");
            }
            else {
                info("Successfully received path from Job Manager.");
            }
        }

        if(msg_return < 0) {
            COMM_close_connection(socket_manager); 
            COMM_connect_to_job_manager(COMM_addr_manager,NULL);
        }    
    } while (msg_return < 0);

    
    // Request and get the hash from the job manager. If get disconnected, retry.
    do {
        msg_return = COMM_send_message(NULL, MSG_GET_HASH, socket_manager);
        if(msg_return < 0) {
            error("Problem sending GET_HASH to Job Manager.");
        }
        else {
            byte_array_init(ba_hash_jm, 0);
            msg_return = COMM_read_message(ba_hash_jm, &mtype, socket_manager);

            if(msg_return < 0) {
                error("Problem reading GET_HASH from Job Manager.");
            }
            else {
                info("Successfully received hash from Job Manager.");
            }
        }

        if(msg_return < 0) {
            COMM_close_connection(socket_manager); 
            COMM_connect_to_job_manager(COMM_addr_manager,NULL);
        }    
    } while (msg_return < 0);

   
    // If the file exists, check if it has the same hash.
    if (stat ((char *) (lib_path->ptr), &buffer) == 0) {
        byte_array_init(ba_binary,0);      
        byte_array_pack_binary(ba_binary, (char *) (lib_path->ptr));
        
        byte_array_init(ba_hash,0);
        byte_array_compute_hash(ba_hash, ba_binary);

        // If file with same name is actually the same.
        if(strcmp((char *)ba_hash->ptr, (char *)ba_hash_jm->ptr)==0) {
            is_binary_correct = 1;
            info("File already exist and has the right hash.");
        }
        // If it's not, remove it, and request the correct from the server.
        else {
            info("Wrong hash in the file, request a new binary.");
            remove((char *) (lib_path->ptr));
            
            byte_array_free(ba_binary);
            byte_array_free(ba_hash);
        }
    }
    
    /* Request and get the binary and hash from the job manager.  
     * If get disconnected or hash doesn't match, retry. After that, unpacks it. */
    while (is_binary_correct == 0){
        msg_return = COMM_send_message(NULL, MSG_GET_BINARY, socket_manager);
        if(msg_return < 0) {
            error("Problem sending GET_BINARY to Job Manager.");
        }
        else {
            byte_array_init(ba_binary, 0);
            msg_return = COMM_read_message(ba_binary, &mtype, socket_manager);

            if(msg_return < 0) {
                error("Problem reading GET_BINARY from Job Manager.");
            }
            else {
                info("Successfully received binary from Job Manager.");
                
                byte_array_init(ba_hash, 0);
                byte_array_compute_hash(ba_hash, ba_binary);

                // Check if hash is actually valid (may received NULL files in a manager's mistake).
                if(ba_hash->ptr == NULL) {
                    msg_return = -1;
                }
                else {
                    // if the hashes are equal, finish it. If not, do it again.
                    if(strcmp((char *)ba_hash->ptr, (char *)ba_hash_jm->ptr)==0) {
                        is_binary_correct = 1; 
                    }
                    else {
                        error("Difference in MD5 hash of binary, will request binary again.");
                    }
                }
            }
        }

        // If failed in some point, restart the connection with job manager.
        if(msg_return < 0) {
            COMM_close_connection(socket_manager); 
            COMM_connect_to_job_manager(COMM_addr_manager,NULL);
        }
        else {
 
            // DEBUG START - Check local multiple binary files. //
            /*char * path_debug;
            path_debug = (char *) (lib_path->ptr);
            switch(COMM_get_rank_id()) {
                case 1:
                    path_debug[strlen(path_debug)-4] = '1';
                    break;
                case 2:
                    path_debug[strlen(path_debug)-4] = '2';
                    break;
                case 3:
                    path_debug[strlen(path_debug)-4] = '3';
                    break;
                case 4:
                    path_debug[strlen(path_debug)-4] = '4';
                    break;
                case 5:
                    path_debug[strlen(path_debug)-4] = '5';
                    break;
            } */
            // DEBUG END //

            
            byte_array_unpack_binary(ba_binary, (char *) lib_path->ptr);   
    
        }
    }     

    
    while (strncmp((char *) lib_path->ptr, "NULL", 4) != 0) {
        info("received a module to run %s", lib_path->ptr);

        handle = dlopen((char *) lib_path->ptr, RTLD_LAZY);
        if (!handle) {
            error("could not open %s", (char *) lib_path->ptr);
            return;
        }

        if (COMM_get_rank_id() == (int) COMMITTER) {
            cd.handle = handle;

            if(CM_KEEP_JOURNAL > 0) {
                i = 1;
                if(CM_COMMIT_THREAD > 0) {
                    i++;
                }
                if(CM_READ_THREADS > 0) {
                    i = i+CM_READ_THREADS;
                }
                cd.dia = (struct journal *) malloc (sizeof(struct journal));
                JOURNAL_init(cd.dia, i);
            }

            if(CM_COMMIT_THREAD > 0 ) {
                cd.results = NULL;
                cd.head = NULL;
                sem_init (&cd.r_counter, 0, 0);
                pthread_mutex_init(&cd.r_lock, NULL);
                pthread_mutex_init(&cd.f_lock, NULL);
                pthread_mutex_trylock(&cd.f_lock);
                cd.results = NULL;

                t = (pthread_t *) malloc(sizeof (pthread_t)); 
                pthread_create(t, NULL, commit_worker, &cd);
            }

            if(CM_READ_THREADS > 0) {
                cd.r_kill = 0;
                sem_init (&cd.blacklist, 0, 0);
                cd.sb = (struct socket_blacklist *) malloc(sizeof(struct socket_blacklist));
                cd.sb->size = 0;
                cd.sb->home = NULL;
                cd.sb->head = NULL;
                pthread_mutex_init(&cd.sb_lock, NULL);

                t_read = (pthread_t *) malloc(sizeof (pthread_t)*CM_READ_THREADS); 
                for (i=0; i < CM_READ_THREADS; i++) {
                    pthread_create(&t_read[i], NULL, read_worker, &cd);
                }

            }

            committer(argc, argv, &cd);

            if(CM_COMMIT_THREAD > 0 ) {
                pthread_join(*t, NULL);
            }

            if(CM_READ_THREADS > 0) {
                cd.r_kill = 1;

                for(i=0; i<CM_READ_THREADS; i++) {
                    sem_post(&cd.blacklist);
                }

                for(i=0; i<CM_READ_THREADS; i++) {
                    pthread_join(t_read[i], NULL);
                }
                free(cd.sb);
                free(t_read);
            }

            // Clean journal memory
            if(CM_KEEP_JOURNAL > 0) {
                if(CM_SAVE_JOURNAL > 0) {
                    filename = (char *) malloc (sizeof(char) * (strlen((char *) lib_path->ptr)+8));
                    filename[0] = '\0';
                    strcat(filename, (char *) lib_path->ptr);
                    strcat(filename, ".cm.dia");
   
                    j_info = JOURNAL_generate_info(cd.dia, filename);
                    debug(j_info);
                    free(j_info);
                    free(filename);
                }
                else {
                    j_info = JOURNAL_generate_info(cd.dia, NULL);
                    debug(j_info);
                    free(j_info);
                }
                JOURNAL_free(cd.dia);
            }

            if(CM_COMMIT_THREAD > 0 ) {
                free(t);
            }
        } 
        else {                                  // Else : Task Manager or VM Task Manager
            //pthread_t t[NTHREADS];
            if(TM_IDENTIFY_CORES > 0) {
                NTHREADS = get_number_of_cores();
            }
            else {
                NTHREADS = TM_NUM_CORES;
            }

            t = (pthread_t *) malloc(sizeof (pthread_t) * NTHREADS); 
             
            cfifo_init(&d.f, sizeof(struct byte_array *), TM_TASK_BUFFER_SIZE);
            sem_init(&d.sem, 0, TM_TASK_BUFFER_SIZE);
            sem_init (&d.tcount, 0, 0);
            pthread_mutex_init(&d.tlock, NULL);
            pthread_mutex_init(&d.rlock, NULL);
            pthread_mutex_init(&d.bf_mutex, NULL);
            pthread_mutex_lock(&d.bf_mutex);
            d.running = 1;
            d.results = NULL;
            d.handle = handle;
            d.argc = argc;
            d.argv = argv;
            d.is_blocking_flush = 0;            // Notify that is not in a blocking flush.
    
            if(type == VM_TASK_MANAGER) {
                pthread_mutex_init(&d.rlock, NULL);
            }

            if(TM_KEEP_JOURNAL > 0) {
                i = 1 + NTHREADS;
                if(TM_FLUSHER_THREAD > 0) {
                    i++;
                }
                d.dia = (struct journal *) malloc (sizeof(struct journal));
                JOURNAL_init(d.dia, i);
            }

            tmid = COMM_get_rank_id();
            for (i = 0; i < NTHREADS; i++) {
                d.id = NTHREADS * tmid + i;
                pthread_create(&t[i], NULL, worker, &d);
            }

            if(TM_FLUSHER_THREAD > 0) {
                t_flusher = (pthread_t *) malloc(sizeof (pthread_t)); 
                sem_init(&d.flusher_r_sem, 0, 0);
                pthread_mutex_init(&d.flusher_d_mutex, NULL);
                pthread_mutex_init(&d.tasks_lock, NULL);
                pthread_create(t_flusher, NULL, flusher, &d);
            }

            if(TM_NO_WAIT_FINAL_FLUSH > 0) {
                sem_init(&d.no_wait_sem, 0, 0);
            }

            task_manager(&d);

            if(TM_FLUSHER_THREAD > 0) {            // Finish Flusher
                pthread_mutex_lock(&d.flusher_d_mutex);
                d.flusher_min_results = -1;
                sem_post(&d.flusher_r_sem);
                pthread_join(*t_flusher, NULL);
                free(t_flusher);
            }

            d.running = 0;                      // Finish the running threads
            for(i=0; i< NTHREADS; i++) {
                sem_post(&d.tcount);
            }

            for (i = 0; i < NTHREADS; i++) {    // Join them all
                pthread_join(t[i], NULL);
            }

            COMM_close_all_connections();       // Close connections, now that is safe.

            // clean memory from results, as it may have some there
            aux = d.results;
            while(aux!= NULL) {
                d.results = d.results->next;
                byte_array_free(&(aux->ba));
                free(aux);
                aux = d.results;
            }

            // Clean journal memory
            if(TM_KEEP_JOURNAL > 0) {
                if(TM_SAVE_JOURNAL > 0) {
                    filename = (char *) malloc (sizeof(char) * (strlen((char *) lib_path->ptr)+8));
                    filename[0] = '\0';
                    strcat(filename, (char *) lib_path->ptr);
                    strcat(filename, ".tm.dia");
   
                    j_info = JOURNAL_generate_info(d.dia, filename);
                    debug(j_info);
                    free(j_info);
                    free(filename);
                }
                else {
                    j_info = JOURNAL_generate_info(d.dia, NULL);
                    debug(j_info);
                    free(j_info);
                }
                JOURNAL_free(d.dia);
            }
            
            cfifo_free(&d.f);
            free(t);
        }
        
        // --- ONE RUN FOR NOW
        //lib_path = COMM_get_path();
        break;                                  // One run, so get out
    }

    // Clean dlopen memory.
    if(handle != NULL) {
        dlclose(handle);
    }
    
    // Clean memory used in byte arrays.
    byte_array_free(lib_path);
    byte_array_free(ba_binary);
    byte_array_free(ba_hash);
    byte_array_free(ba_hash_jm);
    free(lib_path);
    free(ba_binary);
    free(ba_hash);
    free(ba_hash_jm);
}

int main(int argc, char *argv[])
{
    //enum actor type=atoi(argv[1]);                                  
    struct sigaction sa;
    COMM_set_actor_type(argv[1]);                                   // Get the actor type by parameter.
    received_one = 0;                                               // New node, received nothing.
    
    if((type!=JOB_MANAGER)&&(type!=COMMITTER)&&(type!=TASK_MANAGER)&&(type!=MONITOR)&&(type!=VM_TASK_MANAGER)) {
        error("Wrong type specified in argv[1], try again.");
        return -1;
    }

    if(NON_BUFFERED_STDOUT > 0) {
        setvbuf(stdout, (char *) NULL, _IOLBF, 0);                  // Make line buffered stdout.
        setvbuf(stderr, (char *) NULL, _IOLBF, 0);                  // Make line buffered stdout.
    }

    error("SPITZ Started, actor number: %d", (int) type);
    
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa, 0) == -1) {
        error("Couldn't ignore SIGPIPE.");
        return -1;
    }
   
    COMM_my_rank = (int) type;
    if(type==JOB_MANAGER) {
        COMM_setup_job_manager_network(argc , argv);
    }
    if(type == VM_TASK_MANAGER) {
        COMM_setup_vm_network();        
        COMM_vm_connection(1,1);
    }
    else {
        COMM_addr_manager = strcpy(malloc((strlen(argv[2])+1)*sizeof(char)), argv[2]);
        
        COMM_connect_to_job_manager(COMM_addr_manager,NULL);
        lib_path = NULL;                                            // Will get the lib_path later.
        
        if(type==COMMITTER) { 		                                // The committer sets itself in the Job Manager
            COMM_setup_committer_network();
        }
        else if(type==TASK_MANAGER) {                	            // Task Managers get and connect to the committer
            COMM_connect_to_committer(NULL);
        }
    }
    
    char *debug = getenv("SPITS_DEBUG_SLEEP");                      // Get environment variables and check usage.
    if (debug) {
        int amount = atoi(debug);
        pid_t pid = getpid();
        printf("Rank %d at pid %d\n", COMM_get_rank_id(), pid);
        sleep(amount);
    }

    if((SPITS_LOG_LEVEL < 0) || (SPITS_LOG_LEVEL > 2)) {
        error("Not valid SPITS_LOG_LEVEL used, use a integer in [0, 2]. Using 2 for now");
        LOG_LEVEL = 2;
    }
    else {
        LOG_LEVEL = SPITS_LOG_LEVEL; 
    }

    if (type == JOB_MANAGER && LOG_LEVEL >= 1) {
        printf("Welcome to spitz " SPITZ_VERSION "\n");
    }

    if (type == JOB_MANAGER && argc < 2) {
        fprintf(stderr, "Usage: SO_PATH\n");
        return EXIT_FAILURE;
    }

    char *so = argv[3];
    
    /* Remove the first four arguments */
    argc -= 4;
    argv += 4;

    if (type == JOB_MANAGER) {
        start_master_process(argc, argv, so);
    }
    else if((type == TASK_MANAGER) || (type == COMMITTER) || (type == VM_TASK_MANAGER)) {
        start_slave_processes(argc, argv);
    }
    else if (type == MONITOR) {
        monitor(argc, argv); 
    }

    // Clean memory for sockets.
    if((type==JOB_MANAGER)||(type==COMMITTER)||(type!=VM_TASK_MANAGER)) {
        free(COMM_client_socket);
    }

    free(COMM_addr_manager);
    return 0;
}
