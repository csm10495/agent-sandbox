/* kernel/smp.c - SMP CPU discovery via ACPI MADT + BSP LAPIC init */
#include "smp.h"
#include "vga.h"
#include "string.h"
#include "pit.h"

/* ------------------------------------------------------------------ */
/* ACPI structures                                                      */
/* ------------------------------------------------------------------ */
typedef struct __attribute__((packed)) {
    char     sig[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_header_t;

typedef struct __attribute__((packed)) {
    char     sig[8];     /* "RSD PTR " */
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_addr;
    /* ACPI 2+ */
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
} rsdp_t;

typedef struct __attribute__((packed)) {
    acpi_header_t header;
    uint32_t lapic_phys;
    uint32_t flags;
    /* followed by variable-length records */
} madt_t;

/* MADT entry header */
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
} madt_entry_t;

/* MADT Type 0: Processor Local APIC */
typedef struct __attribute__((packed)) {
    madt_entry_t hdr;
    uint8_t  uid;
    uint8_t  apic_id;
    uint32_t flags;   /* bit 0 = enabled */
} madt_lapic_entry_t;

/* ------------------------------------------------------------------ */
/* LAPIC MMIO access                                                    */
/* ------------------------------------------------------------------ */
#define LAPIC_ID_REG     0x020
#define LAPIC_SVR        0x0F0
#define LAPIC_ICR_LO     0x300
#define LAPIC_ICR_HI     0x310

static volatile uint32_t *lapic_base = NULL;

static inline uint32_t lapic_read(uint32_t off) {
    return *(volatile uint32_t *)((uint8_t *)lapic_base + off);
}
static inline void lapic_write(uint32_t off, uint32_t val) {
    *(volatile uint32_t *)((uint8_t *)lapic_base + off) = val;
}

static void lapic_enable(void) {
    /* Spurious Vector Register: enable APIC (bit 8), spurious = 0xFF */
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | 0x100 | 0xFF);
}

uint32_t smp_this_apic_id(void) {
    if (!lapic_base) return 0;
    return (lapic_read(LAPIC_ID_REG) >> 24) & 0xFF;
}

/* ------------------------------------------------------------------ */
/* AP startup (INIT + SIPI)                                            */
/* ------------------------------------------------------------------ */
#define AP_TRAMPOLINE_PHYS  0x8000

/* Embedded trampoline binary (assembled separately) */
extern char ap_trampoline_start[], ap_trampoline_end[];

static void lapic_send_init(uint8_t apic_id) {
    lapic_write(LAPIC_ICR_HI, (uint32_t)apic_id << 24);
    lapic_write(LAPIC_ICR_LO, 0x4500);  /* INIT, assert, physical */
    pit_sleep_ms(10);
    lapic_write(LAPIC_ICR_LO, 0x8500);  /* INIT, deassert */
    pit_sleep_ms(10);
}

static void lapic_send_sipi(uint8_t apic_id, uint8_t vector) {
    lapic_write(LAPIC_ICR_HI, (uint32_t)apic_id << 24);
    lapic_write(LAPIC_ICR_LO, 0x4600 | vector);  /* SIPI */
}

/* ------------------------------------------------------------------ */
static cpu_info_t cpus[SMP_MAX_CPUS];
static int        cpu_count = 0;

/* Checksum an ACPI table */
static bool acpi_checksum(const void *ptr, uint32_t length) {
    const uint8_t *p = ptr;
    uint8_t sum = 0;
    while (length--) sum += *p++;
    return sum == 0;
}

/* Search for RSDP in a memory range */
static rsdp_t *find_rsdp_in(uintptr_t start, uintptr_t end) {
    for (uintptr_t p = start; p < end; p += 16) {
        rsdp_t *r = (rsdp_t *)p;
        if (memcmp(r->sig, "RSD PTR ", 8) == 0 &&
            acpi_checksum(r, 20))
            return r;
    }
    return NULL;
}

static rsdp_t *find_rsdp(void) {
    /* Read EBDA segment from BDA at 0x40E (BIOS Data Area).
     * Suppress the -Warray-bounds warning: this is valid kernel physical-mode access. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
    uint16_t ebda_seg = *(const volatile uint16_t *)0x40EUL;
#pragma GCC diagnostic pop
    uintptr_t ebda = (uintptr_t)ebda_seg << 4;
    if (ebda >= 0x80000 && ebda < 0xA0000) {
        rsdp_t *r = find_rsdp_in(ebda, ebda + 1024);
        if (r) return r;
    }
    /* BIOS ROM area 0xE0000-0xFFFFF */
    return find_rsdp_in(0xE0000, 0x100000);
}

static void parse_madt(madt_t *madt) {
    lapic_base = (volatile uint32_t *)(uintptr_t)madt->lapic_phys;
    lapic_enable();

    uint8_t *ptr = (uint8_t *)(madt + 1);
    uint8_t *end = (uint8_t *)madt + madt->header.length;

    while (ptr < end) {
        madt_entry_t *e = (madt_entry_t *)ptr;
        if (e->type == 0) {  /* Local APIC */
            madt_lapic_entry_t *la = (madt_lapic_entry_t *)ptr;
            if ((la->flags & 1) && cpu_count < SMP_MAX_CPUS) {
                cpus[cpu_count].apic_id = la->apic_id;
                cpus[cpu_count].uid     = la->uid;
                cpus[cpu_count].online  = true;
                cpu_count++;
            }
        }
        ptr += e->length;
    }
}

static void start_aps(void) {
    if (!lapic_base) return;
    uint32_t bsp_id = smp_this_apic_id();

    /* Copy AP trampoline to low memory */
    size_t tlen = (size_t)(ap_trampoline_end - ap_trampoline_start);
    memcpy((void *)AP_TRAMPOLINE_PHYS, ap_trampoline_start, tlen);

    /* Fill in BSP GDT descriptor at offset 0xF0 in trampoline page */
    typedef struct __attribute__((packed)) { uint16_t lim; uint64_t base; } gdt_desc_t;
    gdt_desc_t tmp_gdt;
    __asm__ volatile ("sgdt %0" : "=m"(tmp_gdt));
    memcpy((uint8_t *)AP_TRAMPOLINE_PHYS + 0xF0, &tmp_gdt, sizeof(tmp_gdt));

    /* Store PML4 at offset 0xF8 */
    uint64_t cr3 = read_cr3();
    *(uint32_t *)((uint8_t *)AP_TRAMPOLINE_PHYS + 0xF8) = (uint32_t)cr3;

    for (int i = 0; i < cpu_count; i++) {
        if (cpus[i].apic_id == bsp_id) continue;  /* skip BSP */

        /* Fixed AP stacks */
        static uint8_t ap_stacks[SMP_MAX_CPUS][4096] __attribute__((aligned(4096)));
        uint64_t sp = (uint64_t)&ap_stacks[i][4096];

        *(uint64_t *)((uint8_t *)AP_TRAMPOLINE_PHYS + 0xE0) = sp;
        *(uint8_t  *)((uint8_t *)AP_TRAMPOLINE_PHYS + 0xE8) = 0;

        lapic_send_init(cpus[i].apic_id);
        lapic_send_sipi(cpus[i].apic_id, 0x08);
        pit_sleep_ms(1);
        lapic_send_sipi(cpus[i].apic_id, 0x08);

        /* Wait up to 50ms for AP ready flag */
        uint64_t timeout = pit_ticks() + 5;
        while (pit_ticks() < timeout &&
               *(volatile uint8_t *)((uint8_t *)AP_TRAMPOLINE_PHYS + 0xE8) == 0)
            cpu_pause();
    }
}

/* ------------------------------------------------------------------ */
void smp_init(void) {
    rsdp_t *rsdp = find_rsdp();
    if (!rsdp) {
        /* ACPI not found — assume single CPU */
        cpus[0].apic_id = 0;
        cpus[0].uid     = 0;
        cpus[0].online  = true;
        cpu_count = 1;
        vga_printf("[SMP] ACPI not found; assuming 1 CPU\n");
        return;
    }

    /* Parse RSDT */
    acpi_header_t *rsdt = (acpi_header_t *)(uintptr_t)rsdp->rsdt_addr;
    uint32_t *entries = (uint32_t *)(rsdt + 1);
    int n = ((int)rsdt->length - (int)sizeof(acpi_header_t)) / 4;

    for (int i = 0; i < n; i++) {
        acpi_header_t *tbl = (acpi_header_t *)(uintptr_t)entries[i];
        if (memcmp(tbl->sig, "APIC", 4) == 0) {
            parse_madt((madt_t *)tbl);
            break;
        }
    }

    if (cpu_count == 0) {
        cpus[0].apic_id = 0; cpus[0].uid = 0; cpus[0].online = true;
        cpu_count = 1;
    }

    vga_printf("[SMP] Found %d CPU(s) via ACPI MADT\n", cpu_count);

    if (cpu_count > 1) {
        start_aps();
        vga_printf("[SMP] AP startup initiated\n");
    }
}

int        smp_cpu_count(void)    { return cpu_count; }
cpu_info_t *smp_cpu(int idx)      { return (idx >= 0 && idx < cpu_count) ? &cpus[idx] : NULL; }
