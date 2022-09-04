#include "mm.h"

static size_t SYSTEM_PAGE_SIZE = 0;
static vm_page_family_list_t *first_vm_page_for_family = NULL;


/**
 * get VM Page Size
 */ 
void mm_init(){

    SYSTEM_PAGE_SIZE = getpagesize();
}


/**
 * request VM Page from kernel
 */ 
static void* mm_get_vm_page(uint32_t units){

    if(SYSTEM_PAGE_SIZE == 0){

        #if MM_DEBUG
            printf("System Page is 0!\n");
        #endif

        return NULL;
    }

    size_t length = units * SYSTEM_PAGE_SIZE;
    int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    int flag = MAP_ANON | MAP_PRIVATE;

    uint8_t* vm_page = mmap(0, length, prot, flag, 0, 0);

    if(vm_page == MAP_FAILED){

        #if MM_DEBUG
            printf("Fail to mmap VM page from kernel!\n");
        #endif

        return NULL;
    }

    memset(vm_page, 0x0, length);

    #if MM_DEBUG
        printf("Successfully mmap VM page from kernel!\n\n");
    #endif

    return (void*)vm_page;
}


/**
 * release VM Page
 */ 
static void mm_release_vm_page(void* vm_page, uint32_t units){

    if(SYSTEM_PAGE_SIZE == 0){

        #if MM_DEBUG
            printf("System Page is 0!\n");
        #endif

        return;
    }

    size_t length = units * SYSTEM_PAGE_SIZE;

    if(munmap(vm_page, length) == -1){

        #if MM_DEBUG
            printf("Fail to munmap VM page to kernel!\n");
        #endif
    }

    #if MM_DEBUG
        printf("Successfully munmap VM page to kernel!\n");
    #endif
}


/**
 * union two free blocks
 */ 
static void mm_union_free_blocks(meta_blk_t* first, meta_blk_t* second){

    assert(first->is_free == MM_TRUE && second->is_free == MM_TRUE);

    glthread_remove(&first->priority_thread_glue);
    glthread_remove(&second->priority_thread_glue);

    first->data_blk_size += META_SIZE + second->data_blk_size;
    MM_BIND_BLKS_FOR_DEALLOCATION(first, second);
}


/**
 * return the max size of VM Page
 */ 
static inline uint32_t mm_max_page_allocatable_memory(int units){

    if(units == 0 || SYSTEM_PAGE_SIZE == 0){

        #if MM_DEBUG
            printf("System Page is 0!\n");
        #endif

        return 0;
    }

    return (uint32_t)((SYSTEM_PAGE_SIZE * units) - offset_of(vm_page_t, page_data_blk));
}


/**
 * 
 * Return -1: meta_blk_data1 size is greater than meta_blk_data2
 * Return 1: meta_blk_data2 size is greater than meta_blk_data1
 * Return 0: meta_blk_data1 size is equal to meta_blk_data2
 * 
 */ 
static int free_blocks_comparison_function(void* meta_blk_data1, void* meta_blk_data2){

    assert(meta_blk_data1 && meta_blk_data2);

    if(((meta_blk_t*)meta_blk_data1)->data_blk_size > ((meta_blk_t*)meta_blk_data2)->data_blk_size){

        return -1;
    }else if(((meta_blk_t*)meta_blk_data1)->data_blk_size < ((meta_blk_t*)meta_blk_data2)->data_blk_size){

        return 1;
    }

    return 0;
}


/**
 * Add a given free meta block to a Priority Queue of a given Page family 
 */ 
static void mm_add_free_meta_block_to_free_block_list(vm_page_family_t* vm_page_family, meta_blk_t* free_blk){

    if(free_blk == NULL){

        return;
    }

    assert(free_blk->is_free == MM_TRUE);

    glthread_priority_insert(&vm_page_family->free_blks_pq,
            &free_blk->priority_thread_glue,
            free_blocks_comparison_function,
            offset_of(meta_blk_t, priority_thread_glue));
}


/**
 * 
 */ 
static vm_page_t* mm_family_add_new_page(vm_page_family_t *vm_page_family){

    vm_page_t* new_vm_page = allocate_vm_page(vm_page_family);

    if(new_vm_page == NULL){

        return NULL;
    }   

    /* add new meta block to the pq */
    mm_add_free_meta_block_to_free_block_list(vm_page_family, &new_vm_page->meta_blk);

    return new_vm_page;
}


/**
 * return the best fit node of the Priority Queue
 */ 
static inline meta_blk_t* mm_get_biggest_free_block_page_family(vm_page_family_t* vm_page_family){

    if(vm_page_family == NULL){

        #if MM_DEBUG
            printf("vm_page_family is NULL!\n");
        #endif

        return NULL;
    }

    glthread_node_t* biggest_free_blk = vm_page_family->free_blks_pq.right;

    if(biggest_free_blk){

        return glthread_to_meta_block(biggest_free_blk);
    }

    return NULL;
}


/* 
 * mark meta block as being Allocated for 'size' bytes of application data
 * return MM_TRUE if allocation successed
 * return MM_FALSE if the allocation failed 
 */ 
static vm_bool_t mm_split_free_data_block_for_allocation(vm_page_family_t* page_family, meta_blk_t* meta_blk, uint32_t size){

    assert(page_family);
    assert(meta_blk->is_free == MM_TRUE);

    if(meta_blk->data_blk_size < size){

        return MM_FALSE;
    }

    uint32_t remaining_size = meta_blk->data_blk_size - size;
    meta_blk_t* remaining_blk = NULL;
    meta_blk->is_free = MM_FALSE;
    meta_blk->data_blk_size = size;
    glthread_remove(&meta_blk->priority_thread_glue);

    if(remaining_size == 0){ // no split

        #if MM_DEBUG
            printf("Split: no split\n");
        #endif

        return MM_TRUE;
    }else if(remaining_size > META_SIZE &&
             remaining_size < META_SIZE + page_family->struct_size){ // partial split: Soft Internal Fragmentation

        #if MM_DEBUG
            printf("Split: partial split(Soft IF)\n");
        #endif

        remaining_blk = NEXT_META_BLOCK_BY_SIZE(meta_blk);
        remaining_blk->is_free = MM_TRUE;
        remaining_blk->data_blk_size = remaining_size - META_SIZE;
        remaining_blk->offset = meta_blk->offset + META_SIZE + meta_blk->data_blk_size;
        glthread_init(&remaining_blk->priority_thread_glue);
        mm_add_free_meta_block_to_free_block_list(page_family, remaining_blk);
        MM_BIND_BLKS_FOR_ALLOCATION(meta_blk, remaining_blk);
    }else if(remaining_size < META_SIZE){ // partial split: Hard Internal Fragmentation

        #if MM_DEBUG
            printf("Split: partial split(Hard IF)\n");
        #endif
    }else{ // full split

        #if MM_DEBUG
            printf("Split: full split\n");
        #endif

        remaining_blk = NEXT_META_BLOCK_BY_SIZE(meta_blk);
        remaining_blk->is_free = MM_TRUE;
        remaining_blk->data_blk_size = remaining_size - META_SIZE;
        remaining_blk->offset = meta_blk->offset + META_SIZE + meta_blk->data_blk_size;
        glthread_init(&remaining_blk->priority_thread_glue);
        mm_add_free_meta_block_to_free_block_list(page_family, remaining_blk);
        MM_BIND_BLKS_FOR_ALLOCATION(meta_blk, remaining_blk);
    }

    return MM_TRUE;
}


/**
 * return meta block of free data block 
 */ 
static meta_blk_t* mm_allocate_free_data_block(vm_page_family_t* page_family, uint32_t size){

    vm_bool_t status = MM_FALSE;
    vm_page_t* vm_page = NULL;
    meta_blk_t* bf_meta_blk = mm_get_biggest_free_block_page_family(page_family);

    if(!bf_meta_blk || bf_meta_blk->data_blk_size < size){

        #if MM_DEBUG
            printf("request new VM Page!\n");
        #endif

        vm_page = mm_family_add_new_page(page_family); // bug
        status = mm_split_free_data_block_for_allocation(page_family, &vm_page->meta_blk, size); // bug

        if(status){

            return &vm_page->meta_blk;
        }

        return NULL;
    }

    if(bf_meta_blk){

        status = mm_split_free_data_block_for_allocation(page_family, bf_meta_blk, size);
    }

    if(status){

        return bf_meta_blk;
    }

    return NULL;
}


/**
 * hand hard internal fragmentation when carry out merging
 */ 
static uint32_t mm_get_hard_internal_memory_frag_size(meta_blk_t* first, meta_blk_t* second){

    meta_blk_t* idea_meta_blk = NEXT_META_BLOCK_BY_SIZE(first);

    return (uint32_t)((uint64_t)second - (uint64_t)idea_meta_blk);
}


/**
 * 
 */ 
static meta_blk_t* mm_free_blocks(meta_blk_t* free_meta_blk){

    free_meta_blk->is_free = MM_TRUE;
    meta_blk_t* next_meta_blk = NEXT_META_BLOCK(free_meta_blk);
    meta_blk_t* pre_meta_blk = PREV_META_BLOCK(free_meta_blk);
    meta_blk_t* ret = NULL;
    vm_page_t* vm_page = MM_GET_PAGE_FROM_META_BLOCK(free_meta_blk);
    vm_page_family_t* vm_page_family = vm_page->page_family;

    if(next_meta_blk){

        free_meta_blk->data_blk_size += mm_get_hard_internal_memory_frag_size(free_meta_blk, next_meta_blk);
    }else{

        uint8_t* top_of_vm_page = (uint8_t*)vm_page + SYSTEM_PAGE_SIZE;
        uint8_t* tail_of_data_blk = (uint8_t*)(free_meta_blk + 1) + free_meta_blk->data_blk_size;
        uint64_t hard_IF = (uint64_t)(top_of_vm_page - tail_of_data_blk);
        free_meta_blk->data_blk_size += hard_IF;
    }

    if(next_meta_blk && next_meta_blk->is_free == MM_TRUE){
 
        mm_union_free_blocks(free_meta_blk, next_meta_blk);
        ret = free_meta_blk;
    }

    if(pre_meta_blk && pre_meta_blk->is_free == MM_TRUE){

        mm_union_free_blocks(pre_meta_blk, free_meta_blk);
        ret = pre_meta_blk;
    }

    if(mm_vm_page_is_empty(vm_page)){

        mm_page_delete_and_free(vm_page);
        return NULL;
    }

    mm_add_free_meta_block_to_free_block_list(vm_page_family, ret);

    return ret;
}


/**
 * print all meta blocks in the vm page
 */ 
static uint32_t mm_print_meta_block_usage(vm_page_family_t* current_family){

    uint32_t page_count = 0;

    if(current_family == NULL){

        return page_count;
    }

    vm_page_t* vm_page = current_family->first_page;

    if(vm_page == NULL){

        return page_count;
    }

    printf("Struct Name: %s, Struct Size: %d\n", current_family->struct_name, current_family->struct_size);

    meta_blk_t* meta_blk = NULL;
    vm_page_t* vm_page_ptr = vm_page;
    uint32_t meta_data_size = current_family->struct_size + META_SIZE;

    while(vm_page_ptr){

        uint32_t block_counter = 1;
        uint32_t OBC = 0, FBC = 0;

        printf(ANSI_COLOR_MAGENTA "\nVM Page: %u\n" ANSI_COLOR_RESET, ++page_count);
        printf("\tpre page = %p, next page = %p\n", vm_page_ptr->pre_page, vm_page_ptr->next_page);
        ITERATE_VM_PAGE_ALL_BLOCKS_BEGIN(vm_page_ptr, meta_blk)
            printf(ANSI_COLOR_RED "\tBlock %d: %p" ANSI_COLOR_RESET, block_counter++, meta_blk);
            printf(ANSI_COLOR_YELLOW "%s" ANSI_COLOR_RESET, meta_blk->is_free ? " F R E E D " : " ALLOCATED ");
            printf(ANSI_COLOR_BLUE "data_blk_size = %-6u offset = %-6u pre meta = %-14p next meta = %p\n" ANSI_COLOR_RESET,
                    meta_blk->data_blk_size, meta_blk->offset, meta_blk->pre_blk, meta_blk->next_blk);

            OBC = meta_blk->is_free ? OBC : OBC+1;
            FBC = meta_blk->is_free ? FBC+1 : FBC;
        ITERATE_VM_PAGE_ALL_BLOCKS_END

        printf("\t%-15sTotal blocks: %-6u Allocated blocks: %-6u Free blocks: %-6u Memory in use: %-6u\n\n",
                 current_family->struct_name, OBC + FBC, OBC, FBC, OBC * meta_data_size);

        vm_page_ptr = vm_page_ptr->next_page;
    }

    return page_count;
}


/**
 * instantiate structure info and store it into VM Page 
 */ 
void mm_instantiate_new_page_family(char* struct_name, uint32_t struct_size){

    if(struct_size > SYSTEM_PAGE_SIZE){

        #if MM_DEBUG
            printf("%s() can not instantiate size that exceeds %ld bytes!\n", __FUNCTION__, SYSTEM_PAGE_SIZE);
        #endif
        return;
    }

    vm_page_family_t* current_family = NULL;
    vm_page_family_list_t* new_vm_page_for_family = NULL;

    if(first_vm_page_for_family == NULL){

        first_vm_page_for_family = (vm_page_family_list_t*)mm_get_vm_page(1);
        first_vm_page_for_family->next = NULL;

        strncpy(first_vm_page_for_family->vm_page[0].struct_name, struct_name, MAX_NAME_LEN);
        first_vm_page_for_family->vm_page[0].struct_size = struct_size;
        first_vm_page_for_family->vm_page[0].first_page = NULL;
        glthread_init(&first_vm_page_for_family->vm_page[0].free_blks_pq);

        return;
    }

    uint32_t count = 0;

    ITERATE_PAGE_FAMILIES_BEGIN(first_vm_page_for_family, current_family)

        if(strncmp(current_family->struct_name, struct_name, MAX_NAME_LEN) == 0){

            #if MM_DEBUG
                printf("same structure %s!\n", struct_name);
            #endif

            assert(0);
        }
        ++count;
    ITERATE_PAGE_FAMILIES_END

    if(count == MAX_FAMILY_PER_PAGE){

        new_vm_page_for_family = (vm_page_family_list_t*)mm_get_vm_page(1);
        new_vm_page_for_family->next = first_vm_page_for_family;
        first_vm_page_for_family = new_vm_page_for_family;
        count = 0;    
    }

    strncpy(first_vm_page_for_family->vm_page[count].struct_name, struct_name, MAX_NAME_LEN);
    first_vm_page_for_family->vm_page[count].struct_size = struct_size;
    first_vm_page_for_family->vm_page[count].first_page = NULL;
    glthread_init(&first_vm_page_for_family->vm_page[count].free_blks_pq);
}


/**
 * iterate and print out structure messages
 */ 
void mm_print_registered_page_families(){

    if(first_vm_page_for_family == NULL){

        #if MM_DEBUG
            printf("first_vm_page_for_family should be instantiated first!\n");
        #endif
        return;
    }

    vm_page_family_list_t* list_ptr = first_vm_page_for_family;
    vm_page_family_t* current_family = NULL;
    uint32_t vm_number = 1;

    while(list_ptr){

        printf("VM%d: \n", vm_number);
        ITERATE_PAGE_FAMILIES_BEGIN(list_ptr, current_family)

            printf("Struct Name: %s, Struct Size: %d\n", current_family->struct_name, current_family->struct_size);
        ITERATE_PAGE_FAMILIES_END

        list_ptr = list_ptr->next;
        ++vm_number;

        printf("\r\n");
    }

}


/**
 * find particular struct_name within vm_page_family_list_t
 */ 
vm_page_family_t* lookup_page_family_by_name(char *struct_name){

    if(first_vm_page_for_family == NULL){
        
        #if MM_DEBUG
            printf("first_vm_page_for_family should be instantiated first!\n");
        #endif
        return NULL;
    }

    vm_page_family_list_t* list_ptr = first_vm_page_for_family;
    vm_page_family_t* current_family = NULL;

    while(list_ptr){

        ITERATE_PAGE_FAMILIES_BEGIN(list_ptr, current_family)
            
            if(strncmp(current_family->struct_name, struct_name, MAX_NAME_LEN) == 0){
                return current_family;
            }
        ITERATE_PAGE_FAMILIES_END

        list_ptr = list_ptr->next;
    }

    return NULL;
}


/**
 * check the vm_page is empty or not
 */ 
vm_bool_t mm_vm_page_is_empty(vm_page_t* vm_page){

    assert(vm_page);

    vm_bool_t ret = vm_page->meta_blk.is_free == MM_TRUE && 
                    vm_page->meta_blk.next_blk == NULL && 
                    vm_page->meta_blk.pre_blk == NULL
                    ? MM_TRUE : MM_FALSE;

    return ret;
}


/**
 * VM Page Insertion
 */ 
vm_page_t* allocate_vm_page(vm_page_family_t* vm_page_family){

    if(vm_page_family == NULL){

        #if MM_DEBUG
            printf("Insert VM Page is NULL!\n");
        #endif
        return NULL;
    }

    vm_page_t* new_page = mm_get_vm_page(1);

    /* set the back pointer to page family */
    new_page->page_family = vm_page_family;

    /* meta block init */
    MARK_VM_PAGE_EMPTY(new_page);
    glthread_init(&new_page->meta_blk.priority_thread_glue);
    new_page->meta_blk.data_blk_size = mm_max_page_allocatable_memory(1);
    new_page->meta_blk.offset = offset_of(vm_page_t, meta_blk);
    new_page->pre_page = NULL;
    new_page->next_page = NULL;

    /* mantain vm_page_t dll */
    if(vm_page_family->first_page == NULL){

        vm_page_family->first_page = new_page;
        return new_page;
    }

    new_page->next_page = vm_page_family->first_page;
    vm_page_family->first_page->pre_page = new_page;
    vm_page_family->first_page = new_page;

    return new_page;
}


/**
 * VM Page Deletion
 */ 
void mm_page_delete_and_free(vm_page_t* vm_page){

    if(vm_page == NULL){

        #if MM_DEBUG
            printf("Delete VM Page is NULL!\n");
        #endif
        return;
    }

    /* vm_page is the first page */
    if(vm_page->page_family->first_page == vm_page){

        vm_page->page_family->first_page = vm_page->next_page;
        if(vm_page->next_page){

            vm_page->pre_page = NULL;
        }
        vm_page->pre_page = NULL;
        vm_page->next_page = NULL;
        mm_release_vm_page(vm_page, 1);
        return;
    }

    /* vm_page is at the middle of the dll */
    vm_page->pre_page->next_page = vm_page->next_page;
    if(vm_page->next_page){

        vm_page->next_page->pre_page = vm_page->pre_page;
    }
    vm_page->pre_page = NULL;
    vm_page->next_page = NULL;
    mm_release_vm_page(vm_page, 1);
}


/**
 * test function 
 */
void mm_debug_fn(){
    
    mm_init();
    printf("VM Page Size: %lu\n", SYSTEM_PAGE_SIZE);

    void* addr1 = mm_get_vm_page(1);
    void* addr2 = mm_get_vm_page(1);
    printf("VM1: %p, VM2: %p\n", addr1, addr2);

    mm_release_vm_page(addr1, 1);
    mm_release_vm_page(addr2, 1);
}


/**
 * dynamic memory allocation fnuc for applications
 */ 
void* zalloc(char* struct_name, int units){

    if(struct_name == NULL || units == 0){

        return NULL;
    }

    vm_page_family_t* page_family = NULL;

    if((page_family = lookup_page_family_by_name(struct_name)) == NULL){

        #if MM_DEBUG
            printf("structure %s can't be found!\n", struct_name);
        #endif

        return NULL;
    }

    uint32_t total_struct_size = page_family->struct_size * units;
    meta_blk_t* free_blk = NULL;

    if(total_struct_size > mm_max_page_allocatable_memory(1)){

        #if MM_DEBUG
            printf("Memory requested exeeds page size!\n");
        #endif

        return NULL; // can add extra operation to this senario
    }

    free_blk = mm_allocate_free_data_block(page_family, total_struct_size);

    if(free_blk){

        memset(free_blk + 1, 0x0, total_struct_size);
        return free_blk + 1;
    }

    return NULL;
}


/**
 * 
 */ 
void zfree(void* addr){

    meta_blk_t* free_blk = GET_META_BLK(addr);
    assert(free_blk->is_free == MM_FALSE);
    mm_free_blocks(free_blk);
}


/**
 * 
 */ 
void mm_print_memory_usage(){

    if(first_vm_page_for_family == NULL){
        
        #if MM_DEBUG
            printf("first_vm_page_for_family should be instantiated first!\n");
        #endif
        return;
    }

    vm_page_family_list_t* list_ptr = first_vm_page_for_family;
    vm_page_family_t* current_family = NULL;
    uint32_t count = 1;
    uint32_t total_page = 0;

    while(list_ptr){

        printf(ANSI_COLOR_GREEN "VM Family Page %d size = %ld\n" ANSI_COLOR_RESET, count++ ,SYSTEM_PAGE_SIZE);
        ITERATE_PAGE_FAMILIES_BEGIN(list_ptr, current_family)
            total_page += mm_print_meta_block_usage(current_family);
        ITERATE_PAGE_FAMILIES_END

        list_ptr = list_ptr->next;
    }

    printf("Total Memory being used by Memory Manager = %lu\n", total_page*SYSTEM_PAGE_SIZE);
}
