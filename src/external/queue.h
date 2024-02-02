/*
 *  Zlib license:
 *
 *  Copyright (c) 2024 Lieven Petersen
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you must not
 *  claim that you wrote the original software. If you use this software
 *  in a product, an acknowledgment in the product documentation would be
 *  appreciated but is not required.
 *  2. Altered source versions must be plainly marked as such, and must not be
 *  misrepresented as being the original software.
 *  3. This notice may not be removed or altered from any source distribution.
 */


#ifndef __QUEUE_H
#define __QUEUE_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct __queue_info{
    void *queue_ptr;
    size_t head;
    size_t tail;
    size_t size;
    size_t capacity;
};

static struct __queue_info *__g_qi = NULL;
static size_t __g_qc = 0;

static struct __queue_info *__get_qi(void *queue){
    for (size_t i = 0; i < __g_qc; i++){
        if (__g_qi[i].queue_ptr == queue) return &__g_qi[i];
    }
    // if the queue ptr is unknown, it's probably from uninitialized memory.
    if (queue != NULL){
        perror("[QUEUE] WARNING: given queue was not initialized!\n");
    }
    return NULL;
}

#define __init_queue(queue) do{                                                                                 \
    __g_qi = realloc(__g_qi, (++__g_qc) * sizeof(*__g_qi));                                                     \
    struct __queue_info *entry = &__g_qi[__g_qc-1];                                                             \
    entry->queue_ptr = queue;                                                                                   \
    entry->capacity = 0;                                                                                        \
    entry->size = 0;                                                                                            \
    entry->head = 0;     /*points at the first invalid element ahead, ready for insertion*/                     \
    entry->tail = 0;     /*points at the last valid element OR the head if size == 0*/                          \
}while(0)

#define queue_free(queue) do{                                                                                   \
    struct __queue_info *__qi = __get_qi(queue);                                                                \
    if (__qi != NULL){                                                                                          \
        if (__qi < __g_qi + __g_qc - 1) memmove(__qi, __qi + 1, (__g_qc - (__qi - __g_qi) - 1)*sizeof(*__qi));  \
        __g_qi = realloc(__g_qi, (--__g_qc) * sizeof(*__g_qi));                                                 \
        free(queue);                                                                                            \
        queue = NULL;                                                                                           \
    }                                                                                                           \
}while(0)

#define queue_reserve_capacity(queue, new_capacity) do{                                                         \
    if (queue == NULL) __init_queue(queue);                                                                     \
    struct __queue_info *__qi = __get_qi(queue);                                                                \
    __queue_reserve_capacity(__qi, (void**)&queue, sizeof(*queue), new_capacity);                               \
}while(0)

static void __queue_reserve_capacity(struct __queue_info *qi, void **queue, size_t sizeof_elem, size_t new_capacity){
    size_t applied_capacity = qi->size < new_capacity? new_capacity : qi->size;
    if (qi->tail < qi->head || qi->size == 0) {
        if (qi->size > 0 && qi->head == 0) qi->head = qi->tail + qi->size; // tail is at 0 and head has looped around to it.
        *queue = realloc(*queue, applied_capacity * sizeof_elem);
    } else {
        void *new_mem = malloc(applied_capacity * sizeof_elem);
        memcpy(new_mem, ((char*)*queue) + qi->tail*sizeof_elem, (qi->capacity - qi->tail)*sizeof_elem);
        memcpy((char*)new_mem + (qi->capacity - qi->tail)*sizeof_elem, *queue, qi->head*sizeof_elem);
        qi->tail = 0;
        qi->head = qi->size;

        free(*queue);
        *queue = new_mem;
    }
    qi->capacity = applied_capacity;
    qi->queue_ptr = *queue;
}

#define queue_push(queue, ...)  do{                                                                             \
    if (queue == NULL) __init_queue(queue);                                                                     \
    struct __queue_info *__qi = __get_qi(queue);                                                                \
    if (__qi->size == __qi->capacity) {                                                                         \
        __queue_reserve_capacity(__qi, (void**)&queue, sizeof(*queue), (__qi->capacity + 1) * 2);               \
    }                                                                                                           \
    queue[__qi->head] = __VA_ARGS__;                                                                            \
    __qi->head = (__qi->head + 1) % __qi->capacity;                                                             \
    __qi->size++;                                                                                               \
}while(0)

static size_t __queue_pop_tail_idx(void *queue){
    struct __queue_info *qi = __get_qi(queue);
    if (qi->size > 0){
        qi->size--;
        size_t ret_idx = qi->tail;
        qi->tail = (qi->tail + 1) % qi->capacity;
        return ret_idx;
    } else {
        return 0;
    }
}

#define queue_poll(queue)  queue[__queue_pop_tail_idx(queue)]

#define queue_size(queue) (__get_qi(queue) == NULL? 0 : __get_qi(queue)->size)

#endif //__QUEUE_H
