[org 0x7c00]
[bits 16]

start:
    ; Disable interrupts
    cli
    
    ; Setup segment registers to 0
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    
    ; Save BIOS boot drive number
    mov [boot_drive], dl
    
    ; Print loading message
    mov si, msg_loading
    call print_string
    
    ; Load Stage 2 from the disk.
    ; Stage 2 is located at sector 2 (LBA 1).
    ; We read 96 sectors (48KB) to accommodate the entry stub and C kernel.
    ; NOTE: bump this further if the linked kernel (stage2_entry + stage3
    ; C code) ever approaches this size -- the disk read here is the hard
    ; ceiling on total kernel size. If you add a lot of code/logging and
    ; start triple-faulting partway through boot with no serial output,
    ; check here first before anything else.
    mov dl, [boot_drive]
    mov dh, 0           ; Head 0
    mov ch, 0           ; Cylinder 0
    mov cl, 2           ; Sector 2 (1-based index)
    mov al, 96          ; Number of sectors to read (96 sectors = 48KB)
    mov bx, 0x7e00      ; Destination ES:BX = 0x0000:0x7e00
    
    mov ah, 0x02        ; BIOS Read Sectors function
    int 0x13
    jc disk_error
    
    cmp al, 96          ; Did we read all 96 sectors?
    jne disk_error
    
    ; Print success and jump to Stage 2
    mov si, msg_jump
    call print_string
    
    jmp 0x0000:0x7e00

disk_error:
    mov si, msg_error
    call print_string
.halt:
    cli
    hlt
    jmp .halt

; Print string utility
print_string:
    push ax
    push bx
    mov ah, 0x0e        ; BIOS Teletype Output
.loop:
    lodsb               ; Load AL from DS:SI, increment SI
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

boot_drive db 0
msg_loading db "KRANE Stage 1: Loading Stage 2 (96 sectors)...", 0x0d, 0x0a, 0
msg_error db "KRANE: Disk read error!", 0x0d, 0x0a, 0
msg_jump db "KRANE Stage 1: Jumping to Stage 2...", 0x0d, 0x0a, 0

; Boot sector padding and signature
times 510 - ($ - $$) db 0
dw 0xaa55