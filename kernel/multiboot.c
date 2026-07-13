#include "kernel.h"

/*
 * Minimal Multiboot2 information parser. GRUB hands the kernel the physical
 * address of the boot information structure in the first argument to
 * kernel_main. We recover two things from it: the top of usable RAM (so the
 * frame allocator knows how large the pool is) and any modules GRUB loaded
 * (the ring-3 programs we execute).
 */

#define MB2_TAG_END 0
#define MB2_TAG_MODULE 3
#define MB2_TAG_MMAP 6

#define MB2_MAX_MODULES 8

typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) mb2_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char string[];
} __attribute__((packed)) mb2_module_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    uint8_t entries[];
} __attribute__((packed)) mb2_mmap_t;

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

static uintptr_t ram_top = 0x08000000; /* conservative 128 MiB fallback */
static boot_module_t modules[MB2_MAX_MODULES];
static size_t module_count;

static uintptr_t align_up(uintptr_t value, uintptr_t align) {
    return (value + align - 1) & ~(align - 1);
}

void multiboot_parse(uint32_t info_phys) {
    module_count = 0;
    if (!info_phys) return;

    const uint8_t *base = (const uint8_t *)(uintptr_t)info_phys;
    uint32_t total_size;
    memcpy(&total_size, base, sizeof(total_size));

    uintptr_t discovered_top = 0;
    const uint8_t *p = base + 8; /* skip total_size + reserved */
    const uint8_t *end = base + total_size;
    while (p + sizeof(mb2_tag_t) <= end) {
        const mb2_tag_t *tag = (const mb2_tag_t *)p;
        if (tag->type == MB2_TAG_END) break;
        if (tag->size < sizeof(mb2_tag_t)) break;

        if (tag->type == MB2_TAG_MMAP) {
            const mb2_mmap_t *mmap = (const mb2_mmap_t *)tag;
            if (mmap->entry_size >= sizeof(mb2_mmap_entry_t)) {
                const uint8_t *e = mmap->entries;
                const uint8_t *e_end = p + tag->size;
                while (e + mmap->entry_size <= e_end) {
                    const mb2_mmap_entry_t *entry = (const mb2_mmap_entry_t *)e;
                    if (entry->type == 1) {
                        uintptr_t top = (uintptr_t)(entry->base_addr + entry->length);
                        if (top > discovered_top) discovered_top = top;
                    }
                    e += mmap->entry_size;
                }
            }
        } else if (tag->type == MB2_TAG_MODULE && module_count < MB2_MAX_MODULES) {
            const mb2_module_t *mod = (const mb2_module_t *)tag;
            modules[module_count].phys_start = mod->mod_start;
            modules[module_count].phys_end = mod->mod_end;
            modules[module_count].string = mod->string;
            module_count++;
        }

        p += align_up(tag->size, 8);
    }

    if (discovered_top >= FRAME_POOL_BASE + PAGE_SIZE) ram_top = discovered_top;
}

uintptr_t multiboot_ram_top(void) { return ram_top; }

size_t multiboot_module_count(void) { return module_count; }

const boot_module_t *multiboot_module(size_t index) {
    return index < module_count ? &modules[index] : NULL;
}
