#ifndef SABLEOS_KERNEL_H
#define SABLEOS_KERNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int value, size_t n);
int memcmp(const void *a, const void *b, size_t n);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char *s);
void console_write_dec(uint64_t value);
void console_write_hex(uint64_t value);
void serial_init(void);
void serial_putc(char c);

void keyboard_init(void);
char keyboard_read(void);

#define RAMFS_MAX_FILES 32
#define RAMFS_NAME_MAX 31
#define RAMFS_DATA_MAX 512
typedef struct {
    bool used;
    char name[RAMFS_NAME_MAX + 1];
    char data[RAMFS_DATA_MAX + 1];
    size_t size;
} ramfs_file_t;

void ramfs_init(void);
int ramfs_create(const char *name);
int ramfs_write(const char *name, const char *data);
const ramfs_file_t *ramfs_find(const char *name);
int ramfs_remove(const char *name);
const ramfs_file_t *ramfs_at(size_t index);

void threads_init(void);
int thread_create(const char *name, void (*entry)(void));
void thread_yield(void);
size_t thread_count(void);
const char *thread_name(size_t index);

uint32_t cpu_discover(void);
void cpu_enable_lapic(void);
uint32_t cpu_start_aps(void);
void cpu_enable_sse(void);

/* ---- Linux x86_64 ABI compatibility layer ---------------------------- */

/* Physical memory layout used by the userspace execution facility.
 * [0, USER_WINDOW_BASE)  : identity-mapped kernel image, boot structures, VGA.
 * [USER_WINDOW_BASE, USER_WINDOW_TOP) : virtual window for ring-3 programs.
 * [FRAME_POOL_BASE, ram_top) : identity-mapped pool the frame allocator owns. */
#define USER_WINDOW_BASE 0x00400000ULL /* 4 MiB  */
#define USER_WINDOW_TOP  0x02000000ULL /* 32 MiB */
#define FRAME_POOL_BASE  0x02000000ULL /* 32 MiB */
#define USER_STACK_TOP   0x01ff0000ULL /* just below the window top */
#define USER_STACK_SIZE  0x00040000ULL /* 256 KiB */
#define PAGE_SIZE        0x1000ULL

typedef struct {
    uintptr_t phys_start;
    uintptr_t phys_end;
    const char *string;
} boot_module_t;

void multiboot_parse(uint32_t info_phys);
uintptr_t multiboot_ram_top(void);
size_t multiboot_module_count(void);
const boot_module_t *multiboot_module(size_t index);

void pmm_init(uintptr_t ram_top);
uintptr_t pmm_alloc_frame(void);
void pmm_free_frame(uintptr_t frame);
void pmm_reserve(uintptr_t start, uintptr_t end);
uintptr_t pmm_alloc_contiguous(size_t bytes);
void pmm_free_contiguous(uintptr_t base, size_t bytes);
size_t pmm_free_count(void);

/* An address space is a top-level page table (PML4) plus per-process state. */
typedef struct address_space address_space_t;
void vmm_init(void);
address_space_t *vmm_create(void);
void vmm_destroy(address_space_t *space);
int vmm_map_user(address_space_t *space, uintptr_t vaddr, uintptr_t paddr,
                 bool writable, bool executable);
int vmm_map_user_alloc(address_space_t *space, uintptr_t vaddr, size_t length,
                       bool writable, bool executable);
bool vmm_user_mapped(address_space_t *space, uintptr_t vaddr);
void vmm_switch(address_space_t *space);
void vmm_switch_kernel(void);
uintptr_t vmm_phys_root(address_space_t *space);

void gdt_init(void);
void tss_set_kernel_stack(uintptr_t stack_top);
extern uintptr_t syscall_kernel_stack_top;
void idt_init(void);
void syscall_init(void);

/* ELF64 program image description filled in by the loader. */
typedef struct {
    uintptr_t entry;
    uintptr_t phdr_vaddr;
    uint64_t phentsize;
    uint64_t phnum;
    uintptr_t load_end;
} elf_image_t;

int elf_load(address_space_t *space, const uint8_t *data, size_t size,
             elf_image_t *out);
bool elf_header_valid(const void *data, size_t size);

/* Run a Linux ELF image in ring 3. Returns the process exit status. */
int process_run(const uint8_t *elf_data, size_t elf_size, int argc,
                const char *const argv[]);

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

#endif
