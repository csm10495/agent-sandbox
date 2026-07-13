/*
 * Host unit tests for the Linux-ABI ELF loader's header validator.
 *
 * elf_header_valid() is the real production function from kernel/elf.c; it is
 * pure (no paging state), so it can be exercised directly on the host. The
 * loader's mapping half references the VMM, so a stub is provided to satisfy
 * the linker; it is never called by these tests.
 */
#include "kernel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vmm_map_user_alloc(address_space_t *space, uintptr_t vaddr, size_t length,
                       bool writable, bool executable) {
    (void)space; (void)vaddr; (void)length; (void)writable; (void)executable;
    return -1;
}

static int failures;

static void check(const char *name, int cond) {
    if (cond) {
        printf("ok   - %s\n", name);
    } else {
        printf("FAIL - %s\n", name);
        failures++;
    }
}

static void put16(uint8_t *b, size_t off, uint16_t v) {
    b[off] = (uint8_t)v;
    b[off + 1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *b, size_t off, uint32_t v) {
    for (int i = 0; i < 4; i++) b[off + i] = (uint8_t)(v >> (8 * i));
}

/* Build a minimal but well-formed 64-bit x86_64 ELF header. */
static void make_valid(uint8_t hdr[64]) {
    memset(hdr, 0, 64);
    hdr[0] = 0x7f; hdr[1] = 'E'; hdr[2] = 'L'; hdr[3] = 'F';
    hdr[4] = 2;                 /* ELFCLASS64   */
    hdr[5] = 1;                 /* ELFDATA2LSB  */
    hdr[6] = 1;                 /* EV_CURRENT   */
    put16(hdr, 16, 2);          /* e_type = ET_EXEC   */
    put16(hdr, 18, 62);         /* e_machine = x86_64 */
    put32(hdr, 20, 1);          /* e_version          */
    put16(hdr, 54, 56);         /* e_phentsize        */
    put16(hdr, 56, 0);          /* e_phnum            */
}

static void test_synthetic(void) {
    uint8_t hdr[64];

    make_valid(hdr);
    check("well-formed ET_EXEC header accepted", elf_header_valid(hdr, sizeof hdr));

    make_valid(hdr);
    put16(hdr, 16, 3);          /* ET_DYN is also loadable (PIE) */
    check("ET_DYN header accepted", elf_header_valid(hdr, sizeof hdr));

    make_valid(hdr);
    hdr[0] = 0;                 /* corrupt magic */
    check("bad magic rejected", !elf_header_valid(hdr, sizeof hdr));

    make_valid(hdr);
    hdr[4] = 1;                 /* ELFCLASS32 */
    check("32-bit class rejected", !elf_header_valid(hdr, sizeof hdr));

    make_valid(hdr);
    hdr[5] = 2;                 /* big-endian */
    check("big-endian rejected", !elf_header_valid(hdr, sizeof hdr));

    make_valid(hdr);
    put16(hdr, 18, 0x28);       /* EM_ARM */
    check("non-x86_64 machine rejected", !elf_header_valid(hdr, sizeof hdr));

    make_valid(hdr);
    put16(hdr, 16, 1);          /* ET_REL is not runnable */
    check("relocatable object rejected", !elf_header_valid(hdr, sizeof hdr));

    make_valid(hdr);
    put16(hdr, 54, 8);          /* implausibly small e_phentsize */
    check("tiny phentsize rejected", !elf_header_valid(hdr, sizeof hdr));

    make_valid(hdr);
    put16(hdr, 56, 4);          /* phnum*phentsize runs past the blob */
    check("phdr table past end rejected", !elf_header_valid(hdr, sizeof hdr));

    make_valid(hdr);
    check("truncated header rejected", !elf_header_valid(hdr, 20));
}

/* If the from-source user program has been built, it must pass validation:
 * the exact same bytes are loaded as a multiboot module and run in ring 3. */
static void test_real_artifact(void) {
    FILE *f = fopen("build/hello.elf", "rb");
    if (!f) {
        printf("skip - build/hello.elf not present\n");
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        check("build/hello.elf non-empty", 0);
        return;
    }
    uint8_t *buf = malloc((size_t)size);
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    check("build/hello.elf fully read", got == (size_t)size);
    check("build/hello.elf passes validation",
          elf_header_valid(buf, (size_t)size));
    free(buf);
}

int main(void) {
    test_synthetic();
    test_real_artifact();
    if (failures) {
        printf("%d ABI unit test(s) failed\n", failures);
        return 1;
    }
    printf("All ABI unit tests passed\n");
    return 0;
}
