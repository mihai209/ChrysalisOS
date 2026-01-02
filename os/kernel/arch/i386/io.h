#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Port I/O pentru i386 — functii inline pentru a le putea include in
   mai multe fisiere fara simboluri externe duplicate. */

/* write byte to port */
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* read byte from port */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* write word (16-bit) to port */
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* read word (16-bit) from port */
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* legacy io wait (approx 400ns) — write to port 0x80 */
static inline void io_wait(void) {
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}

#ifdef __cplusplus
}
#endif
