#include "kernel.h"

/*
 * Virtual memory manager for ring-3 address spaces.
 *
 * Every user address space is a private four-level page table that:
 *   - identity-maps [0, 4 GiB) with 2 MiB supervisor pages, matching the boot
 *     mapping, so the kernel, VGA, LAPIC and the frame pool stay reachable
 *     while a process and its syscalls run in the same CR3; and
 *   - carves the range [USER_WINDOW_BASE, USER_WINDOW_TOP) out of that identity
 *     map, backing it with 4 KiB user pages instead. Access control is the AND
 *     of the user/supervisor bit across levels, so marking the shared upper
 *     tables user-accessible never exposes the supervisor 2 MiB leaves.
 */

#define PTE_PRESENT 0x001ULL
#define PTE_WRITE   0x002ULL
#define PTE_USER    0x004ULL
#define PTE_PS      0x080ULL
#define PTE_ADDR_MASK 0x000ffffffffff000ULL

#define ENTRIES_PER_TABLE 512
#define IDENTITY_LIMIT (4ULL * 1024 * 1024 * 1024)

struct address_space {
    uintptr_t pml4_phys;
    bool used;
};

#define MAX_ADDRESS_SPACES 4
static struct address_space spaces[MAX_ADDRESS_SPACES];
static uintptr_t kernel_cr3;

static uint64_t *table_at(uintptr_t phys) { return (uint64_t *)phys; }

static uintptr_t read_cr3(void) {
    uintptr_t value;
    __asm__ volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

void vmm_init(void) { kernel_cr3 = read_cr3(); }

static bool in_user_window(uintptr_t vaddr) {
    return vaddr >= USER_WINDOW_BASE && vaddr < USER_WINDOW_TOP;
}

address_space_t *vmm_create(void) {
    struct address_space *space = NULL;
    for (size_t i = 0; i < MAX_ADDRESS_SPACES; i++) {
        if (!spaces[i].used) {
            space = &spaces[i];
            break;
        }
    }
    if (!space) return NULL;

    uintptr_t pml4 = pmm_alloc_frame();
    if (!pml4) return NULL;
    uintptr_t pdpt = pmm_alloc_frame();
    if (!pdpt) {
        pmm_free_frame(pml4);
        return NULL;
    }
    table_at(pml4)[0] = pdpt | PTE_PRESENT | PTE_WRITE | PTE_USER;

    for (uint64_t gib = 0; gib < 4; gib++) {
        uintptr_t pd = pmm_alloc_frame();
        if (!pd) {
            space->pml4_phys = pml4;
            space->used = true;
            vmm_destroy(space);
            return NULL;
        }
        table_at(pdpt)[gib] = pd | PTE_PRESENT | PTE_WRITE | PTE_USER;
        for (uint64_t j = 0; j < ENTRIES_PER_TABLE; j++) {
            uint64_t base = gib * (1ULL << 30) + j * (2ULL << 20);
            if (base >= USER_WINDOW_BASE && base < USER_WINDOW_TOP) {
                table_at(pd)[j] = 0; /* filled lazily with a 4 KiB user PT */
            } else {
                table_at(pd)[j] = base | PTE_PRESENT | PTE_WRITE | PTE_PS;
            }
        }
    }

    space->pml4_phys = pml4;
    space->used = true;
    return space;
}

static uint64_t *user_pte(address_space_t *space, uintptr_t vaddr, bool create) {
    if (!in_user_window(vaddr)) return NULL;
    uint64_t *pml4 = table_at(space->pml4_phys);
    uint64_t *pdpt = table_at(pml4[0] & PTE_ADDR_MASK);
    size_t pdpt_index = (vaddr >> 30) & 0x1ff;
    uint64_t *pd = table_at(pdpt[pdpt_index] & PTE_ADDR_MASK);
    size_t pd_index = (vaddr >> 21) & 0x1ff;

    uint64_t pd_entry = pd[pd_index];
    uintptr_t pt_phys;
    if (pd_entry & PTE_PRESENT) {
        pt_phys = pd_entry & PTE_ADDR_MASK;
    } else {
        if (!create) return NULL;
        pt_phys = pmm_alloc_frame();
        if (!pt_phys) return NULL;
        pd[pd_index] = pt_phys | PTE_PRESENT | PTE_WRITE | PTE_USER;
    }
    uint64_t *pt = table_at(pt_phys);
    return &pt[(vaddr >> 12) & 0x1ff];
}

int vmm_map_user(address_space_t *space, uintptr_t vaddr, uintptr_t paddr,
                 bool writable, bool executable) {
    (void)executable; /* NX is left disabled; all user pages stay executable. */
    uint64_t *pte = user_pte(space, vaddr & ~(PAGE_SIZE - 1), true);
    if (!pte) return -1;
    uint64_t flags = PTE_PRESENT | PTE_USER;
    if (writable) flags |= PTE_WRITE;
    *pte = (paddr & PTE_ADDR_MASK) | flags;
    return 0;
}

int vmm_map_user_alloc(address_space_t *space, uintptr_t vaddr, size_t length,
                       bool writable, bool executable) {
    uintptr_t start = vaddr & ~(PAGE_SIZE - 1);
    uintptr_t end = (vaddr + length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    for (uintptr_t page = start; page < end; page += PAGE_SIZE) {
        if (vmm_user_mapped(space, page)) continue;
        uintptr_t frame = pmm_alloc_frame();
        if (!frame) return -1;
        if (vmm_map_user(space, page, frame, writable, executable) != 0) {
            pmm_free_frame(frame);
            return -1;
        }
    }
    return 0;
}

bool vmm_user_mapped(address_space_t *space, uintptr_t vaddr) {
    uint64_t *pte = user_pte(space, vaddr & ~(PAGE_SIZE - 1), false);
    return pte && (*pte & PTE_PRESENT);
}

void vmm_switch(address_space_t *space) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(space->pml4_phys) : "memory");
}

void vmm_switch_kernel(void) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(kernel_cr3) : "memory");
}

uintptr_t vmm_phys_root(address_space_t *space) { return space->pml4_phys; }

static void free_table_tree(uintptr_t pml4_phys) {
    uint64_t *pml4 = table_at(pml4_phys);
    uint64_t pml4e = pml4[0];
    if (!(pml4e & PTE_PRESENT)) {
        pmm_free_frame(pml4_phys);
        return;
    }
    uintptr_t pdpt_phys = pml4e & PTE_ADDR_MASK;
    uint64_t *pdpt = table_at(pdpt_phys);
    for (uint64_t gib = 0; gib < 4; gib++) {
        uint64_t pdpte = pdpt[gib];
        if (!(pdpte & PTE_PRESENT)) continue;
        uintptr_t pd_phys = pdpte & PTE_ADDR_MASK;
        uint64_t *pd = table_at(pd_phys);
        for (uint64_t j = 0; j < ENTRIES_PER_TABLE; j++) {
            uint64_t pde = pd[j];
            if (!(pde & PTE_PRESENT) || (pde & PTE_PS)) continue;
            uintptr_t pt_phys = pde & PTE_ADDR_MASK;
            uint64_t *pt = table_at(pt_phys);
            for (uint64_t k = 0; k < ENTRIES_PER_TABLE; k++) {
                if (pt[k] & PTE_PRESENT) pmm_free_frame(pt[k] & PTE_ADDR_MASK);
            }
            pmm_free_frame(pt_phys);
        }
        pmm_free_frame(pd_phys);
    }
    pmm_free_frame(pdpt_phys);
    pmm_free_frame(pml4_phys);
}

void vmm_destroy(address_space_t *space) {
    if (!space || !space->used) return;
    free_table_tree(space->pml4_phys);
    space->pml4_phys = 0;
    space->used = false;
}
