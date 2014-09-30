/*
 * Copyright 2014 Ian Liu Rodrigues <ian.liu@ggaunicamp.com>
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

#ifndef __BARRAY_H__
#define __BARRAY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdlib.h>
#include "log.h"

struct byte_array {
	uint8_t *ptr, *iptr;
	size_t len, cap;
};

void byte_array_init(struct byte_array *ba, size_t cap);
void byte_array_resize(struct byte_array *ba, size_t cap);
void byte_array_clear(struct byte_array *ba);
void byte_array_print(struct byte_array *ba);
void byte_array_free(struct byte_array *ba);

void _byte_array_pack64(struct byte_array *ba, uint64_t v);
void _byte_array_pack32(struct byte_array *ba, uint32_t v);
void _byte_array_pack16(struct byte_array *ba, uint16_t v);
void _byte_array_pack8(struct byte_array *ba, uint8_t v);

#define byte_array_pack64(ba, v) _byte_array_pack64(ba, *((uint64_t*)((char*)(&(v)))))
#define byte_array_pack32(ba, v) _byte_array_pack32(ba, *((uint32_t*)((char*)(&(v)))))
#define byte_array_pack16(ba, v) _byte_array_pack16(ba, *((uint16_t*)((char*)(&(v)))))
#define byte_array_pack8(ba, v)  _byte_array_pack8(ba, *((uint8_t*)((char*)(&(v)))))

void _byte_array_pack64v(struct byte_array *ba, uint64_t *v, size_t n);
void _byte_array_pack32v(struct byte_array *ba, uint32_t *v, size_t n);
void _byte_array_pack16v(struct byte_array *ba, uint16_t *v, size_t n);
void _byte_array_pack8v(struct byte_array *ba, uint8_t *v, size_t n);

#define byte_array_pack64v(ba, v, n) _byte_array_pack64v(ba, ((uint64_t*)((char*)(v))), (size_t) n)
#define byte_array_pack32v(ba, v, n) _byte_array_pack32v(ba, ((uint32_t*)((char*)(v))), (size_t) n)
#define byte_array_pack16v(ba, v, n) _byte_array_pack16v(ba, ((uint16_t*)((char*)(v))), (size_t) n)
#define byte_array_pack8v(ba, v, n)  _byte_array_pack8v(ba, ((uint8_t*)((char*)(v))), (size_t) n)

int _byte_array_unpack64(struct byte_array *ba, uint64_t *v);
int _byte_array_unpack32(struct byte_array *ba, uint32_t *v);
int _byte_array_unpack16(struct byte_array *ba, uint16_t *v);
int _byte_array_unpack8(struct byte_array *ba, uint8_t *v);

#define byte_array_unpack64(ba, v) _byte_array_unpack64(ba, ((uint64_t*)((char*)(v))))
#define byte_array_unpack32(ba, v) _byte_array_unpack32(ba, ((uint32_t*)((char*)(v))))
#define byte_array_unpack16(ba, v) _byte_array_unpack16(ba, ((uint16_t*)((char*)(v))))
#define byte_array_unpack8(ba, v)  _byte_array_unpack8(ba,  ((uint8_t*)((char*)(v))))

int _byte_array_unpack64v(struct byte_array *ba, uint64_t *v, size_t n);
int _byte_array_unpack32v(struct byte_array *ba, uint32_t *v, size_t n);
int _byte_array_unpack16v(struct byte_array *ba, uint16_t *v, size_t n);
int _byte_array_unpack8v(struct byte_array *ba, uint8_t *v, size_t n);

#define byte_array_unpack64v(ba, v, n) _byte_array_unpack64v(ba, ((uint64_t*)((char*)(v))), (size_t) n)
#define byte_array_unpack32v(ba, v, n) _byte_array_unpack32v(ba, ((uint32_t*)((char*)(v))), (size_t) n)
#define byte_array_unpack16v(ba, v, n) _byte_array_unpack16v(ba, ((uint16_t*)((char*)(v))), (size_t) n)
#define byte_array_unpack8v(ba, v, n)  _byte_array_unpack8v(ba, ((uint8_t*)((char*)(v))), (size_t) n)

int pack_binary(struct byte_array *ba, char * path);
int unpack_binary(struct byte_array *ba, char *path);
    
#ifdef __cplusplus
}
#endif

#endif /* __BARRAY_H__ */

