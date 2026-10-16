#pragma once
#include <stdint.h>
void uart1_init(void);
int uart1_getc(void);
void uart1_putc(uint8_t c);
void uart1_write(const uint8_t*p,uint32_t n);
void uart1_wait_complete(void);
