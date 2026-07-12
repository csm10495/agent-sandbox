#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "fs.h"
#include "string.h"
#include "memory.h"
#include "thread.h"
#include "smp.h"
#include "pit.h"

#define LINE_MAX  256
#define ARGV_MAX   16
#define HIST_SIZE   8

static fs_node_t *cwd;           /* current working directory */

static char history[HIST_SIZE][LINE_MAX];
static int  hist_head = 0;
static int  hist_count = 0;

/* ------------------------------------------------------------------ */
/* Line editing with history                                            */
/* ------------------------------------------------------------------ */
static void readline(char *buf, int maxlen) {
    int pos = 0, hist_idx = -1;
    buf[0] = '\0';

    for (;;) {
        int c = keyboard_getchar();

        if (c == '\n' || c == '\r') {
            buf[pos] = '\0';
            vga_putchar('\n');
            /* Save to history if non-empty */
            if (pos > 0) {
                strncpy(history[hist_head], buf, LINE_MAX - 1);
                hist_head = (hist_head + 1) % HIST_SIZE;
                if (hist_count < HIST_SIZE) hist_count++;
            }
            return;
        }
        if (c == '\b' || c == 127) {
            if (pos > 0) { pos--; vga_putchar('\b'); }
            continue;
        }
        if (c == '\x03') {        /* Ctrl+C */
            buf[0] = '\0';
            vga_puts("^C\n");
            return;
        }
        if (c >= 0x20 && pos < maxlen - 1) {
            buf[pos++] = (char)c;
            vga_putchar((char)c);
        }
        (void)hist_idx;
    }
}

/* ------------------------------------------------------------------ */
/* Tokenizer                                                            */
/* ------------------------------------------------------------------ */
static int tokenize(char *line, char **argv, int maxargv) {
    int argc = 0;
    char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (argc >= maxargv - 1) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

/* ------------------------------------------------------------------ */
/* Build a full path string from cwd + arg                             */
/* ------------------------------------------------------------------ */
static fs_node_t *resolve_path(const char *arg) {
    if (!arg || !*arg) return cwd;
    return fs_lookup(arg[0] == '/' ? fs_root() : cwd, arg);
}

/* ------------------------------------------------------------------ */
/* Commands                                                             */
/* ------------------------------------------------------------------ */
static void cmd_help(void) {
    vga_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("Available commands:\n");
    vga_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  help              This help text\n");
    vga_puts("  clear             Clear screen\n");
    vga_puts("  pwd               Print working directory\n");
    vga_puts("  ls [path]         List directory\n");
    vga_puts("  cd <path>         Change directory\n");
    vga_puts("  cat <file>        Print file contents\n");
    vga_puts("  mkdir <dir>       Create directory\n");
    vga_puts("  touch <file>      Create empty file\n");
    vga_puts("  rm <path>         Remove file or empty dir\n");
    vga_puts("  echo [text]       Print text\n");
    vga_puts("  write <f> <text>  Write text to file\n");
    vga_puts("  ps                List kernel threads\n");
    vga_puts("  info              System information\n");
    vga_puts("  meminfo           Memory statistics\n");
    vga_puts("  reboot            Reboot the system\n");
    vga_puts("  halt              Halt the CPU\n");
}

static void cmd_ls(const char *arg) {
    fs_node_t *dir = arg ? resolve_path(arg) : cwd;
    if (!dir) { vga_printf("ls: %s: not found\n", arg); return; }
    if (dir->type != FS_DIR) { vga_printf("ls: not a directory\n"); return; }

    for (fs_node_t *c = dir->child; c; c = c->next) {
        if (c->type == FS_DIR) {
            vga_setcolor(VGA_LIGHT_BLUE, VGA_BLACK);
            vga_printf("  %s/\n", c->name);
            vga_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
        } else {
            vga_printf("  %s (%lu B)\n", c->name, (uint64_t)c->size);
        }
    }
}

static void cmd_cd(const char *arg) {
    if (!arg || strcmp(arg, "/") == 0) { cwd = fs_root(); return; }
    if (strcmp(arg, "..") == 0)        { cwd = cwd->parent; return; }
    fs_node_t *n = resolve_path(arg);
    if (!n)                  { vga_printf("cd: %s: not found\n", arg); return; }
    if (n->type != FS_DIR)   { vga_printf("cd: %s: not a directory\n", arg); return; }
    cwd = n;
}

static void cmd_pwd(void) {
    /* Walk up to root, build path */
    const char *parts[32];
    int depth = 0;
    fs_node_t *n = cwd;
    while (n != fs_root() && depth < 31) {
        parts[depth++] = n->name;
        n = n->parent;
    }
    if (depth == 0) { vga_puts("/\n"); return; }
    for (int i = depth - 1; i >= 0; i--)
        vga_printf("/%s", parts[i]);
    vga_putchar('\n');
}

static void cmd_cat(const char *arg) {
    if (!arg) { vga_puts("Usage: cat <file>\n"); return; }
    fs_node_t *n = resolve_path(arg);
    if (!n) { vga_printf("cat: %s: not found\n", arg); return; }
    if (n->type != FS_FILE) { vga_printf("cat: %s: is a directory\n", arg); return; }

    static char rbuf[FS_MAX_FILE_DATA + 1];
    int len = fs_read(n, rbuf, FS_MAX_FILE_DATA);
    if (len < 0) { vga_puts("cat: read error\n"); return; }
    rbuf[len] = '\0';
    vga_puts(rbuf);
    if (len > 0 && rbuf[len-1] != '\n') vga_putchar('\n');
}

static void cmd_mkdir(const char *arg) {
    if (!arg) { vga_puts("Usage: mkdir <dir>\n"); return; }
    if (!fs_mkdir(cwd, arg))
        vga_printf("mkdir: cannot create '%s'\n", arg);
}

static void cmd_touch(const char *arg) {
    if (!arg) { vga_puts("Usage: touch <file>\n"); return; }
    if (!fs_create(cwd, arg))
        vga_printf("touch: cannot create '%s'\n", arg);
}

static void cmd_rm(const char *arg) {
    if (!arg) { vga_puts("Usage: rm <path>\n"); return; }
    fs_node_t *n = resolve_path(arg);
    if (!n) { vga_printf("rm: %s: not found\n", arg); return; }
    if (fs_unlink(n) < 0)
        vga_printf("rm: cannot remove '%s' (dir not empty?)\n", arg);
}

static void cmd_echo(int argc, char **argv) {
    /* Check for redirection: echo text > file */
    int redir = -1;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], ">") == 0) { redir = i; break; }

    /* Collect text tokens */
    char out_line[LINE_MAX] = {0};
    int limit = (redir >= 0) ? redir : argc;
    for (int i = 1; i < limit; i++) {
        if (i > 1) strcat(out_line, " ");
        strncat(out_line, argv[i], LINE_MAX - strlen(out_line) - 1);
    }

    if (redir >= 0 && argv[redir + 1]) {
        fs_node_t *f = resolve_path(argv[redir + 1]);
        if (!f) f = fs_create(cwd, argv[redir + 1]);
        if (!f) { vga_printf("echo: cannot open '%s'\n", argv[redir+1]); return; }
        strcat(out_line, "\n");
        fs_write(f, out_line, (uint32_t)strlen(out_line));
    } else {
        vga_puts(out_line);
        vga_putchar('\n');
    }
}

static void cmd_write(int argc, char **argv) {
    if (argc < 3) { vga_puts("Usage: write <file> <text...>\n"); return; }
    fs_node_t *f = resolve_path(argv[1]);
    if (!f) f = fs_create(cwd, argv[1]);
    if (!f) { vga_printf("write: cannot open '%s'\n", argv[1]); return; }
    char buf[LINE_MAX] = {0};
    for (int i = 2; i < argc; i++) {
        if (i > 2) strcat(buf, " ");
        strncat(buf, argv[i], LINE_MAX - strlen(buf) - 1);
    }
    strcat(buf, "\n");
    fs_write(f, buf, (uint32_t)strlen(buf));
}

static void cmd_info(void) {
    vga_setcolor(VGA_YELLOW, VGA_BLACK);
    vga_puts("MicrOS v0.1 - in-repo custom bootloader, amd64 kernel\n");
    vga_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    vga_printf("  CPUs online : %d\n", smp_cpu_count());
    for (int i = 0; i < smp_cpu_count(); i++) {
        cpu_info_t *c = smp_cpu(i);
        if (c) vga_printf("    CPU %d  APIC ID %d\n", i, c->apic_id);
    }
    vga_printf("  Uptime ticks: %lu (100 Hz)\n", pit_ticks());
}

static void cmd_meminfo(void) {
    uint64_t free_p  = pmm_free_pages();
    uint64_t total_p = pmm_total_pages();
    vga_printf("  Pages total : %lu (%lu KB)\n", total_p, total_p * 4);
    vga_printf("  Pages free  : %lu (%lu KB)\n", free_p,  free_p  * 4);
    vga_printf("  Pages used  : %lu (%lu KB)\n", total_p - free_p,
               (total_p - free_p) * 4);
}

/* ------------------------------------------------------------------ */
static void cmd_reboot(void) {
    /* Keyboard controller pulse reset line */
    for (;;) {
        uint8_t val;
        do { val = inb(0x64); } while (val & 2);
        outb(0x64, 0xFE);
    }
}

static void cmd_halt(void) {
    vga_puts("Halting.\n");
    cpu_cli();
    for (;;) cpu_halt();
}

/* ------------------------------------------------------------------ */
static void dispatch(int argc, char **argv) {
    if (!argc) return;
    const char *cmd = argv[0];

    if      (strcmp(cmd, "help")    == 0) cmd_help();
    else if (strcmp(cmd, "clear")   == 0) vga_clear();
    else if (strcmp(cmd, "pwd")     == 0) cmd_pwd();
    else if (strcmp(cmd, "ls")      == 0) cmd_ls(argv[1]);
    else if (strcmp(cmd, "cd")      == 0) cmd_cd(argv[1]);
    else if (strcmp(cmd, "cat")     == 0) cmd_cat(argv[1]);
    else if (strcmp(cmd, "mkdir")   == 0) cmd_mkdir(argv[1]);
    else if (strcmp(cmd, "touch")   == 0) cmd_touch(argv[1]);
    else if (strcmp(cmd, "rm")      == 0) cmd_rm(argv[1]);
    else if (strcmp(cmd, "echo")    == 0) cmd_echo(argc, argv);
    else if (strcmp(cmd, "write")   == 0) cmd_write(argc, argv);
    else if (strcmp(cmd, "ps")      == 0) thread_list();
    else if (strcmp(cmd, "info")    == 0) cmd_info();
    else if (strcmp(cmd, "meminfo") == 0) cmd_meminfo();
    else if (strcmp(cmd, "reboot")  == 0) cmd_reboot();
    else if (strcmp(cmd, "halt")    == 0) cmd_halt();
    else {
        vga_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        vga_printf("Unknown command: %s  (type 'help')\n", cmd);
        vga_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    }
}

void shell_run(fs_node_t *root) {
    cwd = root ? root : fs_root();

    vga_setcolor(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n  MicrOS v0.1  -  type 'help' for commands\n\n");
    vga_setcolor(VGA_LIGHT_GREY, VGA_BLACK);

    static char line[LINE_MAX];
    static char *argv[ARGV_MAX];

    for (;;) {
        vga_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("root@micros");
        vga_setcolor(VGA_WHITE, VGA_BLACK);
        vga_puts(":/ $ ");
        vga_setcolor(VGA_LIGHT_GREY, VGA_BLACK);

        readline(line, LINE_MAX);

        int argc = tokenize(line, argv, ARGV_MAX);
        if (argc > 0) dispatch(argc, argv);

        thread_yield();
    }
}
