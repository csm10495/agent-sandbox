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

extern const uint8_t _binary_ap_trampoline_bin_start[];
extern const uint8_t _binary_ap_trampoline_bin_end[];

static uint32_t lapic_address = 0xfee00000;
static uint8_t apic_ids[256];
static uint32_t apic_count;
static volatile uint32_t online_cpus = 1;
static uint8_t ap_stacks[255][4096] __attribute__((aligned(16)));

#define APIC_FLAGS_USABLE 3u
#define IPI_INIT_ASSERT 0x0000c500u
#define IPI_INIT_DEASSERT 0x00008500u
#define IPI_STARTUP_VECTOR_8 0x00004608u

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
    lapic_address = madt->lapic;
    apic_count = 0;
    const uint8_t *p = madt->entries;
    const uint8_t *end = (const uint8_t *)madt + madt->header.length;
    while (p + 2 <= end && p[1] >= 2 && p + p[1] <= end) {
        if (p[0] == 0 && p[1] >= 8) {
            uint32_t flags;
            memcpy(&flags, p + 4, sizeof(flags));
            if ((flags & APIC_FLAGS_USABLE) && apic_count < sizeof(apic_ids))
                apic_ids[apic_count++] = p[3];
        }
        p += p[1];
    }
    return apic_count ? apic_count : 1;
}

void cpu_enable_lapic(void) {
    uint32_t low;
    uint32_t high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(0x1b));
    low |= 1u << 11;
    __asm__ volatile("wrmsr" : : "a"(low), "d"(high), "c"(0x1b));
    volatile uint32_t *lapic = (volatile uint32_t *)(uintptr_t)lapic_address;
    lapic[0xf0 / 4] = lapic[0xf0 / 4] | 0x100;
}

static void delay(void) {
    for (volatile uint32_t i = 0; i < 100000; i++) __asm__ volatile("pause");
}

static void lapic_send(uint8_t apic_id, uint32_t command) {
    volatile uint32_t *lapic = (volatile uint32_t *)(uintptr_t)lapic_address;
    while (lapic[0x300 / 4] & (1u << 12)) __asm__ volatile("pause");
    lapic[0x310 / 4] = (uint32_t)apic_id << 24;
    lapic[0x300 / 4] = command;
    while (lapic[0x300 / 4] & (1u << 12)) __asm__ volatile("pause");
}

static void ap_entry(void) {
    cpu_enable_lapic();
    __atomic_add_fetch(&online_cpus, 1, __ATOMIC_SEQ_CST);
    for (;;) __asm__ volatile("cli; hlt");
}

uint32_t cpu_start_aps(void) {
    if (apic_count <= 1) return online_cpus;
    size_t trampoline_size =
        (size_t)(_binary_ap_trampoline_bin_end - _binary_ap_trampoline_bin_start);
    if (trampoline_size > 0xf00) return online_cpus;
    memcpy((void *)0x8000, _binary_ap_trampoline_bin_start, trampoline_size);
    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    *(volatile uint32_t *)0x8f00 = (uint32_t)cr3;
    uint32_t bsp_id = (*(volatile uint32_t *)(uintptr_t)(lapic_address + 0x20)) >> 24;
    uint32_t stack_index = 0;
    for (uint32_t i = 0; i < apic_count; i++) {
        if (apic_ids[i] == bsp_id) continue;
        *(volatile uint64_t *)0x8f08 =
            (uint64_t)(uintptr_t)&ap_stacks[stack_index++][sizeof(ap_stacks[0])];
        *(volatile uint64_t *)0x8f10 = (uint64_t)(uintptr_t)ap_entry;
        lapic_send(apic_ids[i], IPI_INIT_ASSERT);
        delay();
        lapic_send(apic_ids[i], IPI_INIT_DEASSERT);
        delay();
        lapic_send(apic_ids[i], IPI_STARTUP_VECTOR_8);
        delay();
        lapic_send(apic_ids[i], IPI_STARTUP_VECTOR_8);
        delay();
    }
    for (uint32_t wait = 0; wait < 1000000 && online_cpus < apic_count; wait++)
        __asm__ volatile("pause");
    return online_cpus;
}
