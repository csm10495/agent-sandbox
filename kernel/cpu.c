#include "kernel.h"

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem[6];
    uint8_t revision;
    uint32_t rsdt;
} __attribute__((packed)) rsdp_t;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem[6];
    char table[8];
    uint32_t oem_revision;
    uint32_t creator;
    uint32_t creator_revision;
} __attribute__((packed)) sdt_t;

typedef struct {
    sdt_t header;
    uint32_t lapic;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed)) madt_t;

static bool checksum_ok(const void *data, size_t length) {
    const uint8_t *p = data;
    uint8_t sum = 0;
    while (length--) sum = (uint8_t)(sum + *p++);
    return sum == 0;
}

uint32_t cpu_discover(void) {
    const rsdp_t *rsdp = NULL;
    for (uintptr_t addr = 0xe0000; addr < 0x100000; addr += 16) {
        const rsdp_t *candidate = (const rsdp_t *)addr;
        if (memcmp(candidate->signature, "RSD PTR ", 8) == 0 &&
            checksum_ok(candidate, 20)) {
            rsdp = candidate;
            break;
        }
    }
    if (!rsdp) return 1;
    const sdt_t *rsdt = (const sdt_t *)(uintptr_t)rsdp->rsdt;
    if (!checksum_ok(rsdt, rsdt->length)) return 1;
    size_t entries = (rsdt->length - sizeof(sdt_t)) / sizeof(uint32_t);
    const uint32_t *tables = (const uint32_t *)(rsdt + 1);
    const madt_t *madt = NULL;
    for (size_t i = 0; i < entries; i++) {
        const sdt_t *table = (const sdt_t *)(uintptr_t)tables[i];
        if (memcmp(table->signature, "APIC", 4) == 0) madt = (const madt_t *)table;
    }
    if (!madt || !checksum_ok(madt, madt->header.length)) return 1;
    uint32_t cpus = 0;
    const uint8_t *p = madt->entries;
    const uint8_t *end = (const uint8_t *)madt + madt->header.length;
    while (p + 2 <= end && p[1] >= 2 && p + p[1] <= end) {
        if (p[0] == 0 && p[1] >= 8) {
            uint32_t flags;
            memcpy(&flags, p + 4, sizeof(flags));
            if (flags & 3) cpus++;
        }
        p += p[1];
    }
    return cpus ? cpus : 1;
}

void cpu_enable_lapic(void) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1b));
    low |= 1u << 11;
    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(0x1b));
}
