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
#include "spitz.h"

#define SPITZ_VERSION "0.1.0"
#define NON_BUFFERED_STDOUT 1  

int LOG_LEVEL = 0;
int FIFOSZ = 10;
int NTHREADS = 1; 

void * jm_create_thread(void *ptr) {
	struct jm_thread_data * td = ptr;
    spitz_ctor_t ctor;

    debug("Starting to load shared data in a separated thread.");
    ctor = dlsym(td->handle, "spits_job_manager_new");
    td->user_data = ctor((td->argc), (td->argv));

    debug("Shared data loading done.");
    td->is_done_loading = 1;
    pthread_exit(NULL);
}

void run(int argc, char *argv[], char *so, struct byte_array *final_result)
{
    int i;
    int gen_threads;
    pthread_t * t; 
    pthread_t * t_loading; 
    lib_path = strcpy(malloc(sizeof(char)*strlen(so)+1), so);         // set lib path variable

    struct jm_thread_data td;

    if(GEN_PARALLEL == 0) {
        gen_threads = 1;
    }
    else {
        gen_threads = 0;
    }
    
    t = (pthread_t *) malloc(sizeof (pthread_t) * (JM_EXTRA_THREADS + gen_threads)); 
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

    if(GEN_PARALLEL == 0) {
        pthread_mutex_init(&td.gen_region_lock, NULL);    
        pthread_mutex_init(&td.gen_ready_lock, NULL);    
        pthread_mutex_init(&td.jm_gen_lock, NULL);    
        // jm_gen_lock and gen_ready_lock start locked.
        pthread_mutex_trylock(&td.jm_gen_lock);
        pthread_mutex_trylock(&td.gen_ready_lock);
    }
    
    // Initialize registry structures if applicable. 
    if(KEEP_REGISTRY > 0) {
        pthread_mutex_init(&td.registry_lock, NULL);    
        td.registry_capacity = 1024;
        td.registry = (struct task_registry **) calloc(1024, sizeof(struct task_registry *));
    }

    // Start task counter and lock.
    td.task_counter = 0;
    td.all_generated = 0;                                           // No, not all tasks was generated at start. 
    td.is_finished = 0;
    td.num_tasks_total = atoi(argv[0]);
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
    td.is_done_loading = 1;
    td.argc = argc;
    td.argv = argv;
    pthread_create(t_loading, NULL, jm_create_thread, &td);

    // Create extra-thread(s)
    for (i=0; i < JM_EXTRA_THREADS; i++) {
        pthread_create(&t[i], NULL, jm_worker, &td);
    }

    // Create generate thread if gen is not parallel. 
    if(GEN_PARALLEL == 0) {
        pthread_create(&t[JM_EXTRA_THREADS], NULL, jm_gen_worker, &td);
    }
    
    job_manager(argc, argv, so, final_result, &td);

    for (i = 0; i < JM_EXTRA_THREADS; i++) {    // Join them all
        pthread_join(t[i], NULL);
    }
    
    free(lib_path);
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
    enum message_type type;
    int msg_return;
    int is_binary_correct=0;
    struct stat buffer;
    void *handle = NULL;

    // Request and get the path from the job manager. If get disconnected, retry.
    do {
        msg_return = COMM_send_message(NULL, MSG_GET_PATH, socket_manager);
        if(msg_return < 0) {
            error("Problem sending GET_PATH to Job Manager.");
        }
        else {
            byte_array_init(lib_path, 0);
            msg_return = COMM_read_message(lib_path, &type, socket_manager);

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
            msg_return = COMM_read_message(ba_hash_jm, &type, socket_manager);

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
            msg_return = COMM_read_message(ba_binary, &type, socket_manager);

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
 
            // DEBUG START //
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

            
            byte_array_unpack_binary(ba_binary, lib_path->ptr);   
    
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
            committer(argc, argv, handle);
        } 
        else {                                  // Else : Task Manager or VM Task Manager
            //pthread_t t[NTHREADS];
            if(NTHREADS == -1) {
                NTHREADS = get_number_of_cores();
            }
            pthread_t * t = (pthread_t *) malloc(sizeof (pthread_t) * NTHREADS); 
            
            
            struct tm_thread_data d;
            struct result_node * aux;
            
            cfifo_init(&d.f, sizeof(struct byte_array *), FIFOSZ);
            sem_init(&d.sem, 0, FIFOSZ);
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

            int i, tmid = COMM_get_rank_id();
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

            // clean memory from results, as it may have some there
            aux = d.results;
            while(aux!= NULL) {
                d.results = d.results->next;
                byte_array_free(&(aux->ba));
                free(aux);
                aux = d.results;
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

    info("SPITZ Started, actor number: %d", (int) type);
    
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
        
        if(type==COMMITTER) { 		                            // The committer sets itself in the Job Manager
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

    char *loglvl = getenv("SPITS_LOG_LEVEL");
    if (loglvl) {
        LOG_LEVEL = atoi(loglvl);
    }
    else {
        LOG_LEVEL = 2;
    }

    char *nthreads = getenv("SPITS_NUM_THREADS");
    if (nthreads) {
        NTHREADS = atoi(nthreads);
    }
    else {
        NTHREADS=1;
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

    free(COMM_addr_manager);
    return 0;
}
