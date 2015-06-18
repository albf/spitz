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

#include "journal.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include "log.h"
#include "registry.h"
#include "taskmanager.h"

#define JOURNAL_INITIAL_CAPACITY 16

// Init journal, allocate all necessary memory to start booking.
void JOURNAL_init(struct journal *dia, int num_threads) {
    int i;
    
    gettimeofday(&dia->zero, NULL);

    dia->size = (int *) malloc(sizeof(int)*num_threads);
    dia->capacity = (int *) malloc(sizeof(int)*num_threads);
    dia->entries = (struct j_entry **) malloc(sizeof(struct j_entry *)*num_threads);
    dia->id_type = (char *) malloc(sizeof(char)*num_threads);

    for(i = 0; i < num_threads; i++) {
        dia->size[i] = 0;
        dia->capacity[i] = JOURNAL_INITIAL_CAPACITY;
        dia->entries[i] = (struct j_entry *) malloc(sizeof(struct j_entry)*JOURNAL_INITIAL_CAPACITY);
    }

    pthread_mutex_init(&dia->id_lock, NULL);
    dia->c_id = 0;
    dia->num_threads = num_threads;
}

// Get id to a thread. Each thread has a different id.
int JOURNAL_get_id(struct journal *dia, char my_type) {
    int ret;

    pthread_mutex_lock(&dia->id_lock);
    ret = dia->c_id;
    dia->c_id++;
    pthread_mutex_unlock(&dia->id_lock);

    dia->id_type[ret] = my_type;
    
    return ret;
}

// Get reference to the next entry. Update the values to the allocated entry.
struct j_entry * JOURNAL_new_entry(struct journal *dia, int id) {
    int index = dia->size[id];

    if(dia->capacity[id] == dia->size[id]) {
        dia->capacity[id] = dia->capacity[id]*2;
        dia->entries[id] = (struct j_entry *) realloc (dia->entries[id], sizeof(struct j_entry)*dia->capacity[id]);
    }

    dia->size[id]++;    
    dia->entries[id][index].size = -1;
    return &(dia->entries[id][index]);
}

// Used to fix id, removing a entry from the journal. Used only with committer at the moment.
void JOURNAL_remove_entry(struct journal *dia, int id) {
    dia->size[id]--;
}

// Free memory allocated by the journal.
void JOURNAL_free(struct journal *dia) {
    int i;

    free(dia->size);
    free(dia->capacity);

    for(i = 0; i < dia->num_threads; i++) {
        free(dia->entries[i]);
    }

    free(dia->entries);
    free(dia->id_type);
    free(dia);
}

// Genereate info and save to filename (if filename != NULL)
// Format: action(1) + | + start(30) + | + finish(30) + ;\n <= 70
char * JOURNAL_generate_info(struct journal *dia, char * filename) {
    int size = 0;
    int i,j;
    char * info;
    char aux[10], buffer[30];
    FILE *f;

    for(i=0; i < dia->num_threads; i++) {
        size += dia->size[i];
    }

    info = (char *) malloc (((size*80)+(dia->num_threads*5))*sizeof(char));
    info[0] = '\0';
    
    for(i=0; i < dia->num_threads; i++) {
            strcat(info, "\n");
            sprintf(aux, "%d", i);
            strcat(info, aux);
            strcat(info, "|");
            aux[1] = '\0';
            aux[0] = dia->id_type[i];
            strcat(info, aux);
            strcat(info, ";\n");
        for(j=0; j < dia->size[i]; j++) {
            aux[1] = '\0';
            aux[0] = dia->entries[i][j].action;
            strcat(info, aux);
            strcat(info, "|");
            timeval_subtract(&dia->entries[i][j].start, &dia->entries[i][j].start, &dia->zero);
            sprintf(buffer, "%ld.%06ld", dia->entries[i][j].start.tv_sec, dia->entries[i][j].start.tv_usec);
            strcat(info, buffer);
            strcat(info, "|");
            timeval_subtract(&dia->entries[i][j].end, &dia->entries[i][j].end, &dia->zero);
            sprintf(buffer, "%ld.%06ld", dia->entries[i][j].end.tv_sec, dia->entries[i][j].end.tv_usec);
            strcat(info, buffer);

            if(dia->entries[i][j].size > 0) {
                strcat(info, "|");
                sprintf(buffer, "%d", dia->entries[i][j].size);
                strcat(info, buffer); 
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
        return info;
    }
    else {
        return info;
    }
}
