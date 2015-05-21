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

#include "diary.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include "log.h"
#include "registry.h"
#include "taskmanager.h"

#define DIARY_INITIAL_CAPACITY 16

// Init diary, allocate all necessary memory to start booking.
void DIARY_init(struct tm_thread_data *td, int num_threads) {
    int i;
    
    td->dia = (struct diary *) malloc (sizeof(struct diary));

    gettimeofday(&td->dia->zero, NULL);

    td->dia->size = (int *) malloc(sizeof(int)*num_threads);
    td->dia->capacity = (int *) malloc(sizeof(int)*num_threads);
    td->dia->entries = (struct entry **) malloc(sizeof(struct entry *)*num_threads);

    for(i = 0; i < num_threads; i++) {
        td->dia->size[i] = 0;
        td->dia->capacity[i] = DIARY_INITIAL_CAPACITY;
        td->dia->entries[i] = (struct entry *) malloc(sizeof(struct entry)*DIARY_INITIAL_CAPACITY);
    }

    pthread_mutex_init(&td->dia->id_lock, NULL);
    td->dia->c_id = 0;
    td->dia->num_threads = num_threads;
}

// Get id to a thread. Each thread has a different id.
int DIARY_get_id(struct tm_thread_data *td) {
    int ret;

    pthread_mutex_lock(&td->dia->id_lock);
    ret = td->dia->c_id;
    td->dia->c_id++;
    pthread_mutex_lock(&td->dia->id_lock);
    
    return ret;
}

// Get reference to the next entry. Update the values to the allocated entry.
struct entry * DIARY_new_entry(struct tm_thread_data *td, int id) {
    int index = td->dia->size[id];

    if(td->dia->capacity[id] == td->dia->size[id]) {
        td->dia->capacity[id] = td->dia->capacity[id]*2;
        td->dia->entries[id] = (struct entry *) realloc (td->dia->entries[id], sizeof(struct entry)*td->dia->capacity[id]);
    }

    td->dia->size[id]++;    
    return td->dia->entries[index];
}

// Free memory allocated by the diary.
void DIARY_free(struct tm_thread_data *td) {
    int i;

    free(td->dia->size);
    free(td->dia->capacity);

    for(i = 0; i < td->dia->num_threads; i++) {
        free(td->dia->entries[i]);
    }

    free(td->dia->entries);
    free(td->dia);
}

// Genereate info and save to filename (if filename != NULL)
// Format: action(1) + | + start(30) + | + finish(30) + ;\n <= 70
char * DIARY_generate_info(struct tm_thread_data *td, char * filename) {
    int size = 0;
    int i,j;
    char * info;
    char aux[2], buffer[30];
    FILE *f;

    for(i=0; i < td->dia->num_threads; i++) {
        size += td->dia->size[i];
    }

    info = (char *) malloc (size*70*sizeof(char));
    
    for(i=0; i < td->dia->num_threads; i++) {
        for(j=0; j < td->dia->size[i]; j++) {
            aux[1] = '\0';
            aux[0] = td->dia->entries[i][j].action;
            strcat(info, aux);
            strcat(info, "|");
            timeval_subtract(&td->dia->entries[i][j].start, &td->dia->entries[i][j].start, &td->dia->zero);
            sprintf(buffer, "%ld.%06ld", td->dia->entries[i][j].start.tv_sec, td->dia->entries[i][j].start.tv_usec);
            strcat(info, "|");
            timeval_subtract(&td->dia->entries[i][j].end, &td->dia->entries[i][j].end, &td->dia->zero);
            sprintf(buffer, "%ld.%06ld", td->dia->entries[i][j].end.tv_sec, td->dia->entries[i][j].end.tv_usec);
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
        return info;
    }
    else {
        return info;
    }
}
