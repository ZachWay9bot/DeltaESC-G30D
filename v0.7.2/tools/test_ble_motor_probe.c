#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "motor_probe.h"
#include "ble_motor_probe.h"
static uint16_t u16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t u32(const uint8_t *p){return (uint32_t)u16(p)|((uint32_t)u16(p+2)<<16);}
#define CHECK(X) do{if(!(X)){printf("FAIL line %d: %s\n",__LINE__,#X);return 1;}}while(0)
int main(void){
    uint8_t p[16];
    volatile uint16_t off[3]={1998u,2098u,2198u};
    motor_probe_init();
    CHECK(ble_motor_probe_read(0xDAu,p)==16u);
    CHECK(u32(p)==MOTOR_PROBE_MAGIC&&u32(p+8)==0u);
    CHECK(ble_motor_probe_read(0xDFu,p)==16u&&u32(p+12)==0x00010000u);
    for(unsigned i=0u;i<256u;i++){
        uint16_t d=(uint16_t)(i&3u);
        motor_probe_capture((uint16_t)(2000u+d),(uint16_t)(2100u+d),
                            (uint16_t)(2200u+d),(uint16_t)(1500u+d),
                            off,501u,3900u,4100u,110u,0u,
                            0x12345678u,0xabcdef01u,0x1000u,0x8877u);
    }
    CHECK(ble_motor_probe_read(0xDAu,p)==16u&&u32(p)==MOTOR_PROBE_MAGIC);
    CHECK(u32(p+4)==2u&&u32(p+8)==1u&&u32(p+12)==501u);
    CHECK(ble_motor_probe_read(0xDBu,p)==16u);
    CHECK(u16(p)==2001u&&u16(p+2)==2101u&&u16(p+6)==1501u);
    CHECK(u16(p+8)==2000u&&u16(p+10)==2100u&&u16(p+14)==1500u);
    CHECK(ble_motor_probe_read(0xDCu,p)==16u);
    CHECK(u16(p)==2003u&&u16(p+6)==1503u);
    CHECK(u16(p+8)==2003u&&u16(p+14)==1503u);
    CHECK(ble_motor_probe_read(0xDDu,p)==16u);
    CHECK(u16(p)==1998u&&u16(p+2)==2098u&&u16(p+4)==2198u);
    CHECK(u16(p+6)==256u&&u32(p+8)==0u&&u32(p+12)==256u);
    CHECK(ble_motor_probe_read(0xDEu,p)==16u);
    CHECK(u32(p)==3900u&&u32(p+4)==4100u&&u32(p+8)==110u&&u32(p+12)==0x1000u);
    CHECK(ble_motor_probe_read(0xDFu,p)==16u);
    CHECK(u32(p)==0x12345678u&&u32(p+4)==0xabcdef01u&&u32(p+8)==0x8877u);
    CHECK(u32(p+12)==0x00010001u);
    CHECK(ble_motor_probe_read(0xD9u,p)==0u&&ble_motor_probe_read(0xF0u,p)==0u);
    g_motor_probe.sequence=3u;
    CHECK(ble_motor_probe_read(0xDAu,p)==0u);
    g_motor_probe.sequence=4u;
    CHECK(ble_motor_probe_read(0xDAu,p)==16u&&u32(p+4)==4u);
    CHECK(ble_motor_probe_read(0xDAu,0)==0u);
    puts("PASS BLE DA-DF snapshot serialization, consistency and read-only page set");
    return 0;
}
