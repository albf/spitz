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

#ifndef __SPITZ_COMMITTER_H__
#define __SPITZ_COMMITTER_H__

#include <semaphore.h>
#include "barray.h"
#include "journal.h"

struct cm_result_node {
    struct byte_array *ba;
    struct cm_result_node *next;
};

struct cm_thread_data {
    void * user_data;
    void * handle;
    struct cm_result_node * results;
    struct cm_result_node * head;
    sem_t r_counter;
    pthread_mutex_t r_lock;
    pthread_mutex_t f_lock;
    struct journal * dia;
};

void * commit_worker(void *ptr);
void committer(int argc, char *argv[], struct cm_thread_data * d);

#endif /* __SPITZ_COMMITTER_H__ */
