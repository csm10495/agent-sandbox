#pragma once
#include "types.h"

/* VGA text mode: 80 columns x 25 rows at physical 0xB8000 */
#define VGA_COLS  80
#define VGA_ROWS  25

typedef enum vga_color {
    VGA_BLACK=0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
} vga_color_t;

void vga_init(void);
void vga_clear(void);
void vga_setcolor(vga_color_t fg, vga_color_t bg);
void vga_putchar(char c);
void vga_puts(const char *s);
void vga_printf(const char *fmt, ...);
void vga_set_cursor(int row, int col);
int  vga_get_row(void);
int  vga_get_col(void);
