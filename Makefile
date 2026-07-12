CC := gcc
LD := ld
BUILD := build
ISO_ROOT := $(BUILD)/iso
CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -m64 -mno-red-zone -mgeneral-regs-only \
	-Wall -Wextra -Werror -O2 -Iinclude
LDFLAGS := -nostdlib -z max-page-size=0x1000 -T linker.ld
SOURCES := $(wildcard kernel/*.c)
OBJECTS := $(patsubst kernel/%.c,$(BUILD)/%.o,$(SOURCES)) $(BUILD)/boot.o $(BUILD)/context.o

.PHONY: all iso run test functional-test clean

all: iso

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/boot.o: arch/x86_64/boot.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/context.o: arch/x86_64/context.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

iso: $(BUILD)/kernel.elf grub/grub.cfg
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)/boot/grub
	cp $(BUILD)/kernel.elf $(ISO_ROOT)/boot/kernel.elf
	cp grub/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/sableos.iso $(ISO_ROOT)
	@echo "Created $(BUILD)/sableos.iso"

run: iso
	qemu-system-x86_64 -cdrom $(BUILD)/sableos.iso -smp 4 -m 128M -serial stdio

test: | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -fno-builtin -Iinclude tests/unit.c kernel/string.c kernel/ramfs.c -o $(BUILD)/unit-tests
	$(BUILD)/unit-tests

functional-test: iso
	python3 tests/boot_test.py $(BUILD)/sableos.iso

clean:
	rm -rf $(BUILD)
