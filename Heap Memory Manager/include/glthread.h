#ifndef __GLTHREAD_H_
#define __GLTHREAD_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

/* offset caculate macro */
#define offsetof(struct_name, field_name) (uint64_t)&((struct_name*)0)->field_name

#define BASE(glthreadptr)   ((glthreadptr)->right)

/* Iterative macro (dll) */
#define ITERATE_GLTHREAD_BEGIN(glthreadptrstart, glthreadptr)                                      \
{                                                                                                  \
    glthread_node_t *_glthread_ptr = NULL;                                                         \
    glthreadptr = BASE(glthreadptrstart);                                                          \
    for(; glthreadptr!= NULL; glthreadptr = _glthread_ptr){                                        \
        _glthread_ptr = (glthreadptr)->right;

#define ITERATE_GLTHREAD_END(glthreadptrstart, glthreadptr)  }}

#define ITERATE_LIST_BEGIN(list_ptr, _node)           \
{                                                     \
    _node = list_ptr->head;                           \
    glthread_node_t* _node_next = NULL;               \
    for(;_node != NULL; _node = _node_next){          \
        _node_next = (glthread_node_t*)_node->right;  \

#define ITERATE_LIST_END }}


/* glthread_node_t init macro */
#define glthread_node_init(glnode)  \
        glnode->left = NULL;        \
        glnode->right = NULL;       \

#define GLTHREAD_TO_STRUCT(fn_name, structure_name, field_name, glthread_ptr)                                   \
    static inline structure_name* fn_name(glthread_node_t* glthread_ptr){                                       \
        return (structure_name*)((uint8_t*)(glthread_ptr) - (uint8_t*)&(((structure_name *)0)->field_name));    \
    }

#define IS_GLTHREAD_LIST_EMPTY(glthread_ptr) (glthread_ptr->left == NULL) && (glthread_ptr->right == NULL)
#define GLTHREAD_GET_USER_DATA_FROM_OFFSET(glthreadptr, offset) (void*)((uint8_t*)(glthreadptr) - offset)

typedef struct _glthread_node{

    struct _glthread_node *left;
    struct _glthread_node *right;
} glthread_node_t;

typedef struct _glthread{
    
    glthread_node_t* head;
    uint32_t offset;
} glthread_t;

void glthread_init(glthread_node_t* glthread);
void glthread_add(glthread_node_t* current, glthread_node_t* new);
void glthread_add_pre(glthread_node_t* current, glthread_node_t* new);
void glthread_add_first(glthread_t* list, glthread_node_t* glnode);
void glthread_remove(glthread_node_t* glnode);
void glthread_priority_insert(glthread_node_t* base_glthread, glthread_node_t* new_glthread, int (*comp_fn)(void *, void *), int offset);

#endif /* __GLTHREAD_H_ */