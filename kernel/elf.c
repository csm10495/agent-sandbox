#include "kernel.h"

/*
 * Static ELF64 loader for x86_64 Linux executables.
 *
 * The target address space must be active (its CR3 loaded) when this runs, so
 * segment contents can be written straight to their user virtual addresses.
 * Only PT_LOAD segments are honoured; the loader records the entry point and
 * the program-header location so the process layer can publish a correct
 * auxiliary vector.
 */

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

#define PT_LOAD 1
#define ET_EXEC 2
#define ET_DYN 3
#define EM_X86_64 62
#define PF_X 1
#define PF_W 2

static bool header_ok(const elf64_ehdr_t *eh, size_t size) {
    if (size < sizeof(*eh)) return false;
    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F')
        return false;
    if (eh->e_ident[4] != 2) return false;       /* ELFCLASS64 */
    if (eh->e_ident[5] != 1) return false;       /* little-endian */
    if (eh->e_machine != EM_X86_64) return false;
    if (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) return false;
    if (eh->e_phentsize < sizeof(elf64_phdr_t)) return false;
    if (eh->e_phoff + (uint64_t)eh->e_phnum * eh->e_phentsize > size) return false;
    return true;
}

/* Public validator, shared with host unit tests. Confirms the blob is a
 * little-endian 64-bit x86_64 ET_EXEC/ET_DYN image with a sane phdr table. */
bool elf_header_valid(const void *data, size_t size) {
    if (size < sizeof(elf64_ehdr_t)) return false;
    return header_ok((const elf64_ehdr_t *)data, size);
}

int elf_load(address_space_t *space, const uint8_t *data, size_t size,
             elf_image_t *out) {
    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)data;
    if (!header_ok(eh, size)) return -1;

    uint64_t bias = 0;
    if (eh->e_type == ET_DYN) bias = USER_WINDOW_BASE;

    uintptr_t load_end = 0;
    uintptr_t phdr_vaddr = 0;

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const elf64_phdr_t *ph =
            (const elf64_phdr_t *)(data + eh->e_phoff + (uint64_t)i * eh->e_phentsize);
        if (ph->p_type != PT_LOAD || ph->p_memsz == 0) continue;
        if (ph->p_offset + ph->p_filesz > size) return -2;

        uintptr_t vaddr = (uintptr_t)(ph->p_vaddr + bias);
        uintptr_t seg_end = vaddr + ph->p_memsz;
        if (vaddr < USER_WINDOW_BASE || seg_end > USER_WINDOW_TOP) return -3;

        bool writable = (ph->p_flags & PF_W) != 0;
        bool executable = (ph->p_flags & PF_X) != 0;
        if (vmm_map_user_alloc(space, vaddr, ph->p_memsz, true, executable) != 0)
            return -4;
        (void)writable;

        memcpy((void *)vaddr, data + ph->p_offset, ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz)
            memset((void *)(vaddr + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);

        if (seg_end > load_end) load_end = seg_end;

        /* Record where the program headers landed for AT_PHDR. */
        if (eh->e_phoff >= ph->p_offset &&
            eh->e_phoff + (uint64_t)eh->e_phnum * eh->e_phentsize <=
                ph->p_offset + ph->p_filesz)
            phdr_vaddr = vaddr + (uintptr_t)(eh->e_phoff - ph->p_offset);
    }

    if (!load_end) return -5;

    out->entry = (uintptr_t)(eh->e_entry + bias);
    out->phdr_vaddr = phdr_vaddr;
    out->phentsize = eh->e_phentsize;
    out->phnum = eh->e_phnum;
    out->load_end = (load_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    return 0;
}
