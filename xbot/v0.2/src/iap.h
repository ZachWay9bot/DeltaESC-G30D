#pragma once
#include <stdint.h>
enum {IAP_OK=0,IAPERROR_SIZE=1,IAPERROR_ERASE=2,IAPERROR_WRITEFLASH=3,IAPERROR_UNLOCK=4,IAPERROR_INDEX=5,IAPERROR_BUSY=6,IAPERROR_FORM=7,IAPERROR_CRC=8,IAPERROR_OTHER=9};
#define APP_BASE       0x08001000u
#define STAGING_BASE   0x0800E800u
#define STAGING_LIMIT  0x0801C000u
#define STAGING_MAX    (STAGING_LIMIT-STAGING_BASE)
#define UPDATE_CONTROL 0x0801F800u
void iap_init(void);
uint8_t iap_begin(const uint8_t*data,uint8_t len);
uint8_t iap_write(uint8_t index,const uint8_t*data,uint8_t len);
uint8_t iap_finalize(const uint8_t*data,uint8_t len);
uint8_t iap_can_reset(void);
uint32_t iap_total_size(void);
uint32_t iap_written(void);
uint8_t iap_expected_index(void);
