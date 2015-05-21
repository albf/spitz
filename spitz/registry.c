/*
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

#include "registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

// Adds an registry of task being sent to an Task Manager.
void REGISTRY_add_registry(struct jm_thread_data *td, int task_id, int tm_id) {
    int changed = 0;
    int initial_size, i;
    struct task_registry * ptr;
    struct task_registry * next;

    debug("Registry: adding registry for task : %d to taskmanager : %d", task_id, tm_id);
    pthread_mutex_lock(&td->registry_lock);

    // First check the size, if found a task never registered before.
    initial_size = td->registry_capacity;
    while(task_id >= (td->registry_capacity)) {
        changed = 1;
        td->registry_capacity = td->registry_capacity*2;
    }

    if(changed > 0) {
        td->registry = (struct task_registry **) realloc(td->registry, td->registry_capacity*sizeof(struct task_registry *));
        for(i=initial_size; i < td->registry_capacity; i++) {
            td->registry[i] = NULL;
        }
    }

    // Create new entry
    ptr = (struct task_registry *) malloc (sizeof(struct task_registry));
    ptr->tm_id = tm_id;
    ptr->task_id = task_id;

    // Save time information 
    ptr->send_time = (struct timeval *) malloc(sizeof(struct timeval));
    gettimeofday(ptr->send_time, NULL);
    ptr->completed_time = NULL;
    ptr->next = NULL;

    // Insert new entry in registry.
    if(td->registry[task_id] == NULL) {
        td->registry[task_id] = ptr;
    }
    else {
        next = td->registry[task_id];
        while(next->next != NULL) {
            next = next->next;
        }
        next->next = ptr;
    }

    pthread_mutex_unlock(&td->registry_lock);
}

// Add information about task completion (creating completed time for this entry).
void REGISTRY_add_completion_registry (struct jm_thread_data *td, size_t task_id, int tm_id) {
    struct task_registry * ptr;
    pthread_mutex_lock(&td->registry_lock);
    ptr = td->registry[task_id];
    debug("Adding Completion Registry: Task %d for TaskManager %d", task_id, tm_id);

    // Look for entry that completed task in registry.
    while((ptr != NULL)&&(ptr->tm_id != tm_id)) {
        ptr = ptr->next;
    }

    // How can it be completed if it wasn't registered during the send?
    if(ptr == NULL) {
        error("Error in registry update. Couldn't found Task Manager %d in Registry[%d]", tm_id, (int) task_id);
        pthread_mutex_unlock(&td->registry_lock);
        return;
    }
 
    // Add completion time.
    ptr->completed_time = (struct timeval *) malloc(sizeof(struct timeval));
    gettimeofday(ptr->completed_time, NULL);
    //debug("Completion Registry added.");
    pthread_mutex_unlock(&td->registry_lock);
}

// Indicate if task_id was already sent to tm_id. 1 if yes, 0 if no.
int REGISTRY_check_registry(struct jm_thread_data * td, int task_id, int tm_id) {
    struct task_registry * ptr;
    pthread_mutex_lock(&td->registry_lock);

    // Don't even have the space for this index.
    if(task_id >= (td->registry_capacity)) {
        error("Registry check of unregistered task (not allocated space) : %d", task_id);
        pthread_mutex_unlock(&td->registry_lock);
        return 0;
    }

    ptr = td->registry[task_id];
    
    // Nothing registered for this task. Will be send for the first time.
    if(ptr == NULL) {
        error("Registry check of unregistered task (NULL pointer) : %d", task_id);
        pthread_mutex_unlock(&td->registry_lock);
        return 0;
    }

    // Look for this particular task manager.
    while((ptr!=NULL)&&(ptr->tm_id != tm_id)) {
        ptr=ptr->next;
    }

    pthread_mutex_unlock(&td->registry_lock);

    // check if it was sent or not.
    if(ptr == NULL) {
        return 0;
    }
    else {
        return 1;
    }

    // You never know. 
    error("check_registry bug exit.");
}

// Add information about task completion.
void REGISTRY_free(struct jm_thread_data *td) {
    int i;
    struct task_registry * ptr;
    struct task_registry * free_ptr;

    // Each index
    for(i=0; i < td->registry_capacity; i++) {
        ptr = td->registry[i];
        // Each entry in index
        while(ptr != NULL) {
            free_ptr = ptr;	
            ptr = ptr->next;
            free(free_ptr->send_time);
            if(free_ptr->completed_time != NULL) {
                free(free_ptr->completed_time);
            }
            free(free_ptr);
        }
    }

    free(td->abs_zero);
    free(td->registry);
    debug("Registry memory freed.");
}

/* Return 1 if the difference is negative, otherwise 0.  */
int timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1)
{
    long int diff = (t2->tv_usec + 1000000 * t2->tv_sec) - (t1->tv_usec + 1000000 * t1->tv_sec);
    result->tv_sec = diff / 1000000;
    result->tv_usec = diff % 1000000;

    return (diff<0);
}

// Mount registry info, saving in file or not. If filename == NULL, will return the info.
// If filename != NULL will save in this file and will return the info.
// Format: task_id(12)|whoCompleted(id or -1)(9)|processed_time(Sent-Finish)|
    // sent_counter(12)|total_time_alive(Generated-Finish)(30)|absolute_completed_time(30);
// TotalSize : 1 + 12 + 1 + 9 + 1 + 30 1 + 12 + 1 + 30 + 1 + 30 + 1 <= 130
char * REGISTRY_generate_info(struct jm_thread_data *td, char * filename) {
    struct task_registry * ptr;       
    struct task_registry * completed;       
    int counter = 0;
    int send_counter;
    int i;
    char * info;
    char buffer[30];
    struct timeval * result = (struct timeval *) malloc (sizeof(struct timeval));
    struct timeval * gen; 
    struct timeval * now = (struct timeval *) malloc(sizeof(struct timeval));
    FILE *f;
    
    // Get current time, might use for incomplete tasks.
    gettimeofday(now , NULL);

    // Count how many entriers have, in fact, entriers.
    for(i=0; i<td->registry_capacity; i++) {
        if(td->registry[i] != NULL) {
            counter++;
        }
    }

    info = (char *) malloc (sizeof(char)*counter*130);
    info[0] = '\0';

    for(i=0; i<td->registry_capacity; i++) {
        if(td->registry[i] != NULL) {
            send_counter = 0;
            completed = NULL;
            gen = NULL;
            for(ptr = td->registry[i]; ptr!= NULL; ptr=ptr->next) {
                send_counter++;
                // Check if this generated results.
                if(ptr->completed_time != NULL) {
                    debug("Completed found for %d", i);
                    completed = ptr;
                }
                // Update gen value, it is in the first position.
                if(gen == NULL) {
                    gen = ptr->send_time;
                }
            }
            // Task ID
            sprintf(buffer, "%d", i);
            strcat(info, buffer);
            strcat(info, "|");

            // Completed Task Manager ID
            if(completed != NULL) {
                sprintf(buffer, "%d", completed->tm_id);
                strcat(info, buffer);
            }
            else {
                strcat(info, "-1");
            }
            strcat(info, "|");

            // Completed Time
            if(completed != NULL) {
                timeval_subtract(result, completed->completed_time, completed->send_time);
                sprintf(buffer, "%ld.%06ld", result->tv_sec, result->tv_usec);
                strcat(info, buffer);
            }
            else {
                strcat(info, "-1");
            }
            strcat(info, "|");

            // Send Counter 
            sprintf(buffer, "%d", send_counter);
            strcat(info, buffer);
            strcat(info, "|");

            // Completed Time
            if(completed != NULL) {
                timeval_subtract(result, completed->completed_time, gen);
                sprintf(buffer, "%ld.%06ld", result->tv_sec, result->tv_usec);
                strcat(info, buffer);
            }
            else {
                timeval_subtract(result, now, gen);
                sprintf(buffer, "%ld.%06ld", result->tv_sec, result->tv_usec);
                strcat(info, buffer);
            }
            strcat(info, "|");

            // Completed Time
            if(completed != NULL) {
                timeval_subtract(result, completed->completed_time, td->abs_zero);
                sprintf(buffer, "%ld.%06ld", result->tv_sec, result->tv_usec);
                strcat(info, buffer);
            }
            else {
                strcat(info, "-1");
            }
            strcat(info, ";\n");
        }
    }

    if(filename) {
        // Remove file if exist.
        if( access(filename, F_OK) != -1 ) {
            remove(filename);
        }
        // Create file with info string.
        f = fopen(filename, "w");
        fprintf(f, "%s", info);
        fclose(f);
        free(result);
        free(now);
        return info;
    }
    else {
        free(result);
        free(now);
        return info;
    }
}
