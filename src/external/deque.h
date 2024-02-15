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

/*
push, pop, front
append, poll, back

lpush, lpop, lpeek
rpush, rpop, rpeek
*/

#ifndef __DEQUE_H
#define __DEQUE_H

#include <stdlib.h>
#include <string.h>

typedef unsigned long __deq_size_t;

#define DEQ(type) struct {                                                      \
    type *items; __deq_size_t capacity; __deq_size_t size; __deq_size_t tail;   \
}

#define deq_free(deq) (deq).size = 0

#define deq_size(deq) (deq).size

static int __deq_pop_idx(__deq_size_t *size){
    if (*size > 0 ){
        (*size)--;
        return *size;
    }
    return 0;
}

static int __deq_poll_idx(__deq_size_t *size, __deq_size_t *tail, __deq_size_t capacity){
    if (*size > 0){
        __deq_size_t idx = *tail;
        (*size)--;
        (*tail) = ((*tail) + 1) % capacity;
        return idx;
    }
    return 0;
}

#define deq_front(deq) (deq).items[((deq).tail + (deq).size-1) % (deq).capacity]
#define deq_back(deq) (deq).items[(deq).tail]


#define __deq_resize(deq, is_wrapped){                                                                              \
    __deq_size_t old_cap = (deq).capacity;                                                                          \
    (deq).capacity = ((deq).capacity+1) * 2;                                                                        \
    (deq).items = realloc((deq).items, (deq).capacity * sizeof(*(deq).items));                                      \
    if (is_wrapped) {                                                                                               \
        __deq_size_t new_tail = (deq).tail + (deq).capacity - old_cap;                                              \
        memmove((deq).items + new_tail , (deq).items + (deq).tail, (old_cap - (deq).tail) * sizeof(*(deq).items));  \
        (deq).tail = new_tail;                                                                                      \
    }                                                                                                               \
}

#define deq_push(deq, elem) {                                                                           \
    if ((deq).size == (deq).capacity) {                                                                 \
        char is_wrapped = (deq).tail > 0;                                                               \
        __deq_resize((deq), is_wrapped);                                                                \
    }                                                                                                   \
    (deq).items[((deq).tail + (deq).size++) % (deq).capacity] = elem;}

#define deq_append(deq, elem){                                                                          \
    if ((deq).size == (deq).capacity) {                                                                 \
        char is_wrapped = (deq).tail > 0;                                                               \
        __deq_resize((deq), is_wrapped);                                                                \
    }                                                                                                   \
    if ((deq).size > 0) (deq).tail = ((deq).tail - 1) % (deq).capacity;                                 \
    (deq).size++;                                                                                       \
    (deq).items[((deq).tail)] = elem;                                                                   \
}

#define deq_pop(deq) ((deq).items[((deq).tail + __deq_pop_idx(&(deq).size)) % (deq).capacity])

#define deq_poll(deq) ((deq).items[__deq_poll_idx(&(deq).size, &(deq).tail, (deq).capacity)])


#endif //__DEQUE_H
