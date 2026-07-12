#include "keyboard.h"
#include "idt.h"
#include "pic.h"

/* Keyboard buffer */
#define KBD_BUF_SIZE 256
static volatile char kbd_buf[KBD_BUF_SIZE];
static volatile int  kbd_head = 0, kbd_tail = 0;
static volatile bool ctrl_c_flag = false;

/* Modifier state */
static bool shift_down  = false;
static bool caps_lock   = false;
static bool ctrl_down   = false;

/* US QWERTY scan-code set 1 -> ASCII (lowercase) */
static const char sc_normal[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ', 0,
    0,0,0,0,0,0,0,0,0,0, 0, 0, 0,0,0,0,0,0,0,0, 0, 0, 0, 0, 0, 0
};

/* Shifted version */
static const char sc_shift[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0, '|',
    'Z','X','C','V','B','N','M','<','>','?', 0,'*', 0, ' ', 0,
    0,0,0,0,0,0,0,0,0,0, 0, 0, 0,0,0,0,0,0,0,0, 0, 0, 0, 0, 0, 0
};

static void kbd_enqueue(char c) {
    int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next != kbd_tail) {   /* drop if full */
        kbd_buf[kbd_head] = c;
        kbd_head = next;
    }
}

static void keyboard_irq(void) {
    uint8_t sc = inb(0x60);
    bool released = (sc & 0x80) != 0;
    uint8_t key   = sc & 0x7F;

    if (key == 0x2A || key == 0x36) { /* L/R Shift */
        shift_down = !released; return;
    }
    if (key == 0x1D) { ctrl_down = !released; return; }
    if (key == 0x3A && !released) { caps_lock = !caps_lock; return; }

    if (released) return;

    if (ctrl_down && key == 0x2E) {  /* Ctrl+C */
        ctrl_c_flag = true;
        kbd_enqueue('\x03');
        return;
    }

    bool upper = shift_down ^ caps_lock;
    char c = upper ? sc_shift[key] : sc_normal[key];
    if (c) kbd_enqueue(c);
}

void keyboard_init(void) {
    /* Flush any pending data */
    while (inb(0x64) & 1) inb(0x60);
    irq_register(1, keyboard_irq);
    pic_unmask(1);
}

int keyboard_getchar(void) {
    while (kbd_head == kbd_tail)
        cpu_pause();
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return (unsigned char)c;
}

int keyboard_poll(void) {
    if (kbd_head == kbd_tail) return -1;
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return (unsigned char)c;
}

bool keyboard_ctrl_c(void) {
    if (ctrl_c_flag) { ctrl_c_flag = false; return true; }
    return false;
}
