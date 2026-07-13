CC := gcc
LD := ld
BUILD := build
ISO_ROOT := $(BUILD)/iso
CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -m64 -mno-red-zone -mgeneral-regs-only \
	-Wall -Wextra -Werror -O2 -Iinclude
LDFLAGS := -nostdlib -z max-page-size=0x1000 -z noexecstack -T linker.ld
# Freestanding static Linux ELF programs executed by the ring-3 loader.
USER_CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone \
	-mgeneral-regs-only -Wall -Wextra -Werror -O2
SOURCES := $(wildcard kernel/*.c)
OBJECTS := $(patsubst kernel/%.c,$(BUILD)/%.o,$(SOURCES)) $(BUILD)/boot.o $(BUILD)/context.o \
	$(BUILD)/user_entry.o $(BUILD)/ap_trampoline_blob.o

.PHONY: all iso run test functional-test user-iso user-test linux-abi-test clean

all: iso

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: kernel/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/boot.o: arch/x86_64/boot.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/context.o: arch/x86_64/context.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_entry.o: arch/x86_64/user_entry.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ap_trampoline.o: arch/x86_64/ap_trampoline.S | $(BUILD)
	$(CC) -m64 -c $< -o $@


$(BUILD)/ap_trampoline.elf: $(BUILD)/ap_trampoline.o arch/x86_64/ap_linker.ld
	$(LD) -nostdlib -T arch/x86_64/ap_linker.ld $< -o $@

$(BUILD)/ap_trampoline.bin: $(BUILD)/ap_trampoline.elf
	objcopy -O binary $< $@

$(BUILD)/ap_trampoline_blob.o: $(BUILD)/ap_trampoline.bin
	cd $(BUILD) && $(LD) -r -b binary ap_trampoline.bin -o ap_trampoline_blob.o

$(BUILD)/kernel.elf: $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

# ---- Freestanding user program (a real static Linux ELF) -----------------
$(BUILD)/user_start.o: user/start.S | $(BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD)/user_hello.o: user/hello.c | $(BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD)/hello.elf: $(BUILD)/user_start.o $(BUILD)/user_hello.o user/linker.ld
	$(LD) -nostdlib -no-pie -z noexecstack -T user/linker.ld \
		$(BUILD)/user_start.o $(BUILD)/user_hello.o -o $@

iso: $(BUILD)/kernel.elf grub/grub.cfg
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)/boot/grub
	cp $(BUILD)/kernel.elf $(ISO_ROOT)/boot/kernel.elf
	cp grub/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/sableos.iso $(ISO_ROOT)
	@echo "Created $(BUILD)/sableos.iso"

# ISO that boots the kernel and auto-runs the ring-3 hello program as a module.
user-iso: $(BUILD)/kernel.elf $(BUILD)/hello.elf grub/grub_user.cfg
	rm -rf $(BUILD)/user-iso
	mkdir -p $(BUILD)/user-iso/boot/grub
	cp $(BUILD)/kernel.elf $(BUILD)/user-iso/boot/kernel.elf
	cp $(BUILD)/hello.elf $(BUILD)/user-iso/boot/hello.elf
	cp grub/grub_user.cfg $(BUILD)/user-iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/sableos-user.iso $(BUILD)/user-iso
	@echo "Created $(BUILD)/sableos-user.iso"

run: iso
	qemu-system-x86_64 -cdrom $(BUILD)/sableos.iso -smp 4 -m 128M -serial stdio

test: $(BUILD)/hello.elf | $(BUILD)
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -fno-builtin -Iinclude tests/unit.c kernel/string.c kernel/ramfs.c -o $(BUILD)/unit-tests
	$(BUILD)/unit-tests
	$(CC) -std=c11 -Wall -Wextra -Werror -O2 -fno-builtin -Iinclude tests/abi_unit.c kernel/string.c kernel/elf.c -o $(BUILD)/abi-unit-tests
	$(BUILD)/abi-unit-tests

functional-test: iso
	python3 tests/boot_test.py $(BUILD)/sableos.iso

# Milestone 1: execute a from-source static Linux hello-world ELF in ring 3.
user-test: user-iso
	python3 tests/user_boot_test.py $(BUILD)/sableos-user.iso

# Milestone 2+ demonstration using host-provided static binaries (BusyBox,
# glibc-static). Generated locally, never committed; skips if unavailable.
linux-abi-test: $(BUILD)/kernel.elf grub/grub_user.cfg
	python3 tests/linux_abi_test.py

clean:
	rm -rf $(BUILD)
