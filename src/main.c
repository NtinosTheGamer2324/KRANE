#include "serial.h"
#include "vga.h"
#include "drivers/kbd/keyboard.h"
#include "std/io.h"
#include "memory/string.h"
#include "memory/e820.h"
#include "memory/pmm.h"
#include "memory/paging.h"
#include "memory/heap.h"
#include "log.h"

#define MAX_ENTRIES 5
static const char *menu_entries[MAX_ENTRIES] = {
    "Boot ModuOS Kernel (Default)",
    "Boot ModuOS in Safe Mode",
    "Run Memory Diagnostics",
    "Boot ModuOS Recovery Mode",
    "Reboot System"
};

static void print_mmap(void) {
    unsigned int *entry_count_ptr = (unsigned int *)E820_COUNT_ADDR;
    unsigned int entry_count = *entry_count_ptr;
    e820_entry_t *entries = (e820_entry_t *)E820_ENTRIES_ADDR;
    
    log_str("E820 Memory Map Entries Count: ", 0x07);
    log_hex_uint32(entry_count, 0x0E);
    log_str("\n\n", 0x07);
    
    log_str("  #  | Base Address        | Length              | Type\n", 0x0F);
    log_str("-----+--------------------+--------------------+------\n", 0x07);
    
    for (unsigned int i = 0; i < entry_count; i++) {
        log_str(" ", 0x07);
        print_hex_byte(i, 0x0E);
        
        // Print entry number hex byte to serial
        unsigned char d1 = i >> 4;
        unsigned char d2 = i & 0x0F;
        serial_write_char(d1 < 10 ? '0' + d1 : 'A' + (d1 - 10));
        serial_write_char(d2 < 10 ? '0' + d2 : 'A' + (d2 - 10));
        
        log_str(" | ", 0x07);
        
        log_hex_uint64(entries[i].base_addr, 0x0F);
        log_str(" | ", 0x07);
        
        log_hex_uint64(entries[i].length, 0x0F);
        log_str(" | ", 0x07);
        
        char type_attr = 0x07;
        const char *type_name = "Unknown";
        
        switch (entries[i].type) {
            case 1:
                type_name = "USABLE";
                type_attr = 0x0A;
                break;
            case 2:
                type_name = "RSVD  ";
                type_attr = 0x0C;
                break;
            case 3:
                type_name = "ACPI_R";
                type_attr = 0x0B;
                break;
            case 4:
                type_name = "ACPI_N";
                type_attr = 0x0D;
                break;
            case 5:
                type_name = "BAD   ";
                type_attr = 0x04;
                break;
            default:
                type_name = "UNKNWN";
                type_attr = 0x0E;
                break;
        }
        
        log_str(type_name, type_attr);
        log_str("\n", 0x07);
    }
}

static void print_meminfo(void) {
    unsigned int total = pmm_get_total_block_count();
    unsigned int used = pmm_get_used_block_count();
    unsigned int free_blocks = pmm_get_free_block_count();

    log_str("Physical Memory Manager:\n", 0x0F);
    log_str("  Total: ", 0x07);
    log_hex_uint32(total * PMM_BLOCK_SIZE, 0x0E);
    log_str(" bytes (", 0x07);
    log_hex_uint32(total, 0x07);
    log_str(" blocks)\n", 0x07);

    log_str("  Used:  ", 0x07);
    log_hex_uint32(used * PMM_BLOCK_SIZE, 0x0C);
    log_str(" bytes (", 0x07);
    log_hex_uint32(used, 0x07);
    log_str(" blocks)\n", 0x07);

    log_str("  Free:  ", 0x07);
    log_hex_uint32(free_blocks * PMM_BLOCK_SIZE, 0x0A);
    log_str(" bytes (", 0x07);
    log_hex_uint32(free_blocks, 0x07);
    log_str(" blocks)\n\n", 0x07);

    log_str("Kernel Heap:\n", 0x0F);
    log_str("  Used:  ", 0x07);
    log_hex_uint32(heap_get_used_bytes(), 0x0C);
    log_str(" bytes\n", 0x07);
    log_str("  Free:  ", 0x07);
    log_hex_uint32(heap_get_free_bytes(), 0x0A);
    log_str(" bytes\n", 0x07);
}

static void execute_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        log_str("Available commands:\n", 0x0F);
        log_str("  help    - Show this list\n", 0x07);
        log_str("  mmap    - Print the E820 system memory map\n", 0x07);
        log_str("  meminfo - Print PMM/heap allocator statistics\n", 0x07);
        log_str("  clear   - Clear the screen\n", 0x07);
        log_str("  reboot  - Reboot the computer\n", 0x07);
        log_str("  menu    - Return to the boot menu\n", 0x07);
        log_str("  say <msg> - Echo <msg> to screen & serial\n", 0x07);
    } else if (strcmp(cmd, "mmap") == 0) {
        print_mmap();
    } else if (strcmp(cmd, "meminfo") == 0) {
        print_meminfo();
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
    } else if (strcmp(cmd, "reboot") == 0) {
        log_str("Rebooting...\n", 0x0C);
        // Pulse system reset line using the keyboard controller
        outb(0x64, 0xFE);
        // Fallback: infinite loop if reset fails
        while (1) {
            __asm__ __volatile__("hlt");
        }
    } else if (strcmp(cmd, "menu") == 0) {
        // Exit shell loop and return to menu
        log_str("Returning to boot menu...\n", 0x0A);
    } else if (strncmp(cmd, "say ", 4) == 0) {
        log_str(cmd + 4, 0x0E);
        log_str("\n", 0x07);
    } else if (strcmp(cmd, "") == 0) {
        // Do nothing on empty command
    } else {
        log_str("Unknown command: ", 0x0C);
        log_str(cmd, 0x0F);
        log_str("\nType 'help' for available commands.\n", 0x07);
    }
}

static void shell_loop(void) {
    char cmd_buf[64];
    int cmd_len = 0;
    
    log_str("krane-rescue> ", 0x0E);
    
    while (1) {
        char c = keyboard_get_char();
        
        if (c == '\n') {
            log_str("\n", 0x07);
            cmd_buf[cmd_len] = '\0';
            
            // Special exit shell condition
            if (strcmp(cmd_buf, "menu") == 0) {
                execute_command(cmd_buf);
                break;
            }
            
            execute_command(cmd_buf);
            cmd_len = 0;
            log_str("krane-rescue> ", 0x0E);
        } else if (c == '\b') {
            if (cmd_len > 0) {
                cmd_len--;
                // Backspace on VGA screen
                print_char('\b', 0x07);
                // Backspace on Serial (write backspace, space, backspace to overwrite char)
                serial_write_char('\b');
                serial_write_char(' ');
                serial_write_char('\b');
            }
        } else {
            // Buffer limit check
            if (cmd_len < 63) {
                cmd_buf[cmd_len++] = c;
                // Echo key to screen and serial
                print_char(c, 0x0F);
                serial_write_char(c);
            }
        }
    }
}

#define BOX_INNER 76  // fixed inner width between | borders

static void print_entry_row(const char *text, char text_attr, char border_attr, char pad_attr) {
    int len = strlen_simple(text);
    int pad = BOX_INNER - 1 - len - 1; // 1 leading space + text + pad + 1 trailing space

    print_char('|', border_attr);
    print_char(' ', text_attr);
    print_str(text, text_attr);
    for (int i = 0; i < pad; i++) {
        print_char(' ', pad_attr);
    }
    print_char(' ', text_attr);
    print_char('|', border_attr);
    print_char('\n', 0x07);
}

static void draw_menu(int selected_index) {
    vga_clear();

    // Header (centered in 80 cols)
    print_str("\n", 0x07);
    print_str("                               KRANE Bootloader                                \n", 0x0F);
    print_str("                          Version 1.0.0 - ntsoftware                          \n", 0x08);
    print_str("\n", 0x07);

    // Top border: 2 indent + + + 76 dashes + + = 80
    print_str(" +----------------------------------------------------------------------------+\n", 0x07);

    for (int i = 0; i < MAX_ENTRIES; i++) {
        print_char(' ', 0x07); // 1 char left indent
        if (i == selected_index) {
            // White on cyan highlight bar
            print_entry_row(menu_entries[i], 0x3F, 0x07, 0x3F);
        } else {
            print_entry_row(menu_entries[i], 0x07, 0x08, 0x07);
        }
    }

    // Bottom border
    print_str(" +----------------------------------------------------------------------------+\n", 0x07);

    // Push footer to bottom
    int used_rows = 1 + 1 + 1 + 1 + 1 + MAX_ENTRIES + 1; // header lines + borders + entries
    int blank = 25 - used_rows - 2; // 2 footer lines
    for (int i = 0; i < blank; i++) {
        print_str("\n", 0x07);
    }

    // Footer
    print_str("  Use arrow keys to navigate. Press ENTER to boot the selected OS.\n", 0x08);
    print_str("  Press ESC to open the KRANE rescue shell.\n", 0x08);
}

void menu_loop(void) {
    int selected = 0;
    
    // Log to serial port once that we are entering menu
    LOG_INFO("Displaying KRANE Boot Menu.\n");
    
    while (1) {
        draw_menu(selected);
        
        char c = keyboard_get_char();
        
        if (c == KEY_UP) {
            selected--;
            if (selected < 0) {
                selected = MAX_ENTRIES - 1;
            }
        } else if (c == KEY_DOWN) {
            selected++;
            if (selected >= MAX_ENTRIES) {
                selected = 0;
            }
        } else if (c == KEY_ESC) {
            // Drop to rescue console
            vga_clear();
            log_str("Type 'menu' to return to the boot menu.\n\n", 0x07);
            shell_loop();
            // Once we break out of shell_loop (via 'menu' command), we redraw the menu
            serial_write_str("[COM1 Log] Returning to KRANE Boot Menu.\n");
        } else if (c == KEY_ENTER) {
            // Boot selected entry
            vga_clear();
            log_str("Booting: ", 0x0A);
            log_str(menu_entries[selected], 0x0F);
            log_str("\n\nInitializing kernel boot sequence...\n", 0x07);
            
            // In the future we will load the kernel. For now, simulate boot & halt.
            log_str("Kernel boot simulated. Halting CPU.\n", 0x0E);
            while (1) {
                __asm__ __volatile__("hlt");
            }
        }
    }
}

void main() {
    // 1. Initialize drivers
    serial_init();
    LOG_INFO("=== KRANE boot sequence starting ===\n");

    LOG_INFO("Initialising PS/2 Keyboard\n");
    keyboard_init();
    LOG_OK("PS/2 Keyboard successfully initialised\n");

    // 2. Initialize the memory subsystem
    LOG_INFO("Initialising Physical Memory Manager\n");
    pmm_init();
    LOG_OK("PMM initialised\n");

    LOG_INFO("Initialising paging (identity-mapped boot region)\n");
    LOG_INFO("About to enable paging -- if the system resets here, the fault is in paging_init()\n");
    paging_init();
    LOG_OK("Paging enabled -- CR0/CR3 logged above by paging_init()\n");

    LOG_INFO("Initialising kernel heap\n");
    heap_init();
    LOG_OK("Heap initialised\n");

    LOG_INFO("=== KRANE boot sequence complete, entering menu loop ===\n");

    // 3. Start Menu Loop
    menu_loop();
}