#include "kernel.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile uint16_t *const vga = (uint16_t *)0xb8000;
static size_t row;
static size_t column;
static uint8_t color = 0x0f;

void serial_init(void) {
    outb(0x3f8 + 1, 0);
    outb(0x3f8 + 3, 0x80);
    outb(0x3f8, 3);
    outb(0x3f8 + 1, 0);
    outb(0x3f8 + 3, 3);
    outb(0x3f8 + 2, 0xc7);
    outb(0x3f8 + 4, 0x0b);
}

void serial_putc(char c) {
    while (!(inb(0x3f8 + 5) & 0x20)) {}
    outb(0x3f8, (uint8_t)c);
}

static void cursor_update(void) {
    uint16_t pos = (uint16_t)(row * VGA_WIDTH + column);
    outb(0x3d4, 14);
    outb(0x3d5, (uint8_t)(pos >> 8));
    outb(0x3d4, 15);
    outb(0x3d5, (uint8_t)pos);
}

static void scroll(void) {
    if (row < VGA_HEIGHT) return;
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga[(y - 1) * VGA_WIDTH + x] = vga[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)color << 8 | ' ';
    row = VGA_HEIGHT - 1;
}

void console_clear(void) {
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = (uint16_t)color << 8 | ' ';
    row = 0;
    column = 0;
    cursor_update();
}

void console_init(void) {
    serial_init();
    console_clear();
}

void console_putc(char c) {
    serial_putc(c);
    if (c == '\n') {
        column = 0;
        row++;
    } else if (c == '\b') {
        if (column) {
            column--;
            vga[row * VGA_WIDTH + column] = (uint16_t)color << 8 | ' ';
        }
    } else {
        vga[row * VGA_WIDTH + column] = (uint16_t)color << 8 | (uint8_t)c;
        if (++column == VGA_WIDTH) {
            column = 0;
            row++;
        }
    }
    scroll();
    cursor_update();
}

void console_write(const char *s) {
    while (*s) console_putc(*s++);
}

void console_write_dec(uint64_t value) {
    char digits[21];
    size_t n = 0;
    if (!value) {
        console_putc('0');
        return;
    }
    while (value) {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    }
    while (n) console_putc(digits[--n]);
}

void console_write_hex(uint64_t value) {
    static const char hex[] = "0123456789abcdef";
    console_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        console_putc(hex[(value >> shift) & 0xf]);
}
