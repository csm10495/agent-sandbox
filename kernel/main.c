#include "kernel.h"

static uint32_t detected_cpus;

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

static void command_help(void) {
    console_write("help clear echo ls cat touch write rm uname cpuinfo ps reboot halt\n");
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
            } else if (c >= ' ' && length + 1 < sizeof(line)) {
                line[length++] = c;
                console_putc(c);
            }
        }
        execute(skip_spaces(line));
        thread_yield();
    }
}

void kernel_main(uint32_t multiboot_info) {
    (void)multiboot_info;
    console_init();
    console_write("SableOS 0.1 - freestanding amd64 kernel\n");
    console_write("Initializing ACPI, LAPIC, threads, ramfs, and terminal...\n");
    detected_cpus = cpu_discover();
    cpu_enable_lapic();
    ramfs_init();
    threads_init();
    thread_create("idle-worker", worker);
    keyboard_init();
    console_write("Ready. ");
    console_write_dec(detected_cpus);
    console_write(" CPU(s) discovered. Type 'help'.\n\n");
    shell();
}
