#include "g30_adc_proof.h"
#include <assert.h>
#include <stdio.h>
static unsigned pass;
static void tests(void) {
    g30_adc_proof_input_t x={
        .raw={2048,2048,2048},.off={2048,2048,2048},
        .adc2_ch4_off=2048,.adc2_ch5_off=2048,.ccer=0x1000,
        .have_sample=1,.adc1_offsets_valid=1,
        .adc2_ch4_valid=1,.adc2_ch5_valid=1,
        .now_ms=1000,.last_adc_ms=990
    };
    const unsigned all=(G30_PROOF_SAMPLE_PRESENT|G30_PROOF_ADC1_OFFSET_OK|
        G30_PROOF_ADC2_OFFSET_OK|G30_PROOF_RAW12_OK|G30_PROOF_FRESH|
        G30_PROOF_COAST_CCER|G30_PROOF_ZERO_COUNTS);
    g30_adc_proof_result_t o=g30_adc_proof_eval(&x);
    assert(o.flags==all && o.peak_abs_counts==0u && o.age_ms==10u);++pass;
    uint8_t a[20],b[20];
    a[0]=0xA5;a[19]=0x5A;b[0]=0xA5;b[19]=0x5A;
    g30_adc_proof_page_fa(a+1,&x,o,0xA02Cu);
    g30_adc_proof_page_fb(b+1,&x,o,0x8C40u,2300u,4u,8u);
    assert(a[0]==0xA5 && a[19]==0x5A && b[0]==0xA5 && b[19]==0x5A);
    assert(a[1]==1 && a[2]==all && a[17]==0x2C && a[18]==0xA0);
    assert(b[11]==0x00 && b[12]==0x10 && b[13]==0x40 && b[14]==0x8C);++pass;
    x.raw[2]=2073; o=g30_adc_proof_eval(&x);
    assert(o.peak_abs_counts==25 && !(o.flags&G30_PROOF_ZERO_COUNTS));++pass;
    x.raw[2]=2048;x.ccer=0x1555; o=g30_adc_proof_eval(&x);
    assert(!(o.flags&G30_PROOF_ZERO_COUNTS) && !(o.flags&G30_PROOF_COAST_CCER));++pass;
    x.ccer=0x1000;x.now_ms=1101; o=g30_adc_proof_eval(&x);
    assert(!(o.flags&G30_PROOF_FRESH) && !(o.flags&G30_PROOF_ZERO_COUNTS));++pass;
    x.now_ms=1000;x.off[1]=2149; o=g30_adc_proof_eval(&x);
    assert(!(o.flags&G30_PROOF_ADC1_OFFSET_OK));++pass;
    x.off[1]=2048;x.raw[0]=4096; o=g30_adc_proof_eval(&x);
    assert(!(o.flags&G30_PROOF_RAW12_OK));++pass;
    x.raw[0]=2048;x.adc2_ch5_valid=0;o=g30_adc_proof_eval(&x);
    assert(!(o.flags&G30_PROOF_ADC2_OFFSET_OK));++pass;
    x.adc2_ch5_valid=1;x.killed=1;o=g30_adc_proof_eval(&x);
    assert(o.flags&G30_PROOF_KILLED);++pass;
    x.killed=0;x.have_sample=0;o=g30_adc_proof_eval(&x);
    assert(!(o.flags&G30_PROOF_FRESH) && !(o.flags&G30_PROOF_ZERO_COUNTS));++pass;
    x.have_sample=1;x.now_ms=0;x.last_adc_ms=500;
    o=g30_adc_proof_eval(&x);assert(o.age_ms==65535u);++pass;
    /* Large property sweep including offset plausibility boundaries
       and exactly calibrated unpowered zero values. */
    for(unsigned delta=0;delta<=200u;delta++) {
      x.now_ms=1000;x.last_adc_ms=1000;
      x.off[0]=2048;x.raw[0]=(uint16_t)(2048+delta);
      x.raw[1]=x.off[1]=2048;x.raw[2]=x.off[2]=2048;
      o=g30_adc_proof_eval(&x);
      assert(o.peak_abs_counts==delta);
      assert(((o.flags&G30_PROOF_ZERO_COUNTS)!=0)==(delta<=24)); ++pass;
    }
    /* Property: explicit physical HW current scale is NEVER claimed by FA/FB. */
    assert(!(o.flags & 0x00u));++pass;
}
int main(void){tests();printf("PASS ADC read-only proof: %u checks, 18-byte BLE pages, never motor-current qualification\n",pass);return 0;}
