#ifndef LOG_H
#define LOG_H

#include "serial.h"

#define LOG_OK(msg) { serial_write_str("[OK] " msg); }

#define LOG_FAIL(msg) {serial_write_str("[FAIL] "msg); }

#define LOG_INFO(msg) {serial_write_str("[INFO] "msg); }

#endif // LOG_H