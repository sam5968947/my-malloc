#include "uapi_mm.h"

typedef struct _emp{

    char name[MAX_NAME_LEN];
    uint32_t emp_id;
}emp_t;


typedef struct _student{

    char name[MAX_NAME_LEN];
    uint32_t rollno;
    uint32_t marks_phys;
    uint32_t marks_chem;
    uint32_t marks_maths;
    struct _student* next;
}student_t;


typedef struct _teacher{

    char name[MAX_NAME_LEN];
    uint32_t id;
    uint32_t major;
    struct _teacher* next;
}teacher_t;


void testapp_demo(){

    int wait;
    mm_init();

    MM_REG_STRUCT(emp_t);
    MM_REG_STRUCT(student_t);
    MM_REG_STRUCT(teacher_t);

#if 1
    printf(ANSI_COLOR_YELLOW "Phase 1: \n\n" ANSI_COLOR_RESET);

    emp_t* emp1 = ZMALLOC(emp_t, 1);
    emp_t* emp2 = ZMALLOC(emp_t, 1);
    emp_t* emp3 = ZMALLOC(emp_t, 1);

    student_t* stu1 = ZMALLOC(student_t, 1);
    student_t* stu2 = ZMALLOC(student_t, 1);

    mm_print_memory_usage();
    scanf("%d", &wait);

    printf(ANSI_COLOR_YELLOW "Phase 2: \n\n" ANSI_COLOR_RESET);

    ZFREE(emp1);
    ZFREE(emp3);

    ZFREE(stu2);

    mm_print_memory_usage();
    scanf("%d", &wait);

    printf(ANSI_COLOR_YELLOW "Phase 3: \n\n" ANSI_COLOR_RESET);

    ZFREE(emp2);
    ZFREE(stu1);   

    mm_print_memory_usage();

#endif

#if 0
    for(int i=0; i<500; i++){

        emp_t* emp = ZMALLOC(emp_t, 1);
        ZFREE(emp);
    }

    mm_print_memory_usage();
#endif
    
}
