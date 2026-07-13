#include "proc.h"

/*
 * Process loader. Builds a ring-3 address space for a static Linux ELF, lays
 * out the System V AMD64 initial stack (argc/argv/envp and a full auxiliary
 * vector), and transfers control to user mode. One process runs at a time; the
 * kernel shell resumes when the process exits or faults.
 */

#define MAX_ARGS 16
#define BRK_LIMIT 0x01000000ULL       /* heap ceiling: 16 MiB */
#define MMAP_GUARD 0x00010000ULL       /* gap between mmap arena and the stack */

/* Auxiliary vector types (see the Linux ELF ABI). */
#define AT_NULL 0
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_FLAGS 8
#define AT_ENTRY 9
#define AT_UID 11
#define AT_EUID 12
#define AT_GID 13
#define AT_EGID 14
#define AT_HWCAP 16
#define AT_CLKTCK 17
#define AT_SECURE 23
#define AT_RANDOM 25
#define AT_EXECFN 31

process_t *current_process;

static process_t process_slot;

static const char *const default_env[] = {
    "PATH=/", "HOME=/", "TERM=linux", "SHELL=/bin/sh", "USER=root",
};
#define DEFAULT_ENV_COUNT (sizeof(default_env) / sizeof(default_env[0]))

static uint64_t stack_rng;

static uint8_t random_byte(void) {
    if (!stack_rng) {
        uint32_t low, high;
        __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
        stack_rng = (((uint64_t)high << 32) | low) | 1;
    }
    stack_rng ^= stack_rng << 13;
    stack_rng ^= stack_rng >> 7;
    stack_rng ^= stack_rng << 17;
    return (uint8_t)stack_rng;
}

static uintptr_t push_bytes(uintptr_t sp, const void *src, size_t len) {
    sp -= len;
    memcpy((void *)sp, src, len);
    return sp;
}

static uintptr_t build_stack(const elf_image_t *img, int argc,
                             const char *const argv[]) {
    uintptr_t argv_addr[MAX_ARGS];
    uintptr_t env_addr[DEFAULT_ENV_COUNT];
    uintptr_t sp = USER_STACK_TOP;

    for (int i = argc - 1; i >= 0; i--) {
        sp = push_bytes(sp, argv[i], strlen(argv[i]) + 1);
        argv_addr[i] = sp;
    }
    for (int i = (int)DEFAULT_ENV_COUNT - 1; i >= 0; i--) {
        sp = push_bytes(sp, default_env[i], strlen(default_env[i]) + 1);
        env_addr[i] = sp;
    }

    uint8_t random16[16];
    for (int i = 0; i < 16; i++) random16[i] = random_byte() & 0; /* TEMP diag */
    sp = push_bytes(sp, random16, sizeof(random16));
    uintptr_t random_addr = sp;

    sp &= ~(uintptr_t)0xf;

    uint64_t aux[32];
    size_t naux = 0;
    if (img->phdr_vaddr) {
        aux[naux++] = AT_PHDR;   aux[naux++] = img->phdr_vaddr;
        aux[naux++] = AT_PHENT;  aux[naux++] = img->phentsize;
        aux[naux++] = AT_PHNUM;  aux[naux++] = img->phnum;
    }
    aux[naux++] = AT_PAGESZ; aux[naux++] = PAGE_SIZE;
    aux[naux++] = AT_BASE;   aux[naux++] = 0;
    aux[naux++] = AT_FLAGS;  aux[naux++] = 0;
    aux[naux++] = AT_ENTRY;  aux[naux++] = img->entry;
    aux[naux++] = AT_UID;    aux[naux++] = 0;
    aux[naux++] = AT_EUID;   aux[naux++] = 0;
    aux[naux++] = AT_GID;    aux[naux++] = 0;
    aux[naux++] = AT_EGID;   aux[naux++] = 0;
    aux[naux++] = AT_HWCAP;  aux[naux++] = 0;
    aux[naux++] = AT_CLKTCK; aux[naux++] = 100;
    aux[naux++] = AT_SECURE; aux[naux++] = 0;
    aux[naux++] = AT_RANDOM; aux[naux++] = random_addr;
    aux[naux++] = AT_EXECFN; aux[naux++] = argc > 0 ? argv_addr[0] : random_addr;
    aux[naux++] = AT_NULL;   aux[naux++] = 0;

    size_t slots = 1 + (size_t)(argc + 1) + (DEFAULT_ENV_COUNT + 1) + naux;
    sp -= slots * 8;
    sp &= ~(uintptr_t)0xf;

    uint64_t *stack = (uint64_t *)sp;
    size_t idx = 0;
    stack[idx++] = (uint64_t)argc;
    for (int i = 0; i < argc; i++) stack[idx++] = argv_addr[i];
    stack[idx++] = 0;
    for (size_t i = 0; i < DEFAULT_ENV_COUNT; i++) stack[idx++] = env_addr[i];
    stack[idx++] = 0;
    for (size_t i = 0; i < naux; i++) stack[idx++] = aux[i];

    return sp;
}

static void init_console_fds(process_t *proc) {
    memset(proc->fds, 0, sizeof(proc->fds));
    for (int i = 0; i < 3; i++) {
        proc->fds[i].in_use = true;
        proc->fds[i].is_console = true;
    }
}

int process_run(const uint8_t *elf_data, size_t elf_size, int argc,
                const char *const argv[]) {
    if (argc > MAX_ARGS) argc = MAX_ARGS;

    /* Stage the module into the frame pool before installing the process CR3.
     * The pool is identity-mapped in every address space, whereas the module's
     * original location may fall inside the user virtual window and become
     * unreachable once the process page tables are active. */
    uintptr_t staged = pmm_alloc_contiguous(elf_size);
    if (!staged) {
        console_write("run: out of memory staging module image\n");
        return -1;
    }
    memcpy((void *)staged, elf_data, elf_size);
    const uint8_t *image = (const uint8_t *)staged;

    address_space_t *space = vmm_create();
    if (!space) {
        pmm_free_contiguous(staged, elf_size);
        console_write("run: out of memory building address space\n");
        return -1;
    }

    vmm_switch(space);

    elf_image_t img;
    int rc = elf_load(space, image, elf_size, &img);
    if (rc != 0) {
        vmm_switch_kernel();
        vmm_destroy(space);
        pmm_free_contiguous(staged, elf_size);
        console_write("run: not a loadable x86_64 Linux ELF (code ");
        console_write_dec((uint64_t)(-rc));
        console_write(")\n");
        return -1;
    }

    if (vmm_map_user_alloc(space, USER_STACK_TOP - USER_STACK_SIZE,
                           USER_STACK_SIZE, true, false) != 0) {
        vmm_switch_kernel();
        vmm_destroy(space);
        pmm_free_contiguous(staged, elf_size);
        console_write("run: could not map user stack\n");
        return -1;
    }

    process_t *proc = &process_slot;
    memset(proc, 0, sizeof(*proc));
    proc->space = space;
    proc->brk_start = img.load_end;
    proc->brk_cur = img.load_end;
    proc->brk_max = BRK_LIMIT;
    proc->mmap_next = USER_STACK_TOP - USER_STACK_SIZE - MMAP_GUARD;
    proc->mmap_floor = BRK_LIMIT;
    init_console_fds(proc);

    uintptr_t user_rsp = build_stack(&img, argc, argv);

    current_process = proc;
    tss_set_kernel_stack(syscall_kernel_stack_top);

    long status = enter_user(img.entry, user_rsp);

    vmm_switch_kernel();
    current_process = NULL;
    vmm_destroy(space);
    pmm_free_contiguous(staged, elf_size);
    return (int)status;
}
