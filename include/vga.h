#ifndef VGA_H
#define VGA_H

void vga_clear(void);
void print_char(char c, char attr);
void print_str(const char *str, char attr);
void print_hex_byte(unsigned char b, char attr);
void print_hex_uint32(unsigned int val, char attr);
void print_hex_uint64(unsigned long long val, char attr);

void log_str(const char *str, char attr);
void log_hex_uint32(unsigned int val, char attr);
void log_hex_uint64(unsigned long long val, char attr);

#endif
