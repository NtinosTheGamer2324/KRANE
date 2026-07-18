#include "vga.h"
#include "serial.h"

static char *vga = (char *)0xB8000;
static int cursor_x = 0;
static int cursor_y = 0;

void vga_clear(void) {
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        vga[i] = ' ';
        vga[i+1] = 0x07; // Light grey on black
    }
    cursor_x = 0;
    cursor_y = 0;
}

void print_char(char c, char attr) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            int offset = (cursor_y * 80 + cursor_x) * 2;
            vga[offset] = ' ';
            vga[offset + 1] = attr;
        }
    } else {
        int offset = (cursor_y * 80 + cursor_x) * 2;
        vga[offset] = c;
        vga[offset + 1] = attr;
        cursor_x++;
        if (cursor_x >= 80) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    
    // Vertical scrolling
    if (cursor_y >= 25) {
        for (int y = 0; y < 24; y++) {
            for (int x = 0; x < 80; x++) {
                int src = ((y + 1) * 80 + x) * 2;
                int dest = (y * 80 + x) * 2;
                vga[dest] = vga[src];
                vga[dest + 1] = vga[src + 1];
            }
        }
        for (int x = 0; x < 80; x++) {
            int dest = (24 * 80 + x) * 2;
            vga[dest] = ' ';
            vga[dest + 1] = 0x07;
        }
        cursor_y = 24;
    }
}

void print_str(const char *str, char attr) {
    for (int i = 0; str[i] != '\0'; i++) {
        print_char(str[i], attr);
    }
}

static void print_hex_digit(unsigned char d, char attr) {
    if (d < 10) {
        print_char('0' + d, attr);
    } else {
        print_char('A' + (d - 10), attr);
    }
}

void print_hex_byte(unsigned char b, char attr) {
    print_hex_digit(b >> 4, attr);
    print_hex_digit(b & 0x0F, attr);
}

void print_hex_uint32(unsigned int val, char attr) {
    print_str("0x", attr);
    for (int i = 3; i >= 0; i--) {
        print_hex_byte((val >> (i * 8)) & 0xFF, attr);
    }
}

void print_hex_uint64(unsigned long long val, char attr) {
    print_str("0x", attr);
    for (int i = 7; i >= 0; i--) {
        print_hex_byte((val >> (i * 8)) & 0xFF, attr);
    }
}

void log_str(const char *str, char attr) {
    print_str(str, attr);
    serial_write_str(str);
}

void log_hex_uint32(unsigned int val, char attr) {
    print_hex_uint32(val, attr);
    
    // Output to serial
    serial_write_str("0x");
    for (int i = 3; i >= 0; i--) {
        unsigned char b = (val >> (i * 8)) & 0xFF;
        unsigned char d1 = b >> 4;
        unsigned char d2 = b & 0x0F;
        serial_write_char(d1 < 10 ? '0' + d1 : 'A' + (d1 - 10));
        serial_write_char(d2 < 10 ? '0' + d2 : 'A' + (d2 - 10));
    }
}

void log_hex_uint64(unsigned long long val, char attr) {
    print_hex_uint64(val, attr);
    
    // Output to serial
    serial_write_str("0x");
    for (int i = 7; i >= 0; i--) {
        unsigned char b = (val >> (i * 8)) & 0xFF;
        unsigned char d1 = b >> 4;
        unsigned char d2 = b & 0x0F;
        serial_write_char(d1 < 10 ? '0' + d1 : 'A' + (d1 - 10));
        serial_write_char(d2 < 10 ? '0' + d2 : 'A' + (d2 - 10));
    }
}
