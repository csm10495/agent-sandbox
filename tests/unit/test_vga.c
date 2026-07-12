/* tests/unit/test_vga.c - Tests for VGA buffer logic (host-side mock) */
#include "framework.h"
#include "string.h"

/* VGA buffer layout constants */
#define VGA_COLS 80
#define VGA_ROWS 25
static uint16_t vga_host_buf[VGA_ROWS * VGA_COLS];

void suite_vga(void) {
    /* Test itoa / uitoa (used by vga_printf) */
    char buf[32];

    itoa(0,    buf, 10); ASSERT_STREQ(buf, "0");
    itoa(1234, buf, 10); ASSERT_STREQ(buf, "1234");
    itoa(-1,   buf, 10); ASSERT_STREQ(buf, "-1");

    uitoa(0xDEADBEEFULL, buf, 16);
    ASSERT_STREQ(buf, "deadbeef");

    uitoa(0, buf, 10);   ASSERT_STREQ(buf, "0");
    uitoa(1, buf, 16);   ASSERT_STREQ(buf, "1");

    /* VGA buffer entry layout: char in low byte, attribute in high byte */
    memset(vga_host_buf, 0, sizeof(vga_host_buf));
    uint16_t entry = (uint16_t)'A' | ((uint16_t)0x07u << 8);
    vga_host_buf[0] = entry;
    ASSERT_EQ(vga_host_buf[0] & 0xFFu, (uint16_t)'A');
    ASSERT_EQ(vga_host_buf[0] >> 8,    (uint16_t)0x07u);

    /* Scroll simulation: copy row 1 to row 0 */
    for (int c = 0; c < VGA_COLS; c++)
        vga_host_buf[VGA_COLS + c] = (uint16_t)'B' | ((uint16_t)0x07u << 8);
    for (int c = 0; c < VGA_COLS; c++)
        vga_host_buf[c] = vga_host_buf[VGA_COLS + c];
    ASSERT_EQ(vga_host_buf[0] & 0xFFu, (uint16_t)'B');
}
