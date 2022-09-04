#ifndef __UAPI_MM_H_
#define __UAPI_MM_H_

#include "mm.h"

#define MM_REG_STRUCT(struct_name) mm_instantiate_new_page_family(#struct_name, sizeof(struct_name))

void testapp_demo(void);
void mm_print_memory_usage(void);
void* zalloc(char* struct_name, int units);
void zfree(void* addr);

#endif /* __UAPI_MM_H_ */