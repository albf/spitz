#ifndef __SPITZ_H__
#define __SPITZ_H__

#ifdef __cplusplus
extern "C" {
#endif

// Job Manager DEFINEs.

#define RESTORE_RATE 10                 // Rate of JM loops/VM connection check.
#define JM_EXTRA_THREADS 1              // Number of threads to send tasks (if GEN_PARALLEL = 1, will also generate).
#define WAIT_REQUEST_TIMEOUT_SEC 1      // Values for request timeout.
#define WAIT_REQUEST_TIMEOUT_USEC 0
#define GEN_PARALLEL 0                  // if jm generation function can work in parallel. 
#define KEEP_REGISTRY 1                 // if jm will keep registry and avoid sending repeated tasks.
#define SAVE_REGISTRY 1                 // if jm will save registry of tasks to a file in in the end. (KEEP_REGISTRY=1 required).
#define SAVE_LIST 1                     // if should save the list of ips connected with ther info to a file.
    
// Comm DEFINEs

#define PORT_MANAGER 8898               // Number of ports used to communicate.
#define PORT_COMMITTER 10007
#define PORT_VM 11006
#define INITIAL_MAX_CONNECTIONS 30      // Initial max number of clientes conneceted (will increase if needed).
#define max_pending_connections 3

// Task Manager DEFINEs

#define TM_CON_RETRIES 3                // Retries of connection.
#define TASK_BUFFER_SIZE 10             // Number of tasks to get as buffer.
#define RESULT_BUFFER_SIZE 2            // Number of tasks required for a flush.
#define TM_MAX_SLEEP 16                 // Max sleep before asking again for a task.

// Arguments Checks                     // Used to remove warnings, as args may be used in the future.
#define CHECK_ARGS 0
#define ARGC_C 0
    
/*
 * Returns the worker id. If this method is called in a non-worker entity, it
 * returns -1.
 */
int spitz_get_worker_id(void);

/*
 * Returns the number of workers in this process.
 */
int spitz_get_num_workers(void);

extern char * lib_path;         // path of binary

#ifdef __cplusplus
}
#endif

#endif /* __SPITZ_H__ */
