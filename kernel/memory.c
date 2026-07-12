#include "memory.h"
#include "string.h"

/* Bitmap: 1 bit per page (1=free, 0=used).
 * Pages managed: [base_page, base_page + total_pages)
 * Physical memory starts at PHYS_BASE = 2MB (above kernel at 1MB).
 */
#define PHYS_BASE  (2UL * 1024 * 1024)   /* start managing from 2 MB */

static uint32_t bitmap[PMM_MAX_PAGES / 32];
static uint64_t base_page;    /* first page index (physical addr / page_size) */
static uint64_t total_pages;
static uint64_t free_count;

static inline void bit_set(uint64_t idx) {
    bitmap[idx / 32] |=  (1u << (idx & 31));
}
static inline void bit_clr(uint64_t idx) {
    bitmap[idx / 32] &= ~(1u << (idx & 31));
}
static inline bool bit_get(uint64_t idx) {
    return (bitmap[idx / 32] >> (idx & 31)) & 1;
}

void pmm_init(uintptr_t kernel_end) {
    /* Round kernel_end up to next page boundary */
    uintptr_t start = ALIGN_UP(kernel_end, PMM_PAGE_SIZE);
    if (start < PHYS_BASE) start = PHYS_BASE;

    /* Assume 128 MB total RAM; stop before BIOS region */
    uintptr_t end = 128UL * 1024 * 1024;
    if (end > 0x00FFFFFF00UL) end = 0x00FFFFFF00UL;

    base_page   = start / PMM_PAGE_SIZE;
    total_pages = (end - start) / PMM_PAGE_SIZE;
    if (total_pages > PMM_MAX_PAGES) total_pages = PMM_MAX_PAGES;

    memset(bitmap, 0xFF, sizeof(bitmap));   /* mark everything free initially */
    /* Clear pages we don't manage (beyond total_pages) */
    for (uint64_t i = total_pages; i < PMM_MAX_PAGES; i++) bit_clr(i);
    free_count = total_pages;
}

void *pmm_alloc(void) {
    for (uint64_t i = 0; i < total_pages; i++) {
        if (bit_get(i)) {
            bit_clr(i);
            free_count--;
            uintptr_t phys = (base_page + i) * PMM_PAGE_SIZE;
            return (void *)phys;
        }
    }
    return NULL;   /* out of memory */
}

void pmm_free(void *page) {
    uintptr_t phys = (uintptr_t)page;
    uint64_t  idx  = (phys / PMM_PAGE_SIZE) - base_page;
    if (idx < total_pages && !bit_get(idx)) {
        bit_set(idx);
        free_count++;
    }
}

uint64_t pmm_free_pages(void)  { return free_count; }
uint64_t pmm_total_pages(void) { return total_pages; }
