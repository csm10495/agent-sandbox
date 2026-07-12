#pragma once
#include "types.h"

#define MAX_THREADS       32
#define THREAD_STACK_SIZE (8 * 1024)  /* 8 KB per thread */
#define THREAD_NAME_LEN   32

typedef enum {
    THREAD_READY = 0,
    THREAD_RUNNING,
    THREAD_SLEEPING,
    THREAD_DEAD
} thread_state_t;

typedef struct thread {
    uint64_t        saved_rsp;              /* must be first field */
    uint8_t        *stack_base;             /* allocated stack (low address) */
    char            name[THREAD_NAME_LEN];
    uint32_t        id;
    thread_state_t  state;
    uint64_t        sleep_until;            /* pit_ticks() target */
    struct thread  *next;                   /* circular run queue */
} thread_t;

void       thread_init(void);
thread_t  *thread_create(void (*func)(void *), void *arg, const char *name);
void       thread_yield(void);
void       thread_exit(void);
void       thread_sleep_ms(uint32_t ms);
thread_t  *thread_current(void);
int        thread_count(void);
void       thread_list(void);             /* prints thread table to VGA */

/* Assembly helpers */
extern void thread_switch_asm(uint64_t *save_rsp_ptr, uint64_t new_rsp);
extern void thread_trampoline(void);
