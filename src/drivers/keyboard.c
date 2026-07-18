#include "drivers/kbd/keyboard.h"
#include "drivers/kbd/scancodes.h"
#include "std/io.h"

static int shift_active = 0;


void keyboard_init(void) {
    // Flush the keyboard output buffer by reading any stale data
    while (inb(0x64) & 1) {
        inb(0x60);
    }
}

static int keyboard_data_available(void) {
    return inb(0x64) & 1;
}

static unsigned char keyboard_read_scancode(void) {
    while (!keyboard_data_available());
    return inb(0x60);
}

char keyboard_get_char(void) {
    while (1) {
        unsigned char scancode = keyboard_read_scancode();
        
        // Check for Left/Right Shift presses
        if (scancode == 0x2A || scancode == 0x36) {
            shift_active = 1;
            continue;
        }
        
        // Check for Left/Right Shift releases
        if (scancode == 0xAA || scancode == 0xB6) {
            shift_active = 0;
            continue;
        }
        
        // Ignore other key release events (break codes have bit 7 set)
        if (scancode & 0x80) {
            continue;
        }
        
        // Map scancode to ASCII
        if (scancode < sizeof(keymap)) {
            char c = shift_active ? keymap_shifted[scancode] : keymap[scancode];
            if (c != 0) {
                return c;
            }
        }
    }
}
