#include "terminal.h"

void syscall_handler(registers_t* regs) {
    uint32_t num = regs->eax;
    if (num == 1) { /* sys_write(fd, buf, len) */
        const char* buf = (const char*)regs->ebx;
        uint32_t len = regs->ecx;
        /* ATENȚIE: buf este într-un spațiu user -> trebuie validat că paginile sunt mapped și accesibile */
        /* pentru început: copiați buffer într-un kernel buffer, cu validare simplă */
        char tmp[256];
        if (len > 255) len = 255;
        for (uint32_t i=0;i<len;i++) tmp[i] = ((char*)buf)[i];
        tmp[len] = 0;
        terminal_writestring(tmp);
        regs->eax = len;
    } else {
        regs->eax = -1;
    }
}
