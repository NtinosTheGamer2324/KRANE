[org 0x7e00]
[bits 16]

stage2_entry:
    ; Disable interrupts
    cli
    
    ; Clear segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    ; Print start message
    mov si, msg_stage2_start
    call print_string_16
    
    ; 1. Enable A20 Gate
    call enable_a20
    
    ; 2. Detect memory via E820
    call detect_memory
    
    ; 3. Load GDT
    lgdt [gdt_descriptor]
    
    ; 4. Switch to Protected Mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; 5. Far jump to 32-bit code segment
    jmp CODE_SEG:init_pm

[bits 32]
init_pm:
    ; Set up 32-bit segment registers
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack
    mov esp, 0x7C000
    
    ; Enable SSE
    mov eax, cr0
    and ax, 0xFFFB      ; Clear EM (bit 2)
    or ax, 0x0002       ; Set MP (bit 1)
    mov cr0, eax
    
    mov eax, cr4
    or eax, 0x00000600  ; Set OSFXSR (bit 9) and OSXMMEXCPT (bit 10)
    mov cr4, eax
    
    ; Jump to Stage 3 (C code) at 0x8200 (Stage 2 starts at 0x7E00 + 1024-byte entry)
    jmp 0x8200

[bits 16]
; --- 16-bit Helper Functions ---

; Print string utility for 16-bit real mode
print_string_16:
    push ax
    push bx
    mov ah, 0x0e
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; Check if A20 is enabled
check_a20:
    pushf
    push ds
    push es
    push di
    push si
    
    xor ax, ax
    mov ds, ax
    mov di, 0x7dfe
    
    not ax ; ax = 0xffff
    mov es, ax
    mov si, 0x800e ; 0xffff:0x800e = 0x107dfe
    
    mov al, [ds:di]
    mov ah, [es:si]
    
    cmp al, ah
    jne .a20_enabled
    
    mov byte [ds:di], 0x00
    mov byte [es:si], 0xFF
    cmp byte [ds:di], 0xFF
    je .a20_disabled
    
.a20_enabled:
    mov [ds:di], al ; restore
    mov ax, 1
    jmp .exit
    
.a20_disabled:
    mov [ds:di], al ; restore
    xor ax, ax
    
.exit:
    pop si
    pop di
    pop es
    pop ds
    popf
    ret

; Enable A20 Gate using multiple fallback methods
enable_a20:
    call check_a20
    test ax, ax
    jnz .success
    
    ; Method 1: Try BIOS
    mov ax, 0x2401
    int 0x15
    call check_a20
    test ax, ax
    jnz .success
    
    ; Method 2: Try Fast A20 (Port 0x92)
    in al, 0x92
    or al, 2
    out 0x92, al
    call check_a20
    test ax, ax
    jnz .success
    
    ; Method 3: Try Keyboard Controller (8042)
    cli
    call a20_wait_command
    mov al, 0xad
    out 0x64, al
    
    call a20_wait_command
    mov al, 0xd0
    out 0x64, al
    
    call a20_wait_data
    in al, 0x60
    push ax
    
    call a20_wait_command
    mov al, 0xd1
    out 0x64, al
    
    call a20_wait_command
    pop ax
    or al, 2
    out 0x60, al
    
    call a20_wait_command
    mov al, 0xae
    out 0x64, al
    
    call check_a20
    test ax, ax
    jnz .success
    
    ; If all methods fail
    mov si, msg_a20_fail
    call print_string_16
    cli
    hlt

.success:
    mov si, msg_a20_ok
    call print_string_16
    ret

a20_wait_command:
    in al, 0x64
    test al, 2
    jnz a20_wait_command
    ret

a20_wait_data:
    in al, 0x64
    test al, 1
    jz a20_wait_data
    ret

; Detect memory map via E820
detect_memory:
    mov si, msg_e820_start
    call print_string_16
    
    ; Destination for map is ES:DI = 0:0x9008
    xor ax, ax
    mov es, ax
    mov di, 0x508
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x534d4150 ; 'SMAP'
    
.loop:
    mov eax, 0xe820
    mov [es:di + 20], dword 1 ; ACPI 3.0 extended attribute write
    mov ecx, 24
    int 0x15
    jc .done
    cmp eax, 0x534d4150
    jne .error
    
    cmp ecx, 24
    jne .skip_acpi
    test byte [es:di + 20], 1
    jz .skip_entry
    
.skip_acpi:
    mov eax, [es:di + 8]
    or eax, [es:di + 12]
    jz .skip_entry
    
    inc bp
    add di, 24
    
.skip_entry:
    test ebx, ebx
    jz .done
    jmp .loop
    
.done:
    mov [0x500], bp ; store entry count at 0x9000
    mov [0x502], word 0 ; clear upper 16-bits of count
    mov si, msg_e820_ok
    call print_string_16
    ret
    
.error:
    mov dword [0x9000], 0
    mov si, msg_e820_fail
    call print_string_16
    ret

; --- Data and Tables ---

msg_stage2_start db "KRANE Stage 2: Initializing...", 0x0d, 0x0a, 0
msg_a20_ok       db "  A20 gate enabled.", 0x0d, 0x0a, 0
msg_a20_fail     db "  CRITICAL: A20 gate failed to enable!", 0x0d, 0x0a, 0
msg_e820_start   db "  Detecting memory map (E820)...", 0x0d, 0x0a, 0
msg_e820_ok      db "  Memory map detected successfully.", 0x0d, 0x0a, 0
msg_e820_fail    db "  WARNING: E820 memory map failed!", 0x0d, 0x0a, 0

gdt_start:
    dd 0
    dd 0
gdt_code:
    dw 0xffff
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0
gdt_data:
    dw 0xffff
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; Pad to exactly 1024 bytes (2 sectors)
times 1024 - ($ - $$) db 0
