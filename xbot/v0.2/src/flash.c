#include "flash.h"
#include "stm32f103_min.h"
static int wait_ready(void){uint32_t guard=8000000u;while((FLASH_SR&FLASH_SR_BSY)&&--guard){}if(!guard)return -1;if(FLASH_SR&(FLASH_SR_PGERR|FLASH_SR_WRPRTERR))return -2;return 0;}
static void clear_status(void){FLASH_SR=FLASH_SR_EOP|FLASH_SR_PGERR|FLASH_SR_WRPRTERR;}
int flash_unlock(void){if(!(FLASH_CR&FLASH_CR_LOCK))return 0;FLASH_KEYR=FLASH_KEY1;FLASH_KEYR=FLASH_KEY2;return (FLASH_CR&FLASH_CR_LOCK)?-1:0;}
void flash_lock(void){FLASH_CR|=FLASH_CR_LOCK;}
int flash_erase_page(uint32_t addr){if(wait_ready())return -1;clear_status();FLASH_CR|=FLASH_CR_PER;FLASH_AR=addr;FLASH_CR|=FLASH_CR_STRT;int r=wait_ready();FLASH_CR&=~FLASH_CR_PER;if(r)return r;for(uint32_t p=addr;p<addr+FLASH_PAGE_SIZE;p+=4u)if(REG32(p)!=0xffffffffu)return -3;return 0;}
int flash_erase_range(uint32_t addr,uint32_t len){if(!len)return 0;uint32_t end=addr+len;for(uint32_t p=addr;p<end;p+=FLASH_PAGE_SIZE){int r=flash_erase_page(p);if(r)return r;}return 0;}
int flash_program(uint32_t addr,const uint8_t*data,uint32_t len){if((addr&1u)||(len&1u))return -1;for(uint32_t i=0;i<len;i+=2u){if(wait_ready())return -2;clear_status();FLASH_CR|=FLASH_CR_PG;uint16_t v=(uint16_t)data[i]|((uint16_t)data[i+1]<<8);REG16(addr+i)=v;int r=wait_ready();FLASH_CR&=~FLASH_CR_PG;if(r)return r;if(REG16(addr+i)!=v)return -3;}return 0;}
