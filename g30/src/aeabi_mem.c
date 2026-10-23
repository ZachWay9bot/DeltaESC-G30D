#include <stdint.h>
/* Cortex-M3 freestanding (no libc): compiler-emitted EABI zero-fill helper.
 * Never touch peripheral registers: called for local RAM structures only. */
void __aeabi_memclr4(void *dest, uint32_t bytes) {
    volatile uint8_t *p=(volatile uint8_t *)dest;
    for(uint32_t i=0u;i<bytes;i++) p[i]=0u;
}

/* Clang 18 may lower C aggregate copies to EABI memcpy even with
 * -ffreestanding. Provide a deterministic Cortex-M3 implementation without
 * depending on an unavailable libc or compiler runtime. */
void __aeabi_memcpy(void *dest, const void *src, uint32_t bytes) {
    volatile uint8_t *d=(volatile uint8_t *)dest;
    const volatile uint8_t *s=(const volatile uint8_t *)src;
    for(uint32_t i=0u;i<bytes;i++) d[i]=s[i];
}
void __aeabi_memcpy4(void *dest, const void *src, uint32_t bytes) {
    __aeabi_memcpy(dest,src,bytes);
}
void __aeabi_memcpy8(void *dest, const void *src, uint32_t bytes) {
    __aeabi_memcpy(dest,src,bytes);
}
