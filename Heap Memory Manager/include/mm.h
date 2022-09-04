#ifndef __MM_H_
#define __MM_H_

#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <unistd.h>  // get page size from kernel (getpagesize())
#include <sys/mman.h> // mmap(), munmap()
#include <assert.h>
#include "glthread.h"
#include "css.h"

#define DEBUG_ON        1
#define DEBUG_OFF       0
#define MM_DEBUG        DEBUG_OFF
#define MAX_NAME_LEN    32

/* Free VM Page size that can be used */
#define MAX_FAMILY_PER_PAGE (SYSTEM_PAGE_SIZE - sizeof(vm_page_family_list_t*)) / sizeof(vm_page_family_t)

/* the offset of a particular field_name */ 
#define offset_of(container_structure, field_name) (size_t)&(((container_structure*)0)->field_name)
#define META_SIZE sizeof(meta_blk_t)                                  

/* Iterative macro for searching the VM Page from bottom to top */
#define ITERATE_PAGE_FAMILIES_BEGIN(vm_page_for_families_ptr, curr)          \
        {                                                                    \
        curr = (vm_page_family_t*)&vm_page_for_families_ptr->vm_page[0];     \
        uint32_t max = MAX_FAMILY_PER_PAGE;                                  \
        for(uint32_t i=0; i<max; i++, curr++){                               \
            if(curr->struct_size == 0){                                      \
                break;                                                       \
            }                                                                
        
#define ITERATE_PAGE_FAMILIES_END }}

#define ITERATE_VM_PAGE_BEGIN(vm_page_family_ptr, cur)  \
            {                                           \
            cur = vm_page_family_ptr->first_page;       \
            for(; cur; cur = cur->next_page){           

#define ITERATE_VM_PAGE_END }}

#define ITERATE_VM_PAGE_ALL_BLOCKS_BEGIN(vm_page_ptr, cur)  \
            {                                               \
            cur = &vm_page_ptr->meta_blk;                   \
            for(; cur; cur = NEXT_META_BLOCK(cur)){                       

#define ITERATE_VM_PAGE_ALL_BLOCKS_END }}

#define PQ_ITERATE_BEGIN(glthread_ptr, ptr)                                                             \
        {                                                                                               \
            meta_blk_t* ptr = NULL;                                                                     \
            glthread_node_t* _node = (glthread_ptr)->right;                                             \
            for(; _node; _node = _node->right){                                                         \
                ptr = (meta_blk_t*)((uint8_t*)(_node) - offsetof(meta_blk_t, priority_thread_glue));    

#define PQ_ITERATE_END   }}

#define MM_GET_PAGE_FROM_META_BLOCK(meta_blk_ptr) (void*)((uint8_t*)meta_blk_ptr - meta_blk_ptr->offset)
#define GET_DATA_BLK(meta_blk_ptr) (meta_blk_t*)meta_blk_ptr + 1
#define GET_META_BLK(meta_blk_ptr) (meta_blk_t*)meta_blk_ptr - 1
#define NEXT_META_BLOCK(meta_blk_ptr) (((meta_blk_t*)meta_blk_ptr)->next_blk)
#define PREV_META_BLOCK(meta_blk_ptr) (((meta_blk_t*)meta_blk_ptr)->pre_blk)
#define NEXT_META_BLOCK_BY_SIZE(meta_blk_ptr)   \
            (meta_blk_t*)((uint8_t*)((meta_blk_t*)meta_blk_ptr + 1) + meta_blk_ptr->data_blk_size)

#define MM_BIND_BLKS_FOR_ALLOCATION(allocated_meta_block, free_meta_block)      \
    free_meta_block->pre_blk = allocated_meta_block;                            \
    free_meta_block->next_blk = allocated_meta_block->next_blk;                 \
    allocated_meta_block->next_blk = free_meta_block;                           \
    if (free_meta_block->next_blk)                                              \
        free_meta_block->next_blk->pre_blk = free_meta_block   

#define MM_BIND_BLKS_FOR_DEALLOCATION(freed_meta_block_top, freed_meta_block_down)  \
    freed_meta_block_top->next_blk = freed_meta_block_down->next_blk;               \
    if(freed_meta_block_down->next_blk)                                             \
    freed_meta_block_down->next_blk->pre_blk = freed_meta_block_top


#define MARK_VM_PAGE_EMPTY(vm_page_t_ptr)              \
            vm_page_t_ptr->meta_blk.next_blk = NULL;   \
            vm_page_t_ptr->meta_blk.pre_blk = NULL;    \
            vm_page_t_ptr->meta_blk.is_free = MM_TRUE

#define ZMALLOC(struct_name, units) zalloc(#struct_name, units)
#define ZFREE(addr) zfree(addr);

typedef enum{

    MM_FALSE,
    MM_TRUE
}vm_bool_t;

typedef struct _meta_blk{

    uint32_t data_blk_size;
    uint32_t offset;
    vm_bool_t is_free;
    glthread_node_t priority_thread_glue;
    struct _meta_blk* pre_blk;
    struct _meta_blk* next_blk;
}meta_blk_t;

struct _vm_page_family;

typedef struct _vm_page{

    struct _vm_page* next_page;
    struct _vm_page* pre_page;
    struct _vm_page_family* page_family; // back pointer
    meta_blk_t meta_blk;
    uint8_t page_data_blk[0];
}vm_page_t;

typedef struct _vm_page_family{

    char struct_name[MAX_NAME_LEN];
    uint32_t struct_size;
    vm_page_t* first_page;
    glthread_node_t free_blks_pq; // priority queue
}vm_page_family_t;

typedef struct _vm_page_family_list{

    struct _vm_page_family_list* next;
    vm_page_family_t vm_page[0];
}vm_page_family_list_t;

GLTHREAD_TO_STRUCT(glthread_to_meta_block, meta_blk_t, priority_thread_glue, glthread_ptr);

void mm_init(void);
void mm_debug_fn(void);
void mm_instantiate_new_page_family(char* struct_name, uint32_t struct_size);
void mm_print_registered_page_families();
vm_page_family_t* lookup_page_family_by_name(char *struct_name);
vm_bool_t mm_vm_page_is_empty(vm_page_t* vm_page);
vm_page_t* allocate_vm_page(vm_page_family_t* vm_page_family);
void mm_page_delete_and_free(vm_page_t* vm_page);

#endif /* __MM_H_ */