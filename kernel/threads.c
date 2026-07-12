#include "kernel.h"

#define MAX_THREADS 8
#define STACK_SIZE 8192
#define CONTEXT_SAVED_REGISTERS 6

typedef struct {
    uint64_t rsp;
    const char *name;
    bool active;
} thread_t;

static thread_t threads[MAX_THREADS];
static uint8_t stacks[MAX_THREADS][STACK_SIZE] __attribute__((aligned(16)));
static size_t current;
static size_t count;

extern void context_switch(uint64_t *old_rsp, const uint64_t *new_rsp);

static void thread_exit(void) {
    threads[current].active = false;
    for (;;) thread_yield();
}

void threads_init(void) {
    memset(threads, 0, sizeof(threads));
    threads[0].name = "shell";
    threads[0].active = true;
    count = 1;
}

int thread_create(const char *name, void (*entry)(void)) {
    if (count == MAX_THREADS) return -1;
    size_t id = count++;
    uintptr_t top = (uintptr_t)&stacks[id][STACK_SIZE];
    top &= ~(uintptr_t)0xf;
    uint64_t *sp = (uint64_t *)top;
    *--sp = (uint64_t)thread_exit;
    *--sp = (uint64_t)entry;
    for (int i = 0; i < CONTEXT_SAVED_REGISTERS; i++) *--sp = 0;
    threads[id].rsp = (uint64_t)sp;
    threads[id].name = name;
    threads[id].active = true;
    return (int)id;
}

void thread_yield(void) {
    size_t next = current;
    size_t checked = 0;
    while (checked++ < count) {
        next = (next + 1) % count;
        if (threads[next].active) break;
    }
    if (!threads[next].active) {
        for (;;) __asm__ volatile("cli; hlt");
    }
    if (next == current) return;
    size_t old = current;
    current = next;
    context_switch(&threads[old].rsp, &threads[next].rsp);
}

size_t thread_count(void) {
    return count;
}

const char *thread_name(size_t index) {
    return index < count && threads[index].active ? threads[index].name : NULL;
}
