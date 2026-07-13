#include "kernel.h"

static uint32_t detected_cpus;
static uint32_t online_cpus;

static void worker(void) {
    for (;;) {
        thread_yield();
    }
}

static char *skip_spaces(char *s) {
    while (*s == ' ') s++;
    return s;
}

static char *argument(char *line) {
    while (*line && *line != ' ') line++;
    if (*line) *line++ = 0;
    return skip_spaces(line);
}

/* Split a command string into an argv vector in place. Returns the count. */
static int tokenize(char *line, const char *argv[], int max) {
    int argc = 0;
    line = skip_spaces(line);
    while (*line && argc < max) {
        argv[argc++] = line;
        while (*line && *line != ' ') line++;
        if (*line) *line++ = 0;
        line = skip_spaces(line);
    }
    return argc;
}

/* Load and run a Multiboot module (a static Linux ELF) in ring 3. */
static void run_module(size_t index, const char *argv[], int argc) {
    const boot_module_t *mod = multiboot_module(index);
    if (!mod) {
        console_write("run: no such module\n");
        return;
    }
    const uint8_t *data = (const uint8_t *)mod->phys_start;
    size_t size = mod->phys_end - mod->phys_start;
    console_write("[sableos] exec module ");
    console_write_dec(index);
    console_write(": ");
    console_write(argc > 0 ? argv[0] : mod->string);
    console_putc('\n');
    int status = process_run(data, size, argc, argv);
    console_write("[sableos] exit status ");
    console_write_dec((uint64_t)(status & 0xff));
    console_putc('\n');
}

static void command_help(void) {
    console_write("help clear echo ls cat touch write rm uname cpuinfo ps "
                  "lsmod run reboot halt\n");
}

static void command_lsmod(void) {
    size_t count = multiboot_module_count();
    if (!count) {
        console_write("no modules loaded\n");
        return;
    }
    for (size_t i = 0; i < count; i++) {
        const boot_module_t *mod = multiboot_module(i);
        console_write_dec(i);
        console_write("  ");
        console_write(mod->string);
        console_write("  (");
        console_write_dec(mod->phys_end - mod->phys_start);
        console_write(" bytes)\n");
    }
}

static void command_run(char *arg) {
    if (!*arg) {
        console_write("usage: run INDEX [args...]\n");
        return;
    }
    const char *argv[16];
    char *rest = argument(arg);
    size_t index = 0;
    for (const char *p = arg; *p; p++) {
        if (*p < '0' || *p > '9') {
            console_write("run: INDEX must be numeric\n");
            return;
        }
        index = index * 10 + (size_t)(*p - '0');
    }
    int argc = tokenize(rest, argv, 16);
    if (argc == 0) {
        const boot_module_t *mod = multiboot_module(index);
        if (mod) {
            static char strbuf[128];
            size_t n = 0;
            for (const char *s = mod->string; *s && n < sizeof(strbuf) - 1; s++)
                strbuf[n++] = *s;
            strbuf[n] = 0;
            argc = tokenize(strbuf, argv, 16);
        }
    }
    run_module(index, argv, argc);
}

static void execute(char *line) {
    char *arg = argument(line);
    if (!*line) return;
    if (strcmp(line, "help") == 0) command_help();
    else if (strcmp(line, "clear") == 0) console_clear();
    else if (strcmp(line, "echo") == 0) {
        console_write(arg);
        console_putc('\n');
    } else if (strcmp(line, "ls") == 0) {
        for (size_t i = 0; i < RAMFS_MAX_FILES; i++) {
            const ramfs_file_t *file = ramfs_at(i);
            if (file) {
                console_write(file->name);
                console_putc(' ');
            }
        }
        console_putc('\n');
    } else if (strcmp(line, "cat") == 0) {
        const ramfs_file_t *file = ramfs_find(arg);
        if (file) {
            console_write(file->data);
            console_putc('\n');
        } else console_write("cat: file not found\n");
    } else if (strcmp(line, "touch") == 0) {
        if (ramfs_create(arg)) console_write("touch: invalid name, duplicate, or full filesystem\n");
    } else if (strcmp(line, "write") == 0) {
        char *data = argument(arg);
        if (ramfs_write(arg, data)) console_write("write: file not found or content too large\n");
    } else if (strcmp(line, "rm") == 0) {
        if (ramfs_remove(arg)) console_write("rm: file not found\n");
    } else if (strcmp(line, "uname") == 0) {
        console_write("SableOS 0.1 amd64\n");
    } else if (strcmp(line, "cpuinfo") == 0) {
        console_write("amd64 CPUs discovered through ACPI MADT: ");
        console_write_dec(detected_cpus);
        console_write(", online: ");
        console_write_dec(online_cpus);
        console_putc('\n');
    } else if (strcmp(line, "ps") == 0) {
        for (size_t i = 0; i < thread_count(); i++) {
            const char *name = thread_name(i);
            if (name) {
                console_write_dec(i);
                console_write("  ");
                console_write(name);
                console_putc('\n');
            }
        }
    } else if (strcmp(line, "lsmod") == 0) {
        command_lsmod();
    } else if (strcmp(line, "run") == 0) {
        command_run(arg);
    } else if (strcmp(line, "reboot") == 0) {
        outb(0x64, 0xfe);
    } else if (strcmp(line, "halt") == 0) {
        console_write("System halted.\n");
        __asm__ volatile("cli; hlt");
    } else console_write("command not found\n");
}

static void shell(void) {
    char line[160];
    for (;;) {
        console_write("sable:/$ ");
        size_t length = 0;
        for (;;) {
            char c = keyboard_read();
            if (c == '\n') {
                console_putc('\n');
                line[length] = 0;
                break;
            }
            if (c == '\b') {
                if (length) {
                    length--;
                    console_putc(c);
                }
            } else if (c >= ' ' && length < sizeof(line) - 1) {
                line[length++] = c;
                console_putc(c);
            }
        }
        execute(skip_spaces(line));
        thread_yield();
    }
}

/* Run every module GRUB loaded, in order, before entering the shell. With no
 * modules the boot path is identical to before, preserving existing behavior. */
static void autorun_modules(void) {
    size_t count = multiboot_module_count();
    for (size_t i = 0; i < count; i++) {
        const boot_module_t *mod = multiboot_module(i);
        static char strbuf[128];
        const char *argv[16];
        size_t n = 0;
        for (const char *s = mod->string; *s && n < sizeof(strbuf) - 1; s++)
            strbuf[n++] = s[0] == ',' ? ' ' : s[0];
        strbuf[n] = 0;
        int argc = tokenize(strbuf, argv, 16);
        run_module(i, argv, argc);
    }
}

void kernel_main(uint32_t multiboot_info) {
    console_init();
    console_write("SableOS 0.1 - freestanding amd64 kernel\n");
    console_write("Initializing ACPI, LAPIC, threads, ramfs, and terminal...\n");
    detected_cpus = cpu_discover();
    cpu_enable_lapic();
    online_cpus = cpu_start_aps();
    multiboot_parse(multiboot_info);
    ramfs_init();
    threads_init();
    thread_create("idle-worker", worker);
    keyboard_init();

    pmm_init(multiboot_ram_top());
    for (size_t i = 0; i < multiboot_module_count(); i++) {
        const boot_module_t *mod = multiboot_module(i);
        pmm_reserve(mod->phys_start, mod->phys_end);
    }
    vmm_init();
    gdt_init();
    idt_init();
    syscall_init();
    cpu_enable_sse();

    console_write("Ready. ");
    console_write_dec(detected_cpus);
    console_write(" CPU(s) discovered, ");
    console_write_dec(online_cpus);
    console_write(" online. Type 'help'.\n\n");

    autorun_modules();
    shell();
}

