; macro for writing to console
%macro WRITE 2
    mov rax, SYS_WRITE          ; syscall to write
    mov rdi, STDOUT             ; 1st arg - file descriptor
    mov rsi, %1                 ; 2nd arg - string
    mov rdx, %2                 ; 3rd arg - string / buffer length
    syscall
%endmacro

section .data
    SYS_UNAME:      equ 63
    SYS_WRITE:      equ 1
    SYS_EXIT:       equ 60
    STDOUT:         equ 1

    hostnamemsg:    db "Hostname: "
    hostnamemsglen: equ $-hostnamemsg
    osmsg:          db 0xA, "OS: "
    osmsglen:       equ $-osmsg
    versionmsg:     db 0xA, "Version: "
    versionmsglen:  equ $-versionmsg
    releasemsg:     db 0xA, "Release: "
    releasemsglen:  equ $-releasemsg
    newlinemsg:     db 0xA
    newlinemsglen:  equ $-newlinemsg

    ; define struct utsname from sys/utsname.h
    struc utsname
        sysname:    resb 65
        nodename:   resb 65
        release:    resb 65
        version:    resb 65
        machine:    resb 65
    endstruc

    sysinfo:        istruc utsname
                        at sysname, db ""
                        at nodename, db ""
                        at release, db ""
                        at version, db ""
                        at machine, db ""
    iend

global _start
section .text

    ; int uname(struct utsname *buf); (*buf -> sysinfo)
    uname:
        mov rax, SYS_UNAME
        mov rdi, sysinfo
        syscall
        ret

    ; void _exit(int status); (status -> r8)
    exit:
        mov rax, SYS_EXIT
        mov rdi, r8
        syscall
        ret

    _start:
        call uname

        WRITE hostnamemsg, hostnamemsglen
        WRITE sysinfo + nodename, 65
        WRITE osmsg, osmsglen
        WRITE sysinfo + sysname, 65
        WRITE versionmsg, versionmsglen
        WRITE sysinfo + version, 65
        WRITE releasemsg, releasemsglen
        WRITE sysinfo + release, 65
        WRITE newlinemsg, newlinemsglen

        ; exit with status 0
        mov r8, 0
        call exit
