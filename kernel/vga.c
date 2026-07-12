#include "vga.h"
#include "string.h"

#define VGA_BASE  0xB8000UL

static uint16_t *vga_buf  = (uint16_t *)VGA_BASE;
static int       cur_row  = 0;
static int       cur_col  = 0;
static uint8_t   cur_attr = 0x07; /* light grey on black */

static inline uint16_t vga_entry(char c, uint8_t attr) {
    return (uint16_t)c | ((uint16_t)attr << 8);
}

/* Hardware cursor via VGA index registers */
static void vga_hw_cursor(int row, int col) {
    uint16_t pos = (uint16_t)(row * VGA_COLS + col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
}

void vga_init(void) {
    cur_attr = 0x07;
    vga_clear();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_ROWS * VGA_COLS; i++)
        vga_buf[i] = vga_entry(' ', cur_attr);
    cur_row = 0; cur_col = 0;
    vga_hw_cursor(0, 0);
}

void vga_setcolor(vga_color_t fg, vga_color_t bg) {
    cur_attr = (uint8_t)((bg << 4) | (fg & 0x0F));
}

void vga_set_cursor(int row, int col) {
    cur_row = row; cur_col = col;
    vga_hw_cursor(row, col);
}

int vga_get_row(void) { return cur_row; }
int vga_get_col(void) { return cur_col; }

static void vga_scroll(void) {
    for (int r = 0; r < VGA_ROWS - 1; r++)
        for (int c = 0; c < VGA_COLS; c++)
            vga_buf[r * VGA_COLS + c] = vga_buf[(r+1) * VGA_COLS + c];
    for (int c = 0; c < VGA_COLS; c++)
        vga_buf[(VGA_ROWS - 1) * VGA_COLS + c] = vga_entry(' ', cur_attr);
    cur_row = VGA_ROWS - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        cur_col = 0;
        if (++cur_row >= VGA_ROWS) vga_scroll();
    } else if (c == '\r') {
        cur_col = 0;
    } else if (c == '\t') {
        int next = (cur_col + 8) & ~7;
        while (cur_col < next) vga_putchar(' ');
        return;
    } else if (c == '\b') {
        if (cur_col > 0) {
            cur_col--;
            vga_buf[cur_row * VGA_COLS + cur_col] = vga_entry(' ', cur_attr);
        }
    } else {
        vga_buf[cur_row * VGA_COLS + cur_col] = vga_entry(c, cur_attr);
        if (++cur_col >= VGA_COLS) {
            cur_col = 0;
            if (++cur_row >= VGA_ROWS) vga_scroll();
        }
    }
    vga_hw_cursor(cur_row, cur_col);
}

void vga_puts(const char *s) {
    while (*s) vga_putchar(*s++);
}

/* Minimal printf: supports %s %d %u %x %X %c %% %p */
void vga_printf(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    char buf[32];

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { vga_putchar(*p); continue; }
        p++;
        int pad = 0;
        /* Parse optional width */
        while (*p >= '0' && *p <= '9') pad = pad * 10 + (*p++ - '0');

        switch (*p) {
        case 'd': {
            int64_t v = (int64_t)__builtin_va_arg(ap, int);
            itoa(v, buf, 10);
            vga_puts(buf); break; }
        case 'u': {
            uint64_t v = (uint64_t)__builtin_va_arg(ap, unsigned);
            uitoa(v, buf, 10);
            vga_puts(buf); break; }
        case 'x': {
            uint64_t v = (uint64_t)__builtin_va_arg(ap, unsigned);
            uitoa(v, buf, 16);
            if (pad) {
                int len = (int)strlen(buf);
                for (int i = len; i < pad; i++) vga_putchar('0');
            }
            vga_puts(buf); break; }
        case 'X': {
            uint64_t v = (uint64_t)__builtin_va_arg(ap, unsigned);
            uitoa(v, buf, 16);
            /* uppercase */
            for (char *q = buf; *q; q++) if (*q >= 'a' && *q <= 'f') *q -= 32;
            if (pad) {
                int len = (int)strlen(buf);
                for (int i = len; i < pad; i++) vga_putchar('0');
            }
            vga_puts(buf); break; }
        case 'p': {
            uint64_t v = (uint64_t)(uintptr_t)__builtin_va_arg(ap, void*);
            vga_puts("0x"); uitoa(v, buf, 16); vga_puts(buf); break; }
        case 's': {
            const char *s = __builtin_va_arg(ap, const char *);
            if (!s) s = "(null)";
            vga_puts(s); break; }
        case 'c': {
            char c = (char)__builtin_va_arg(ap, int);
            vga_putchar(c); break; }
        case 'l': {
            /* %lu or %lx */
            p++;
            if (*p == 'u') {
                uint64_t v = __builtin_va_arg(ap, uint64_t);
                uitoa(v, buf, 10); vga_puts(buf);
            } else if (*p == 'x') {
                uint64_t v = __builtin_va_arg(ap, uint64_t);
                uitoa(v, buf, 16);
                if (pad) { int len = (int)strlen(buf); for (int i=len;i<pad;i++) vga_putchar('0'); }
                vga_puts(buf);
            } else if (*p == 'd') {
                int64_t v = __builtin_va_arg(ap, int64_t);
                itoa(v, buf, 10); vga_puts(buf);
            }
            break; }
        case '%':
            vga_putchar('%'); break;
        default:
            vga_putchar('%'); vga_putchar(*p); break;
        }
    }
    __builtin_va_end(ap);
}
