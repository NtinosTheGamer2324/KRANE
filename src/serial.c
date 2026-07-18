#include "serial.h"
#include "std/io.h"

void serial_init(void) {
    outb(PORT_COM1 + 1, 0x00);    // Disable all interrupts
    outb(PORT_COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT_COM1 + 0, 0x01);    // Set divisor to 1 (115200 baud)
    outb(PORT_COM1 + 1, 0x00);    //                  (hi byte)
    outb(PORT_COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT_COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT_COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

static int is_transmit_empty(void) {
    return inb(PORT_COM1 + 5) & 0x20;
}

void serial_write_char(char c) {
    while (is_transmit_empty() == 0);
    outb(PORT_COM1, c);
}

void serial_write_str(const char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(str[i]);
    }
}

static void com_write_char(uint8_t port, char c) {
    while (is_transmit_empty() == 0);
    outb(port, c);
}

void com_write_str(uint8_t port, const char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            com_write_char(port, '\r');
        }
        com_write_char(port, str[i]);
    }
}