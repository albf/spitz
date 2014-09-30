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

#include "barray.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h> 

#define RETURN_FALSE_IF_NOT_ENOUGHT_SIZE(ba, sz) do { \
	if ((ba)->ptr + (ba)->len < (ba)->iptr + (sz))\
		return 0; \
	} while(0)

static uint64_t htonll(uint64_t x)
{
	if (htons(1) == 1)
		return x;
	uint32_t lo = (x & 0xFFFFFFFF00000000) >> 32;
	uint32_t hi = x & 0x00000000FFFFFFFF;
	uint64_t nlo = htonl(lo);
	uint64_t nhi = htonl(hi);
	return nhi << 32 | nlo;
}

static uint64_t ntohll(uint64_t x)
{
	if (ntohs(1) == 1)
		return x;
	uint32_t lo = (x & 0xFFFFFFFF00000000) >> 32;
	uint32_t hi = x & 0x00000000FFFFFFFF;
	uint64_t nlo = htonl(lo);
	uint64_t nhi = htonl(hi);
	return nhi << 32 | nlo;
}

static void ensure_size(struct byte_array *ba, size_t sz)
{
	if (ba->len + sz >= ba->cap)
		byte_array_resize(ba, ba->len + sz);
}

/**
 * Initializes the byte array with capacity #cap. Free with byte_array_free().
 */
void byte_array_init(struct byte_array *ba, size_t cap)
{
	ba->len = 0;
	ba->cap = cap;
	ba->ptr = malloc(cap);
	ba->iptr = ba->ptr;
}

/**
 * Resizes the byte array capacity to #cap. If #cap is less than the actual
 * byte array length, its content is truncated.
 */
void byte_array_resize(struct byte_array *ba, size_t cap)
{
	ba->cap = cap;
	ba->ptr = realloc(ba->ptr, cap);
	ba->iptr = ba->ptr;
	if (ba->len > cap) {
		ba->len = cap;
    }
}

void byte_array_clear(struct byte_array *ba)
{
	ba->len = 0;
	ba->iptr = ba->ptr;
}

void byte_array_print(struct byte_array *ba)
{
	size_t i;
	for (i=0; i < ba->len; i++) {
		printf("%02X ", ba->ptr[i]);
    }
	printf("\n");
}

/**
 * Frees the internal byte array memory and set its length and capacity to
 * zero.
 */
void byte_array_free(struct byte_array *ba)
{
	free(ba->ptr);
	ba->ptr = NULL;
	ba->len = ba->cap = 0;
}

/**
 * Pack a unsigned 64 bits integer into this byte array. If the byte array
 * capacity isn't enough, the buffer is increased with the exact size needed.
 * Consider resizing the byte array if performance becomes an issue.
 *
 * @see byte_array_resize()
 */
void _byte_array_pack64(struct byte_array *ba, uint64_t v)
{
	ensure_size(ba, sizeof(v));
	union {
		uint64_t i;
		char c[8];
	} nv = { htonll(v) };
	memcpy(ba->ptr + ba->len, nv.c, sizeof(v));
	ba->len += sizeof(v);
}

/**
 * Pack a unsigned 32 bits integer into this byte array. See
 * _byte_array_pack64() for performance and resizing issues.
 */
void _byte_array_pack32(struct byte_array *ba, uint32_t v)
{
	ensure_size(ba, sizeof(v));
	union {
		uint32_t i;
		char c[4];
	} nv = { htonl(v) };
	memcpy(ba->ptr + ba->len, nv.c, sizeof(v));
	ba->len += sizeof(v);
}

/**
 * Pack a unsigned 16 bits integer into this byte array. See
 * _byte_array_pack64() for performance and resizing issues.
 */
void _byte_array_pack16(struct byte_array *ba, uint16_t v)
{
	ensure_size(ba, sizeof(v));
	union {
		uint16_t i;
		char c[2];
	} nv = { htons(v) };
	memcpy(ba->ptr + ba->len, nv.c, sizeof(v));
	ba->len += sizeof(v);
}

/**
 * Pack a unsigned 8 bits integer into this byte array. See
 * _byte_array_pack64() for performance and resizing issues.
 */
void _byte_array_pack8(struct byte_array *ba, uint8_t v)
{
	ensure_size(ba, 1);
	*(ba->ptr + ba->len) = v;
	ba->len++;
}

/**
 * Pack a vector of unsigned 64 bits integer into this byte array. See
 * _byte_array_pack64() for performance and resizing issues.
 */
void _byte_array_pack64v(struct byte_array *ba, uint64_t *v, size_t n)
{
	ensure_size(ba, n * sizeof(v[0]));
	size_t i;
	for (i = 0; i < n; i++) {
		_byte_array_pack64(ba, v[i]);
    }
}

/**
 * Pack a vector of unsigned 64 bits integer into this byte array. See
 * _byte_array_pack64() for performance and resizing issues.
 */
void _byte_array_pack32v(struct byte_array *ba, uint32_t *v, size_t n)
{
	ensure_size(ba, n * sizeof(v[0]));
	size_t i;
	for (i = 0; i < n; i++)
		_byte_array_pack32(ba, v[i]);
}

/**
 * Pack a vector of unsigned 64 bits integer into this byte array. See
 * _byte_array_pack64() for performance and resizing issues.
 */
void _byte_array_pack16v(struct byte_array *ba, uint16_t *v, size_t n)
{
	ensure_size(ba, n * sizeof(v[0]));
	size_t i;
	for (i = 0; i < n; i++)
		_byte_array_pack16(ba, v[i]);
}

/**
 * Pack a vector of unsigned 64 bits integer into this byte array. See
 * _byte_array_pack64() for performance and resizing issues.
 */
void _byte_array_pack8v(struct byte_array *ba, uint8_t *v, size_t n)
{
	ensure_size(ba, n);
	memcpy(ba->ptr + ba->len, v, n);
	ba->len += n;
}

/**
 * Unpack a unsigned 64 bits integer into #v. If there are insuficient bytes in
 * the byte array buffer to unpack, 0 is returned and #v is left untouched,
 * otherwise 1 is returned.
 */
int _byte_array_unpack64(struct byte_array *ba, uint64_t *v)
{
	RETURN_FALSE_IF_NOT_ENOUGHT_SIZE(ba, sizeof(v[0]));
	union {
		uint64_t i;
		char c[8];
	} nv;
	memcpy(nv.c, ba->iptr, sizeof(v[0]));
	*v = ntohll(nv.i);
	ba->iptr += sizeof(v[0]);
	return 1;
}

/**
 * Unpack a unsigned 32 bits integer into #v. If there are insuficient bytes in
 * the byte array buffer to unpack, 0 is returned and #v is left untouched,
 * otherwise 1 is returned.
 */
int _byte_array_unpack32(struct byte_array *ba, uint32_t *v)
{
	RETURN_FALSE_IF_NOT_ENOUGHT_SIZE(ba, sizeof(v[0]));
	union {
		uint32_t i;
		char c[4];
	} nv;
	memcpy(nv.c, ba->iptr, sizeof(v[0]));
	*v = ntohl(nv.i);
	ba->iptr += sizeof(v[0]);
	return 1;
}

/**
 * Unpack a unsigned 16 bits integer into #v. If there are insuficient bytes in
 * the byte array buffer to unpack, 0 is returned and #v is left untouched,
 * otherwise 1 is returned.
 */
int _byte_array_unpack16(struct byte_array *ba, uint16_t *v)
{
	RETURN_FALSE_IF_NOT_ENOUGHT_SIZE(ba, sizeof(v[0]));
	union {
		uint16_t i;
		char c[2];
	} nv;
	memcpy(nv.c, ba->iptr, sizeof(v[0]));
	*v = ntohs(nv.i);
	ba->iptr += sizeof(v[0]);
	return 1;
}

/**
 * Unpack a unsigned 8 bits integer into #v. If there are insuficient bytes in
 * the byte array buffer to unpack, 0 is returned and #v is left untouched,
 * otherwise 1 is returned.
 */
int _byte_array_unpack8(struct byte_array *ba, uint8_t *v)
{
	RETURN_FALSE_IF_NOT_ENOUGHT_SIZE(ba, 1);
	*v = *ba->iptr;
	ba->iptr++;
	return 1;
}

/**
 * Unpack a vector of #n unsigned 64 bits integers into #v, which must be large
 * enough to receive its values. If there are insuficient bytes in the byte
 * array buffer to unpack, 0 is returned and #v is left untouched, otherwise 1
 * is returned.
 */
int _byte_array_unpack64v(struct byte_array *ba, uint64_t *v, size_t n)
{
	RETURN_FALSE_IF_NOT_ENOUGHT_SIZE(ba, sizeof(v[0]) * n);
	size_t i;
	for (i = 0; i < n; i++)
		_byte_array_unpack64(ba, v + i);
	return 1;
}

/**
 * Unpack a vector of #n unsigned 32 bits integers into #v, which must be large
 * enough to receive its values. If there are insuficient bytes in the byte
 * array buffer to unpack, 0 is returned and #v is left untouched, otherwise 1
 * is returned.
 */
int _byte_array_unpack32v(struct byte_array *ba, uint32_t *v, size_t n)
{
	RETURN_FALSE_IF_NOT_ENOUGHT_SIZE(ba, sizeof(v[0]) * n);
	size_t i;
	for (i = 0; i < n; i++)
		_byte_array_unpack32(ba, v + i);
	return 1;
}

/**
 * Unpack a vector of #n unsigned 16 bits integers into #v, which must be large
 * enough to receive its values. If there are insuficient bytes in the byte
 * array buffer to unpack, 0 is returned and #v is left untouched, otherwise 1
 * is returned.
 */
int _byte_array_unpack16v(struct byte_array *ba, uint16_t *v, size_t n)
{
	RETURN_FALSE_IF_NOT_ENOUGHT_SIZE(ba, sizeof(v[0]) * n);
	size_t i;
	for (i = 0; i < n; i++)
		_byte_array_unpack16(ba, v + i);
	return 1;
}

/**
 * Unpack a vector of #n unsigned 8 bits integers into #v, which must be large
 * enough to receive its values. If there are insuficient bytes in the byte
 * array buffer to unpack, 0 is returned and #v is left untouched, otherwise 1
 * is returned.
 */
int _byte_array_unpack8v(struct byte_array *ba, uint8_t *v, size_t n)
{
	RETURN_FALSE_IF_NOT_ENOUGHT_SIZE(ba, n);
	memcpy(v, ba->iptr, n);
	ba->iptr += n;
	return 1;
}

int pack_binary(struct byte_array *ba, char * path) {
    struct stat f_status;
    FILE *fp;

    if(ba == NULL) {
        error("Null pointer passed to _pack binary.");
        return -1;
    }

    if(stat(path, &f_status) != 0) {
        error("Problem getting info from binary. Does it really exists?");
        return -2;
    }

    fp = fopen(path, "rb");
    byte_array_resize(ba, f_status.st_size);
    if(!fread(ba->ptr, f_status.st_size, 1, fp)) {
        error("Error loading file.");
        return -3;
    }

    fclose(fp);
}

int unpack_binary(struct byte_array *ba, char *path) {
    FILE *fp;

    if(ba == NULL) {
        error("Null pointer passed to _pack binary.");
        return -1;
    }
    
    fp = fopen(path, "wb");
    if(!fp) {
       error("Couldn't open file to open.");
       return -2;
    }

    if( fwrite(ba->ptr, 1, ba->cap, fp) != ba->cap ) {
        error("Error ocurred in fwrite of unpack binary.");
        return -3;
    }

    fclose(fp); 
}