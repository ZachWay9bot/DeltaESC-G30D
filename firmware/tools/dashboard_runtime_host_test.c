#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "dashboard_runtime.h"

int main(void) {
    ninebot_frame_t f;
    memset(&f,0,sizeof(f));
    f.format=NINEBOT_FMT_5AA5;
    f.src=G30_DASH_ADDR;
    f.dst=G30_ESC_ADDR;
    f.cmd=G30_DASH_CMD_ADC;
    f.payload_len=3u;
    f.payload[0]=0x00u;
    f.payload[1]=0x7Bu;
    f.payload[2]=0x2Au;
    uint8_t t=0u,b=0u;
    assert(dashboard_decode_adc(&f,&t,&b)==1u);
    assert(t==0x7Bu && b==0x2Au);

    f.cmd=G30_DASH_CMD_STATUS;
    assert(dashboard_is_status_request(&f)==1u);
    f.src=0x3Eu;
    assert(dashboard_is_status_request(&f)==0u);
    f.src=G30_DASH_ADDR;
    f.format=NINEBOT_FMT_55AA;
    assert(dashboard_is_status_request(&f)==0u);

    uint8_t p[G30_DASH_STATUS_BYTES]={0};
    dashboard_safe_status_payload(p);
    const uint8_t want[6]={0x01u,50u,0u,0u,0u,0u};
    assert(memcmp(p,want,sizeof(want))==0);
    dashboard_drive_status_payload(p,237u,67u,1u);
    assert(p[0]==1u && p[1]==67u && p[4]==24u && p[5]==1u);
    dashboard_drive_status_payload(p,65535u,255u,0u);
    assert(p[1]==100u && p[4]==255u && p[5]==0u);
    return 0;
}
