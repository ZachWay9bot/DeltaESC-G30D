#include "iap.h"
#include "flash.h"
#include "ninebot_tea.h"
#include "stm32f103_min.h"

typedef struct{
 uint32_t total,written,body_sum,enc_sum;
 uint8_t expected_index,begun,finalized,vector_ok;
 ninebot_tea_t tea;
} iap_state_t;
static iap_state_t s;

static uint32_t rd32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t rol16(uint32_t v){return (v<<16)|(v>>16);}
static int vector_valid(const uint8_t*p){uint32_t sp=rd32(p),pc=rd32(p+4);if((sp&0x2FFE0000u)!=0x20000000u)return 0;if(!(pc&1u))return 0;pc&=~1u;return pc>=APP_BASE && pc<STAGING_BASE;}
static int control_write(uint32_t pending,uint32_t size){uint8_t b[12]={0x5A,0x50,0x00,0x00,0,0,0,0,0,0,0,0};b[4]=(uint8_t)pending;b[5]=(uint8_t)(pending>>8);b[6]=(uint8_t)(pending>>16);b[7]=(uint8_t)(pending>>24);b[8]=(uint8_t)size;b[9]=(uint8_t)(size>>8);b[10]=(uint8_t)(size>>16);b[11]=(uint8_t)(size>>24);if(flash_erase_page(UPDATE_CONTROL))return -1;if(flash_program(UPDATE_CONTROL,b,sizeof b))return -2;return 0;}

void iap_init(void){s.total=s.written=s.body_sum=s.enc_sum=0;s.expected_index=0;s.begun=s.finalized=s.vector_ok=0;ninebot_tea_init(&s.tea);}
uint32_t iap_total_size(void){return s.total;}uint32_t iap_written(void){return s.written;}uint8_t iap_expected_index(void){return s.expected_index;}

uint8_t iap_begin(const uint8_t*data,uint8_t len){
 if(len!=4u)return IAPERROR_FORM;
 uint32_t n=rd32(data);
 if(n<16u || n>STAGING_MAX || (n&7u))return IAPERROR_SIZE;
 if(flash_unlock())return IAPERROR_UNLOCK;
 if(control_write(0u,0u)){flash_lock();return IAPERROR_ERASE;}
 uint32_t erase_len=(n+FLASH_PAGE_SIZE-1u)&~(FLASH_PAGE_SIZE-1u);
 if(STAGING_BASE+erase_len>STAGING_LIMIT){flash_lock();return IAPERROR_SIZE;}
 if(flash_erase_range(STAGING_BASE,erase_len)){flash_lock();return IAPERROR_ERASE;}
 flash_lock();
 s.total=n;s.written=0;s.body_sum=0;s.enc_sum=0;s.expected_index=0;s.begun=1;s.finalized=0;s.vector_ok=0;ninebot_tea_init(&s.tea);
 return IAP_OK;
}

uint8_t iap_write(uint8_t index,const uint8_t*data,uint8_t len){
 if(!s.begun || s.finalized)return IAPERROR_BUSY;
 if(index!=(uint8_t)s.expected_index){if(index==(uint8_t)(s.expected_index-1u))return IAP_OK;return IAPERROR_INDEX;}
 if(!len)return IAPERROR_FORM;
 uint32_t rem=s.total-s.written;
 uint32_t use=len<rem?len:rem;
 if(!use || (use&7u))return IAPERROR_FORM;
 if(flash_unlock())return IAPERROR_UNLOCK;
 for(uint32_t off=0;off<use;off+=8u){
  uint8_t dec[8];
  for(unsigned j=0;j<8;j++)s.enc_sum+=(uint32_t)data[off+j];
  ninebot_tea_decrypt_block(&s.tea,data+off,dec);
  if((s.written+off)==0u){if(!vector_valid(dec)){flash_lock();s.begun=0;return IAPERROR_FORM;}s.vector_ok=1;}
  uint32_t abs=s.written+off;
  uint32_t body_end=s.total-4u;
  if(abs+4u<=body_end)s.body_sum+=rd32(dec);
  if(abs+8u<=body_end)s.body_sum+=rd32(dec+4);
  if(flash_program(STAGING_BASE+abs,dec,8u)){flash_lock();return IAPERROR_WRITEFLASH;}
 }
 flash_lock();
 s.written+=use;s.expected_index=(uint8_t)(s.expected_index+1u);
 return IAP_OK;
}

uint8_t iap_finalize(const uint8_t*data,uint8_t len){
 (void)data;
 if(len!=4u)return IAPERROR_FORM;
 if(!s.begun || s.finalized)return IAPERROR_BUSY;
 if(s.written!=s.total || !s.vector_ok)return IAPERROR_SIZE;
 uint32_t stored=REG32(STAGING_BASE+s.total-4u);
 uint32_t calc=~rol16(s.body_sum);
 if(stored!=calc)return IAPERROR_CRC;
 uint32_t sp=REG32(STAGING_BASE),pc=REG32(STAGING_BASE+4u);if((sp&0x2FFE0000u)!=0x20000000u || !(pc&1u) || ((pc&~1u)<APP_BASE) || ((pc&~1u)>=STAGING_BASE))return IAPERROR_FORM;
 if(flash_unlock())return IAPERROR_UNLOCK;
 if(control_write(1u,s.total)){flash_lock();return IAPERROR_WRITEFLASH;}
 flash_lock();
 s.finalized=1;s.begun=0;
 return IAP_OK;
}
uint8_t iap_can_reset(void){return s.finalized;}
