#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "iap_proto.h"
int main(void){
    uint8_t b[128],take=0; memset(b,0,sizeof(b));
    for(unsigned i=0;i<80;i++) b[i]=(uint8_t)(i+1u);
    if(!iap_proto_effective_write_len(10192u,79u*128u,b,128u,&take)||take!=80u) return 1;
    b[127]=1u; if(iap_proto_effective_write_len(10192u,79u*128u,b,128u,&take)) return 2;
    b[127]=0u; if(!iap_proto_effective_write_len(256u,0u,b,128u,&take)||take!=128u) return 3;
    if(iap_proto_next_index(0xFEu)!=0xFFu||iap_proto_next_index(0xFFu)!=0x00u) return 4;
    const uint8_t crcvec[]={1u,2u,3u,0xFEu};
    uint32_t sum=iap_proto_sum32_update(0u,crcvec,(uint8_t)sizeof(crcvec));
    if(iap_proto_sum32_finish(sum)!=(uint32_t)~0x104u) return 5;
    puts("stock 128-byte IAP transport + checksum vector: PASS"); return 0;
}
