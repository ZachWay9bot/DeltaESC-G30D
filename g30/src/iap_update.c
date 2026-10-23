#include "iap_update.h"
#include "iap_proto.h"
#include "iap_control.h"
#include "stm32f103_min.h"

#define IAP_HDR0 0x5Au
#define IAP_HDR1 0xA5u
#define IAP_DST_ESC 0x20u
#define IAP_CMD_BEGIN 0x07u
#define IAP_CMD_WRITE 0x08u
#define IAP_CMD_CRC   0x09u
#define IAP_CMD_RESET 0x0Au

#define STAGE_BASE   0x0800E800u
#define STAGE_LIMIT  0x0801C000u
#define FLASH_PAGE 1024u
#define MAX_RX_DATA 128u

#define ERR_NONE 0u
#define ERR_FRAME 1u
#define ERR_SIZE 2u
#define ERR_FLASH 3u
#define ERR_ORDER 4u
#define ERR_CRC 5u
#define ERR_IMAGE 6u
#define ERR_BUSY 7u

static volatile uint8_t q_ready;
static volatile uint8_t q_len,q_src,q_dst,q_cmd,q_index;
static volatile uint8_t q_data[MAX_RX_DATA];
static volatile uint8_t q_data_len;

typedef struct { uint8_t state,pos,len; uint8_t buf[MAX_RX_DATA+7u]; } parser_t;
static parser_t p;

static volatile uint8_t st,last_err,mode;
static volatile uint32_t expected,received,staged;
static uint8_t expected_index;
/* Cache only the most recently accepted wire block. A lost ACK may cause
   the sender to retransmit it; never decrypt or program that block twice. */
static uint8_t last_write_valid,last_write_len;
static uint8_t last_write_data[MAX_RX_DATA];
static uint32_t plain_sum,wire_sum;
static uint32_t enc_iv_lo,enc_iv_hi,enc_processed;
static uint32_t tea_key[4];
static uint8_t first_block;
static uint8_t tail[8];
static uint8_t tail_len;

static uint32_t rd32(const uint8_t *b){return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static void wr32(uint8_t *b,uint32_t v){b[0]=(uint8_t)v;b[1]=(uint8_t)(v>>8);b[2]=(uint8_t)(v>>16);b[3]=(uint8_t)(v>>24);}
static uint8_t valid_vector(const uint8_t *b){uint32_t sp=rd32(b),r=rd32(b+4);return sp>=0x20000000u&&sp<=0x20005000u&&(r&1u)&&((r&~1u)>=0x08001000u)&&((r&~1u)<0x0800E000u);}

static uint8_t flash_wait(void){uint32_t n=3000000u;while(FLASH_SR_REG&FLASH_SR_BSY){if(!n--)return 0u;}return 1u;}
static uint8_t flash_unlock(void){if(FLASH_CR_REG&FLASH_CR_LOCK){FLASH_KEYR=FLASH_KEY1;FLASH_KEYR=FLASH_KEY2;}return (FLASH_CR_REG&FLASH_CR_LOCK)?0u:1u;}
static void flash_lock(void){FLASH_CR_REG|=FLASH_CR_LOCK;}
static uint8_t flash_erase_page(uint32_t a){
    if(!flash_wait()||!flash_unlock())return 0u;
    FLASH_SR_REG=FLASH_SR_EOP|FLASH_SR_PGERR|FLASH_SR_WRPRTERR;
    FLASH_CR_REG|=FLASH_CR_PER; FLASH_AR_REG=a; FLASH_CR_REG|=FLASH_CR_STRT;
    uint8_t ok=flash_wait(); FLASH_CR_REG&=~FLASH_CR_PER;
    if(FLASH_SR_REG&(FLASH_SR_PGERR|FLASH_SR_WRPRTERR))ok=0u; flash_lock(); return ok;
}
static uint8_t flash_half(uint32_t a,uint16_t v){
    if(!flash_wait()||!flash_unlock())return 0u;
    FLASH_SR_REG=FLASH_SR_EOP|FLASH_SR_PGERR|FLASH_SR_WRPRTERR; FLASH_CR_REG|=FLASH_CR_PG; REG16(a)=v;
    uint8_t ok=flash_wait(); FLASH_CR_REG&=~FLASH_CR_PG;
    if(FLASH_SR_REG&(FLASH_SR_PGERR|FLASH_SR_WRPRTERR))ok=0u; flash_lock(); return ok&&REG16(a)==v;
}
static uint8_t flash_bytes(uint32_t a,const uint8_t *b,uint8_t n){
    for(uint8_t i=0;i<n;i+=2u){uint16_t v=b[i];if(i+1u<n)v|=(uint16_t)b[i+1u]<<8;else v|=0xFF00u;if(!flash_half(a+i,v))return 0u;}return 1u;
}
static uint8_t erase_stage(void){for(uint32_t a=STAGE_BASE&~(FLASH_PAGE-1u);a<STAGE_LIMIT;a+=FLASH_PAGE)if(!flash_erase_page(a))return 0u;return 1u;}
static uint32_t control_read_word(uint32_t addr){return REG32(addr);}
static uint8_t commit_control(uint32_t size){
    return iap_control_commit(size,flash_half,control_read_word);
}

static void key_reset(void){tea_key[0]=0xB21C80FEu;tea_key[1]=0xA641EFD1u;tea_key[2]=0xF53117A4u;tea_key[3]=0xF02468A0u;enc_iv_lo=enc_iv_hi=enc_processed=0u;}
static void key_update(void){uint8_t r[16];for(uint8_t w=0;w<4;w++)wr32(r+4u*w,tea_key[w]);for(uint8_t i=0;i<16;i++)r[i]=(uint8_t)(r[i]+i);for(uint8_t w=0;w<4;w++)tea_key[w]=rd32(r+4u*w);}
static void tea_dec(uint32_t *y,uint32_t *z){uint32_t sum=0xC6EF3720u;for(uint8_t i=0;i<32;i++){*z-=(((*y<<4)+tea_key[2])^(*y+sum)^((*y>>5)+tea_key[3]));*y-=(((*z<<4)+tea_key[0])^(*z+sum)^((*z>>5)+tea_key[1]));sum-=0x9E3779B9u;}}
static void decrypt8(const uint8_t *in,uint8_t *out){
    if(enc_processed==1024u){key_update();enc_processed=0u;}
    uint32_t cy=rd32(in),cz=rd32(in+4),y=cy,z=cz;tea_dec(&y,&z);y^=enc_iv_lo;z^=enc_iv_hi;enc_iv_lo=cy;enc_iv_hi=cz;enc_processed+=8u;wr32(out,y);wr32(out+4,z);
}
static uint32_t tea_checksum_words(uint32_t upto){uint32_t s=0u;for(uint32_t a=STAGE_BASE;a<STAGE_BASE+upto;a+=4u)s+=REG32(a);return (((s>>16)&0xFFFFu)|((s&0xFFFFu)<<16))^0xFFFFFFFFu;}

static void send_ack(uint8_t dst,uint8_t result){
    uint8_t b[IAP_ACK_FRAME_LEN];iap_proto_build_ack(dst,result,b);
    uint32_t cr1=USART_CR1_REG(USART2_BASE);USART_CR1_REG(USART2_BASE)=cr1&~(USART_CR1_RXNEIE|USART_CR1_RE);USART_CR1_REG(USART2_BASE)|=USART_CR1_TE;
    for(uint8_t i=0;i<IAP_ACK_FRAME_LEN;i++){while(!(USART_SR_REG(USART2_BASE)&USART_SR_TXE)){}USART_DR_REG(USART2_BASE)=b[i];}while(!(USART_SR_REG(USART2_BASE)&USART_SR_TC)){}USART_CR1_REG(USART2_BASE)&=~USART_CR1_TE;USART_CR1_REG(USART2_BASE)|=USART_CR1_RE|USART_CR1_RXNEIE;
}
static void fail(uint8_t e,uint8_t src,uint8_t cmd){(void)cmd;last_err=e;st=IAP_ERROR;send_ack(src,e);}

void iap_update_init(void){st=IAP_IDLE;last_err=0u;q_ready=0u;p.state=0u;last_write_valid=0u;}
uint8_t iap_update_active(void){return st!=IAP_IDLE&&st!=IAP_ERROR;}
uint8_t iap_update_pending(void){return q_ready;}
uint8_t iap_update_state(void){return st;}
uint8_t iap_update_last_error(void){return last_err;}
uint32_t iap_update_expected_bytes(void){return expected;}
uint32_t iap_update_received_bytes(void){return received;}
uint32_t iap_update_staged_bytes(void){return staged;}

void iap_update_feed(uint8_t b){
    if(p.state==0u){if(b==IAP_HDR0)p.state=1u;return;}if(p.state==1u){if(b==IAP_HDR1){p.state=2u;p.pos=0u;}else if(b!=IAP_HDR0)p.state=0u;return;}
    if(p.pos>=sizeof(p.buf)){p.state=0u;return;}p.buf[p.pos++]=b;
    if(p.pos==1u){p.len=b;if(p.len>MAX_RX_DATA){p.state=0u;return;}}
    uint8_t total=(uint8_t)(p.len+7u);if(p.pos<total)return;
    uint16_t sum=0u;for(uint8_t i=0;i<(uint8_t)(total-2u);i++)sum=(uint16_t)(sum+p.buf[i]);uint16_t want=(uint16_t)~sum;uint16_t got=(uint16_t)p.buf[total-2u]|((uint16_t)p.buf[total-1u]<<8);
    if(got==want&&!q_ready&&p.buf[2]==IAP_DST_ESC&&p.buf[3]>=IAP_CMD_BEGIN&&p.buf[3]<=IAP_CMD_RESET){q_len=p.buf[0];q_src=p.buf[1];q_dst=p.buf[2];q_cmd=p.buf[3];q_index=p.buf[4];q_data_len=p.len;for(uint8_t i=0;i<q_data_len;i++)q_data[i]=p.buf[5u+i];q_ready=1u;}
    p.state=0u;p.pos=0u;
}

static uint8_t stage_plain(const uint8_t *b,uint8_t n){if(staged+(uint32_t)n>(STAGE_LIMIT-STAGE_BASE))return 0u;if(!flash_bytes(STAGE_BASE+staged,b,n))return 0u;for(uint8_t i=0;i<n;i++)plain_sum+=b[i];staged+=n;return 1u;}
static uint8_t stage_encrypted(const uint8_t *b,uint8_t n){
    for(uint8_t i=0;i<n;i++){tail[tail_len++]=b[i];if(tail_len==8u){uint8_t o[8];decrypt8(tail,o);if(!stage_plain(o,8u))return 0u;tail_len=0u;}}
    return 1u;
}
static void handle_begin(uint8_t src,const uint8_t *d,uint8_t n){
    if(n!=4u){fail(ERR_FRAME,src,IAP_CMD_BEGIN);return;}uint32_t sz=rd32(d);if(sz<256u||sz>(STAGE_LIMIT-STAGE_BASE)||((sz&7u)!=0u)){fail(ERR_SIZE,src,IAP_CMD_BEGIN);return;}
    /* Clear a previous pending-update marker before touching staged bytes.
       Without this, a power cut while restaging could boot a partial image. */
    if(!iap_control_invalidate(flash_erase_page,control_read_word)){
        fail(ERR_FLASH,src,IAP_CMD_BEGIN);return;
    }
    if(!erase_stage()){fail(ERR_FLASH,src,IAP_CMD_BEGIN);return;}expected=sz;received=staged=plain_sum=wire_sum=0u;expected_index=0u;last_write_valid=0u;mode=0u;first_block=1u;tail_len=0u;key_reset();last_err=0u;st=IAP_READY;send_ack(src,0u);
}
static void handle_write(uint8_t src,uint8_t idx,const uint8_t *d,uint8_t n){
    if(st!=IAP_READY&&st!=IAP_RECEIVING){fail(ERR_ORDER,src,IAP_CMD_WRITE);return;}
    if(last_write_valid&&idx==(uint8_t)(expected_index-1u)&&n==last_write_len){
        uint8_t same=1u;for(uint8_t i=0u;i<n;i++)if(d[i]!=last_write_data[i]){same=0u;break;}
        if(same){send_ack(src,ERR_NONE);return;}
    }
    if(idx!=expected_index){fail(ERR_ORDER,src,IAP_CMD_WRITE);return;}
    uint8_t take=0u;
    if(!iap_proto_effective_write_len(expected,received,d,n,&take)){fail(ERR_SIZE,src,IAP_CMD_WRITE);return;}
    if(first_block){if(take<8u){fail(ERR_FRAME,src,IAP_CMD_WRITE);return;}if(valid_vector(d))mode=1u;else{uint8_t o[8];decrypt8(d,o);key_reset();if(!valid_vector(o)){fail(ERR_IMAGE,src,IAP_CMD_WRITE);return;}mode=2u;}first_block=0u;}
    if(mode==2u && (take&7u)){fail(ERR_SIZE,src,IAP_CMD_WRITE);return;}
    wire_sum=iap_proto_sum32_update(wire_sum,d,take);
    uint8_t ok=(mode==1u)?stage_plain(d,take):stage_encrypted(d,take);if(!ok){fail(ERR_FLASH,src,IAP_CMD_WRITE);return;}
    received+=take;expected_index=iap_proto_next_index(expected_index);st=IAP_RECEIVING;
    for(uint8_t i=0u;i<n;i++)last_write_data[i]=d[i];last_write_len=n;last_write_valid=1u;
    send_ack(src,0u);
}
static void handle_crc(uint8_t src,const uint8_t *d,uint8_t n){
    /* CRC ACK can also be lost after committing the marker. Re-ACK only
       the exact checksum already validated; do not rewrite control flash. */
    if(st==IAP_COMPLETE&&n==4u&&rd32(d)==iap_proto_sum32_finish(wire_sum)){
        send_ack(src,ERR_NONE);return;
    }
    if(st!=IAP_RECEIVING||received!=expected||tail_len!=0u||n!=4u){fail(ERR_ORDER,src,IAP_CMD_CRC);return;}
    uint32_t supplied=rd32(d);uint8_t ok=0u;uint32_t final_size=staged;
    uint8_t wire_ok=(uint8_t)(iap_proto_sum32_finish(wire_sum)==supplied);
    if(mode==1u){uint32_t calc=~plain_sum;ok=(uint8_t)(wire_ok&&(calc==supplied));}
    else {if(staged>=4u){uint32_t internal=REG32(STAGE_BASE+staged-4u);uint32_t calc=tea_checksum_words(staged-4u);ok=(uint8_t)(wire_ok&&(internal==calc));final_size=staged-4u;}}
    if(!ok){fail(ERR_CRC,src,IAP_CMD_CRC);return;}if(!valid_vector((const uint8_t *)STAGE_BASE)){fail(ERR_IMAGE,src,IAP_CMD_CRC);return;}if(!commit_control(final_size)){fail(ERR_FLASH,src,IAP_CMD_CRC);return;}st=IAP_COMPLETE;send_ack(src,0u);
}
static void reset_now(void){__asm volatile("dsb":::"memory");SCB_AIRCR=SCB_AIRCR_VECTKEY|SCB_AIRCR_SYSRESETREQ;for(;;){}}
void iap_update_poll(void){
    if(!q_ready)return;irq_disable();uint8_t len=q_len,src=q_src,cmd=q_cmd,idx=q_index,n=q_data_len;uint8_t d[MAX_RX_DATA];for(uint8_t i=0;i<n;i++)d[i]=q_data[i];q_ready=0u;irq_enable();(void)len;
    if(cmd==IAP_CMD_BEGIN)handle_begin(src,d,n);else if(cmd==IAP_CMD_WRITE)handle_write(src,idx,d,n);else if(cmd==IAP_CMD_CRC)handle_crc(src,d,n);else if(cmd==IAP_CMD_RESET){if(st!=IAP_COMPLETE){fail(ERR_ORDER,src,cmd);return;}send_ack(src,0u);for(volatile uint32_t x=0;x<200000u;x++){}reset_now();}
}
