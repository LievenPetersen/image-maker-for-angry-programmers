
/*
 *  zlib license:
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


#ifndef __STACK_H
#define __STACK_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct __stack_info{
    void *stack_ptr;
    size_t size;
    size_t capacity;
};

static struct __stack_info *__g_si = NULL;
static size_t __g_sc = 0;

static struct __stack_info *__get_si(void *stack){
    for (size_t i = 0; i < __g_sc; i++){
        if (__g_si[i].stack_ptr == stack) return &__g_si[i];
    }
    // if the stack ptr is unknown, it's probably from uninitialized memory.
    if (stack != NULL){
        perror("[STACK] WARNING: given stack was not initialized!\n");
    }
    return NULL;
}

#define __init_stack(stack) do{                                                                                 \
    __g_si = realloc(__g_si, (++__g_sc) * sizeof(*__g_si));                                                     \
    __g_si[__g_sc-1].stack_ptr = stack;                                                                         \
    __g_si[__g_sc-1].size = 0;                                                                                  \
    __g_si[__g_sc-1].capacity = 0;                                                                              \
}while(0)

#define stack_free(stack) do{                                                                                   \
    struct __stack_info *__si = __get_si(stack);                                                                \
    if (__si != NULL){                                                                                          \
        if (__si < __g_si + __g_sc - 1) memmove(__si, __si + 1, (__g_sc - (__si - __g_si) - 1)*sizeof(*__si));  \
        __g_si = realloc(__g_si, (--__g_sc) * sizeof(*__g_si));                                                 \
        free(stack);                                                                                            \
        stack = NULL;                                                                                           \
    }                                                                                                           \
}while(0)

#define stack_reserve_capacity(stack, new_capacity) do{                                                         \
    if (stack == NULL) __init_stack(stack);                                                                     \
    struct __stack_info *__si = __get_si(stack);                                                                \
    __stack_reserve_capacity(__si, stack, new_capacity);                                                        \
}while(0)

#define __stack_reserve_capacity(__si, stack, new_capacity) do{                                                 \
    __si->capacity = __si->size < new_capacity? new_capacity : __si->size;                                      \
    stack = realloc(stack, __si->capacity * sizeof(*stack));                                                    \
    __si->stack_ptr = stack;                                                                                    \
}while(0)

#define stack_push(stack, ...)  do{                                                                             \
    if (stack == NULL) __init_stack(stack);                                                                     \
    struct __stack_info *__si = __get_si(stack);                                                                \
    if (__si->size == __si->capacity) {                                                                         \
        __stack_reserve_capacity(__si, stack, (__si->capacity + 1) * 2);                                        \
    }                                                                                                           \
    stack[__si->size++] = __VA_ARGS__;                                                                          \
}while(0)

#define stack_pop(stack)  stack[__get_si(stack)->size > 0? --__get_si(stack)->size : 0]

#define stack_size(stack) (__get_si(stack) == NULL? (size_t)0 : __get_si(stack)->size)

#endif //__STACK_H
