/*
 * Copyright 2015 Alexandre Luiz Brisighello Filho <albf.unicamp@gmail.com>
 *
 * This file is part of CFIFO.
 * 
 * CFIFO is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * CFIFO is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with CFIFO.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "registry.h"

#include <stdio.h>
#include <stdlib.h>
//#include <assert.h>
#include <string.h>
#include "log.h"
#include <pthread.h>
#include <sys/time.h>

// Adds an registry of task being sent to an Task Manager.
void REGISTRY_add_registry(struct jm_thread_data *td, int task_id, int tm_id) {
    int changed = 0;
    int initial_size, i;
    struct task_registry * ptr;
    struct task_registry * next;

    debug("Registry: adding registry for task : %d to taskmanager : %d", task_id, tm_id);
    pthread_mutex_lock(&td->registry_lock);

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

    ptr = (struct task_registry *) malloc (sizeof(struct task_registry));

    ptr->tm_id = tm_id;
    ptr->task_id = task_id;

    // Save time information 
    ptr->send_time = (struct timeval *) malloc(sizeof(struct timeval));
    gettimeofday(ptr->send_time, NULL);
    ptr->completed_time = NULL;
    ptr->next = NULL;
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

// Add information about task completion.
void REGISTRY_add_completion_registry (struct jm_thread_data *td, size_t task_id, int tm_id) {
    struct task_registry * ptr;
    pthread_mutex_lock(&td->registry_lock);
    ptr = td->registry[task_id];
    debug("Adding Completion Registry: Task %d for TaskManager %d", task_id, tm_id);

    while((ptr != NULL)&&(ptr->tm_id != tm_id)) {
        ptr = ptr->next;
    }

    if(ptr == NULL) {
        error("Error in registry update. Couldn't found Task Manager %d in Registry[%d]", tm_id, (int) task_id);
        pthread_mutex_unlock(&td->registry_lock);
        return;
    }

    ptr->completed_time = (struct timeval *) malloc(sizeof(struct timeval));
    gettimeofday(ptr->completed_time, NULL);
    debug("Completion Registry added.");
    pthread_mutex_unlock(&td->registry_lock);
}

// Indicate if task_id was already sent to tm_id. 1 if yes, 0 if no.
int REGISTRY_check_registry(struct jm_thread_data * td, int task_id, int tm_id) {
    struct task_registry * ptr;
    pthread_mutex_lock(&td->registry_lock);

    if(task_id >= (td->registry_capacity)) {
        error("Registry check of unregistered task (not allocated space) : %d", task_id);
        pthread_mutex_unlock(&td->registry_lock);
        return 0;
    }
    ptr = td->registry[task_id];
    if(ptr == NULL) {
        error("Registry check of unregistered task (NULL pointer) : %d", task_id);
        pthread_mutex_unlock(&td->registry_lock);
        return 0;
    }

    while((ptr!=NULL)&&(ptr->tm_id != tm_id)) {
        ptr=ptr->next;
    }

    if(ptr == NULL) {
        pthread_mutex_unlock(&td->registry_lock);
        return 0;
    }
    else {
        pthread_mutex_unlock(&td->registry_lock);
        return 1;
    }
    error("check_registry bug exit.");
}

// Add information about task completion.
void REGISTRY_free(struct jm_thread_data *td) {
    int i;
    struct task_registry * ptr;
    struct task_registry * free_ptr;

    for(i=0; i < td->registry_capacity; i++) {
        ptr = td->registry[i];
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

    free(td->registry);
    debug("Registry memory freed.");
}
