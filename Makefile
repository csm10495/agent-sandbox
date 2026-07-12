# Makefile - MicrOS build system
# Targets: kernel.elf, os.iso, os.img, tests, screenshot

.PHONY: all clean iso img test screenshot

# ── Toolchain ───────────────────────────────────────────────────────────
AS        := nasm
CC        := gcc
LD        := ld
OBJCOPY   := objcopy

# ── Compiler flags (freestanding amd64) ─────────────────────────────────
CFLAGS    := -m64 -std=c11 \
             -ffreestanding -fno-builtin -nostdlib -nostdinc \
             -fno-stack-protector -fno-pic -fno-pie \
             -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
             -mcmodel=kernel \
             -O2 -Wall -Wextra -Wpedantic \
             -Ikernel

ASFLAGS   := -f elf64
LDFLAGS   := -T boot/linker.ld -nostdlib -z max-page-size=0x1000

# ── Sources ───────────────────────────────────────────────────────────────
KERNEL_C := \
    kernel/string.c   \
    kernel/vga.c      \
    kernel/pic.c      \
    kernel/pit.c      \
    kernel/idt.c      \
    kernel/keyboard.c \
    kernel/memory.c   \
    kernel/fs.c       \
    kernel/thread.c   \
    kernel/smp.c      \
    kernel/shell.c    \
    kernel/kernel.c

KERNEL_ASM := \
    kernel/entry.asm         \
    kernel/idt_stubs.asm     \
    kernel/thread_switch.asm

KERNEL_OBJS := \
    $(KERNEL_C:.c=.o) \
    $(KERNEL_ASM:.asm=.o)

# AP trampoline: separate flat binary included as data
AP_TRAMP_BIN := kernel/ap_trampoline.bin

# ── Stage 1 & 2 bootloader ───────────────────────────────────────────────
BOOT1_SRC := boot/stage1.asm
BOOT2_SRC := boot/stage2.asm
BOOT1_BIN := boot/stage1.bin
BOOT2_BIN := boot/stage2.bin

# ── Primary target ────────────────────────────────────────────────────────
all: os.iso os.img

# ── Compile C sources ─────────────────────────────────────────────────────
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Assemble ASM sources ──────────────────────────────────────────────────
kernel/entry.o: kernel/entry.asm
	$(AS) $(ASFLAGS) -o $@ $<

kernel/idt_stubs.o: kernel/idt_stubs.asm
	$(AS) $(ASFLAGS) -o $@ $<

kernel/thread_switch.o: kernel/thread_switch.asm
	$(AS) $(ASFLAGS) -o $@ $<

# ── AP trampoline (flat binary, 256 bytes) ────────────────────────────────
$(AP_TRAMP_BIN): kernel/ap_trampoline.asm
	$(AS) -f bin -o $@ $<

# Create a linkable object from the trampoline binary
kernel/ap_trampoline.o: $(AP_TRAMP_BIN)
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
	    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
	    --redefine-sym _binary_kernel_ap_trampoline_bin_start=ap_trampoline_start \
	    --redefine-sym _binary_kernel_ap_trampoline_bin_end=ap_trampoline_end \
	    --redefine-sym _binary_kernel_ap_trampoline_bin_size=ap_trampoline_size \
	    --add-section .note.GNU-stack=/dev/null \
	    $< $@

# ── Link kernel ELF ───────────────────────────────────────────────────────
KERNEL_ALL_OBJS := $(KERNEL_OBJS) kernel/ap_trampoline.o

kernel.elf: $(KERNEL_ALL_OBJS) boot/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_ALL_OBJS)

# ── Flat binary for disk-image boot ──────────────────────────────────────
kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

# ── Bootloader binaries ───────────────────────────────────────────────────
$(BOOT1_BIN): $(BOOT1_SRC)
	$(AS) -f bin -o $@ $<

$(BOOT2_BIN): $(BOOT2_SRC)
	$(AS) -f bin -o $@ $<

# ── Disk image (floppy-style, 1.44 MB) ───────────────────────────────────
# Layout: sector 0 = stage1, sectors 1-15 = stage2, sectors 16+ = kernel
DISK_SECTORS := 2880
os.img: $(BOOT1_BIN) $(BOOT2_BIN) kernel.bin
	dd if=/dev/zero  bs=512 count=$(DISK_SECTORS) of=$@ 2>/dev/null
	dd if=$(BOOT1_BIN)  bs=512 count=1  conv=notrunc of=$@ 2>/dev/null
	dd if=$(BOOT2_BIN)  bs=512 seek=1   conv=notrunc of=$@ 2>/dev/null
	dd if=kernel.bin    bs=512 seek=16  conv=notrunc of=$@ 2>/dev/null
	@echo "Disk image: $@"

# ── ISO (GRUB + Multiboot2) ───────────────────────────────────────────────
ISO_DIR := iso_build

os.iso: kernel.elf boot/grub.cfg
	mkdir -p $(ISO_DIR)/boot/grub
	cp kernel.elf     $(ISO_DIR)/boot/kernel.elf
	cp boot/grub.cfg  $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_DIR) 2>/dev/null
	@echo "ISO: $@"

# ── Unit tests ────────────────────────────────────────────────────────────
test: tests/unit/run_tests
	tests/unit/run_tests

tests/unit/run_tests: tests/unit/test_runner.c tests/unit/test_string.c \
                      tests/unit/test_fs.c tests/unit/test_vga.c \
                      kernel/string.c kernel/fs.c
	$(CC) -std=c11 -O1 -Wall -Wextra \
	    -DUNIT_TEST \
	    -Ikernel \
	    -o $@ \
	    tests/unit/test_runner.c \
	    tests/unit/test_string.c \
	    tests/unit/test_fs.c     \
	    tests/unit/test_vga.c    \
	    kernel/string.c          \
	    kernel/fs.c

# ── Boot functional test (QEMU) ───────────────────────────────────────────
boot-test: os.iso
	bash tests/boot/run_boot_test.sh

# ── Screenshot ────────────────────────────────────────────────────────────
screenshot: os.iso
	bash scripts/take_screenshot.sh

# ── Clean ─────────────────────────────────────────────────────────────────
clean:
	rm -f $(KERNEL_OBJS) kernel/ap_trampoline.o $(AP_TRAMP_BIN) \
	      kernel.elf kernel.bin \
	      $(BOOT1_BIN) $(BOOT2_BIN) \
	      os.iso os.img
	rm -rf $(ISO_DIR) tests/unit/run_tests

-include $(KERNEL_C:.c=.d)
