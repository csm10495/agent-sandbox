#pragma once
#include "types.h"

#define SMP_MAX_CPUS  8

typedef struct {
    uint32_t apic_id;
    uint32_t uid;        /* ACPI processor UID */
    bool     online;
} cpu_info_t;

void     smp_init(void);
int      smp_cpu_count(void);
cpu_info_t *smp_cpu(int idx);
uint32_t smp_this_apic_id(void);
