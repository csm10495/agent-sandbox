#include "thread.h"
#include "memory.h"
#include "vga.h"
#include "pit.h"
#include "string.h"

static thread_t  threads[MAX_THREADS];
static thread_t *current  = NULL;  /* currently running thread */
static thread_t *run_head = NULL;  /* head of circular ready list */
static uint32_t  next_tid = 1;
static int       alive    = 0;

/* ------------------------------------------------------------------ */
static thread_t *thread_alloc(void) {
    for (int i = 0; i < MAX_THREADS; i++)
        if (threads[i].state == THREAD_DEAD || threads[i].id == 0)
            return &threads[i];
    return NULL;
}

/* Insert at end of circular list */
static void list_insert(thread_t *t) {
    if (!run_head) {
        run_head = t; t->next = t;
    } else {
        /* find tail */
        thread_t *tail = run_head;
        while (tail->next != run_head) tail = tail->next;
        tail->next = t; t->next = run_head;
    }
}

/* Remove from circular list */
static void list_remove(thread_t *t) {
    if (!run_head) return;
    if (run_head == t && t->next == t) { run_head = NULL; return; }
    thread_t *prev = t->next;
    while (prev->next != t) prev = prev->next;
    prev->next = t->next;
    if (run_head == t) run_head = t->next;
}

/* ------------------------------------------------------------------ */
void thread_init(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i].id    = 0;
        threads[i].state = THREAD_DEAD;
    }

    /* Adopt the current executing context as thread 0 (the main/idle thread) */
    thread_t *main_t = &threads[0];
    main_t->id    = next_tid++;
    main_t->state = THREAD_RUNNING;
    strncpy(main_t->name, "main", THREAD_NAME_LEN - 1);
    main_t->stack_base = NULL;   /* uses original boot stack */
    main_t->next       = main_t;

    run_head = main_t;
    current  = main_t;
    alive    = 1;
}

thread_t *thread_create(void (*func)(void *), void *arg, const char *name) {
    thread_t *t = thread_alloc();
    if (!t) return NULL;

    /* Allocate stack */
    uint8_t *stack = (uint8_t *)pmm_alloc();
    if (!stack) return NULL;
    /* Second page for larger stack */
    uint8_t *stack2 = (uint8_t *)pmm_alloc();
    (void)stack2; /* contiguous if lucky; just use one page */

    t->id         = next_tid++;
    t->state      = THREAD_READY;
    t->stack_base = stack;
    t->sleep_until = 0;
    strncpy(t->name, name ? name : "thread", THREAD_NAME_LEN - 1);

    /* Set up initial stack frame for thread_switch_asm/thread_trampoline.
     * thread_switch_asm saves: rbx,rbp,r12,r13,r14,r15,rflags then RSP.
     * For a new thread we fake that frame so "ret" lands in thread_trampoline,
     * with r12=func, r13=arg.
     */
    uint64_t *sp = (uint64_t *)(stack + PMM_PAGE_SIZE);
    *--sp = (uint64_t)thread_trampoline; /* return address for thread_switch_asm */
    *--sp = 0x202;     /* rflags: IF=1, reserved bit */
    *--sp = 0;         /* r15 */
    *--sp = 0;         /* r14 */
    *--sp = (uint64_t)arg;   /* r13 */
    *--sp = (uint64_t)func;  /* r12 */
    *--sp = 0;         /* rbp */
    *--sp = 0;         /* rbx */

    t->saved_rsp = (uint64_t)sp;

    list_insert(t);
    alive++;
    return t;
}

/* Round-robin scheduler: pick next READY or SLEEPING thread */
static thread_t *pick_next(void) {
    if (!run_head) return NULL;
    uint64_t now = pit_ticks();

    thread_t *start = current->next;
    thread_t *t = start;
    do {
        if (t->state == THREAD_SLEEPING && t->sleep_until <= now)
            t->state = THREAD_READY;
        if (t->state == THREAD_READY || t->state == THREAD_RUNNING)
            return t;
        t = t->next;
    } while (t != start);
    return current;  /* no other thread; continue */
}

void thread_yield(void) {
    thread_t *from = current;
    thread_t *to   = pick_next();
    if (to == from) return;

    from->state = (from->state == THREAD_RUNNING) ? THREAD_READY : from->state;
    to->state   = THREAD_RUNNING;
    current     = to;

    thread_switch_asm(&from->saved_rsp, to->saved_rsp);
}

void thread_exit(void) {
    thread_t *dying = current;
    dying->state = THREAD_DEAD;
    list_remove(dying);
    if (dying->stack_base) {
        pmm_free(dying->stack_base);
        dying->stack_base = NULL;
    }
    alive--;

    /* Switch to next thread (must exist: main thread never exits) */
    thread_t *to = run_head;
    while (to && to->state == THREAD_DEAD) to = to->next;
    if (!to) { cpu_cli(); for (;;) cpu_halt(); }

    to->state = THREAD_RUNNING;
    current   = to;
    thread_switch_asm(&dying->saved_rsp, to->saved_rsp);
    /* never returns */
    for (;;) cpu_halt();
}

void thread_sleep_ms(uint32_t ms) {
    current->state      = THREAD_SLEEPING;
    current->sleep_until = pit_ticks() + ((uint64_t)ms * 100) / 1000 + 1;
    thread_yield();
}

thread_t *thread_current(void) { return current; }
int       thread_count(void)   { return alive; }

void thread_list(void) {
    static const char *states[] = {"READY","RUN  ","SLEEP","DEAD "};
    vga_printf("  ID  State  Name\n");
    vga_printf("  --  -----  ----\n");
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_t *t = &threads[i];
        if (t->id == 0) continue;
        vga_printf("  %2lu  %s  %s\n",
                   (uint64_t)t->id,
                   states[t->state < 4 ? t->state : 3],
                   t->name);
    }
}
