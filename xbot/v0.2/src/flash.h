#pragma once
#include <stdint.h>
#define FLASH_PAGE_SIZE 0x400u
int flash_unlock(void);
void flash_lock(void);
int flash_erase_page(uint32_t addr);
int flash_erase_range(uint32_t addr,uint32_t len);
int flash_program(uint32_t addr,const uint8_t*data,uint32_t len);
