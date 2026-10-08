#include "iap_control.h"
#include <stdint.h>
#include <stdio.h>

static uint16_t flash_words[6];
static uint8_t fail_erase;
static int fail_before_write, writes;
static int bad;

static void erased(void) { for (unsigned i=0; i<6; ++i) flash_words[i]=0xffff; }
static uint32_t read_word(uint32_t addr) {
    if (addr < IAP_CONTROL_BASE || addr > IAP_CONTROL_BASE+8u || (addr&3u)) {bad=1;return 0;}
    unsigned i=(unsigned)(addr-IAP_CONTROL_BASE)/2u;
    return (uint32_t)flash_words[i] | ((uint32_t)flash_words[i+1] << 16);
}
static uint8_t erase_page(uint32_t addr) {
    if(addr!=IAP_CONTROL_BASE) {bad=1;return 0;}
    if(fail_erase) return 0;
    erased();return 1;
}
static uint8_t write_half(uint32_t addr,uint16_t val) {
    if(addr<IAP_CONTROL_BASE || addr>=IAP_CONTROL_BASE+12u || (addr&1u)) {bad=1;return 0;}
    if(writes++==fail_before_write) return 0;
    unsigned i=(unsigned)(addr-IAP_CONTROL_BASE)/2u;
    if((flash_words[i] & val)!=val){bad=1;return 0;}
    flash_words[i]&=val;
    return 1;
}
static int valid_marker(void) {return read_word(IAP_CONTROL_BASE)==IAP_CONTROL_MAGIC &&
    read_word(IAP_CONTROL_BASE+4u)==1u && read_word(IAP_CONTROL_BASE+8u)==10712u;}

int main(void) {
    const uint32_t size=10712u;
    erased();flash_words[0]=0x505a; flash_words[1]=0;flash_words[2]=1;flash_words[3]=0;flash_words[4]=(uint16_t)size;flash_words[5]=0;
    if(!valid_marker()) return 1;
    if(!iap_control_invalidate(erase_page,read_word) || valid_marker()) return 2;

    for(int cut=0;cut<6;++cut){
        erased();writes=0;fail_before_write=cut;
        uint8_t ok=iap_control_commit(size,write_half,read_word);
        if(ok || valid_marker() || read_word(IAP_CONTROL_BASE)==IAP_CONTROL_MAGIC) {
            fprintf(stderr,"power cut %d left pending update\n",cut);return 3;
        }
    }
    erased();writes=0;fail_before_write=-1;
    if(!iap_control_commit(size,write_half,read_word)||!valid_marker()||writes!=6) return 4;
    writes=0;
    if(iap_control_commit(size,write_half,read_word) || writes) return 5;
    fail_erase=1;
    if(iap_control_invalidate(erase_page,read_word)) return 6;
    if(bad) return 7;
    erased();writes=0;fail_before_write=-1;
    if(iap_control_commit(IAP_APPLICATION_MAX_BYTES+1u,write_half,read_word) || writes) return 8;
    if(read_word(IAP_CONTROL_BASE)!=0xffffffffu) return 9;
    puts("IAP staged update power-loss control tests: PASS (6 cut points)");
    return 0;
}
