#pragma once
#include "types.h"

/* Physical memory manager: 4KB page bitmap allocator.
 * Manages RAM above kernel_phys_end up to ~128MB.
 */
#define PMM_PAGE_SIZE  4096UL
#define PMM_MAX_PAGES  (128 * 1024 * 1024 / PMM_PAGE_SIZE)  /* 32768 pages */

void     pmm_init(uintptr_t kernel_end);
void    *pmm_alloc(void);          /* alloc one 4KB page, returns virtual=physical addr */
void     pmm_free(void *page);
uint64_t pmm_free_pages(void);
uint64_t pmm_total_pages(void);
