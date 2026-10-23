#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "iap_proto.h"

static int check(uint8_t dst, uint8_t result, const uint8_t exp[9]) {
    uint8_t got[9];
    iap_proto_build_ack(dst, result, got);
    if (memcmp(got, exp, 9) != 0) {
        for (unsigned i=0;i<9;i++) fprintf(stderr, "%02X%s", got[i], i==8?"\n":" ");
        return 1;
    }
    return 0;
}
int main(void) {
    const uint8_t ok[9]  = {0x5A,0xA5,0x00,0x20,0x3E,0x0B,0x00,0x96,0xFF};
    const uint8_t err[9] = {0x5A,0xA5,0x00,0x20,0x3E,0x0B,0x03,0x93,0xFF};
    if (check(0x3E,0x00,ok) || check(0x3E,0x03,err)) return 1;
    puts("stock DRV126 IAP ACK vector: PASS");
    return 0;
}
