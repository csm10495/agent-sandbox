#include "kernel.h"

/*
 * Bitmap physical frame allocator. It owns the pool that starts at
 * FRAME_POOL_BASE and runs to the top of RAM discovered from Multiboot2.
 * Frames handed out are identity-mapped in every address space, so the kernel
 * can always touch a frame through its physical address while filling page
 * tables or copying program images.
 */

#define FRAME_SIZE PAGE_SIZE
/* Enough bitmap for a 4 GiB pool (1 Mi frames); clamped below if RAM is huge. */
#define MAX_FRAMES (1u << 20)

static uint8_t frame_bitmap[MAX_FRAMES / 8];
static uintptr_t pool_base;
static size_t pool_frames;
static size_t free_frames;
static size_t next_hint;

static void bitmap_set(size_t index) {
    frame_bitmap[index >> 3] |= (uint8_t)(1u << (index & 7));
}

static void bitmap_clear(size_t index) {
    frame_bitmap[index >> 3] &= (uint8_t)~(1u << (index & 7));
}

static bool bitmap_test(size_t index) {
    return (frame_bitmap[index >> 3] >> (index & 7)) & 1u;
}

void pmm_init(uintptr_t ram_top) {
    pool_base = FRAME_POOL_BASE;
    if (ram_top <= pool_base) {
        pool_frames = 0;
        free_frames = 0;
        return;
    }
    size_t frames = (size_t)((ram_top - pool_base) / FRAME_SIZE);
    if (frames > MAX_FRAMES) frames = MAX_FRAMES;
    pool_frames = frames;
    free_frames = frames;
    next_hint = 0;
    memset(frame_bitmap, 0, sizeof(frame_bitmap));
}

uintptr_t pmm_alloc_frame(void) {
    if (!free_frames) return 0;
    for (size_t scanned = 0; scanned < pool_frames; scanned++) {
        size_t index = (next_hint + scanned) % pool_frames;
        if (!bitmap_test(index)) {
            bitmap_set(index);
            free_frames--;
            next_hint = index + 1;
            uintptr_t frame = pool_base + (uintptr_t)index * FRAME_SIZE;
            memset((void *)frame, 0, FRAME_SIZE);
            return frame;
        }
    }
    return 0;
}

void pmm_free_frame(uintptr_t frame) {
    if (frame < pool_base) return;
    size_t index = (size_t)((frame - pool_base) / FRAME_SIZE);
    if (index >= pool_frames || !bitmap_test(index)) return;
    bitmap_clear(index);
    free_frames++;
}

/* Mark the frames overlapping [start, end) as permanently in use. Ranges below
 * the pool are ignored (they are never handed out). Used to protect GRUB module
 * images that land inside the pool from being reused as page frames. */
void pmm_reserve(uintptr_t start, uintptr_t end) {
    if (end <= pool_base || end <= start) return;
    if (start < pool_base) start = pool_base;
    size_t first = (size_t)((start - pool_base) / FRAME_SIZE);
    size_t last = (size_t)((end - pool_base + FRAME_SIZE - 1) / FRAME_SIZE);
    for (size_t i = first; i < last && i < pool_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_frames--;
        }
    }
}

/* Allocate a run of physically contiguous, zeroed frames spanning at least
 * `bytes`. Returns the base physical address (identity-mapped in every address
 * space) or 0. Used to stage a boot module into the pool so it stays readable
 * after a process CR3 is installed. */
uintptr_t pmm_alloc_contiguous(size_t bytes) {
    if (bytes == 0 || !pool_frames) return 0;
    size_t need = (bytes + FRAME_SIZE - 1) / FRAME_SIZE;
    for (size_t start = 0; start + need <= pool_frames;) {
        size_t run = 0;
        while (run < need && !bitmap_test(start + run)) run++;
        if (run == need) {
            for (size_t i = 0; i < need; i++) bitmap_set(start + i);
            free_frames -= need;
            uintptr_t base = pool_base + (uintptr_t)start * FRAME_SIZE;
            memset((void *)base, 0, need * FRAME_SIZE);
            return base;
        }
        start += run + 1; /* skip past the used frame that broke the run */
    }
    return 0;
}

void pmm_free_contiguous(uintptr_t base, size_t bytes) {
    if (base < pool_base || bytes == 0) return;
    size_t first = (size_t)((base - pool_base) / FRAME_SIZE);
    size_t frames = (bytes + FRAME_SIZE - 1) / FRAME_SIZE;
    for (size_t i = 0; i < frames && first + i < pool_frames; i++) {
        if (bitmap_test(first + i)) {
            bitmap_clear(first + i);
            free_frames++;
        }
    }
}

size_t pmm_free_count(void) { return free_frames; }
