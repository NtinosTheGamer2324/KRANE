#ifndef SERIAL_H
#define SERIAL_H

#define PORT_COM1 0x3F8
#define PORT_COM2 0x2F8

#include <stdint.h>

void serial_init(void);
void serial_write_char(char c);
void serial_write_str(const char *str);

void com_write_str(uint8_t, const char *str);

#endif
