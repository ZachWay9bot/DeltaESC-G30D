#include "protocol.h"
#include "uart.h"
typedef struct{uint8_t st,pos;uint16_t sum,rxcrc;nb_frame_t f;} parser_t;
static parser_t p;
void nb_parser_init(void){p.st=p.pos=0;p.sum=p.rxcrc=0;}
int nb_parser_push(uint8_t b,nb_frame_t*out){switch(p.st){case 0:if(b==0x5A)p.st=1;break;case 1:if(b==0xA5)p.st=2;else p.st=(b==0x5A)?1:0;break;case 2:if(b>NB_MAX_DATA){p.st=0;break;}p.f.len=b;p.sum=b;p.st=3;break;case 3:p.f.src=b;p.sum+=b;p.st=4;break;case 4:p.f.dst=b;p.sum+=b;p.st=5;break;case 5:p.f.cmd=b;p.sum+=b;p.st=6;break;case 6:p.f.index=b;p.sum+=b;p.pos=0;p.st=p.f.len?7:8;break;case 7:p.f.data[p.pos++]=b;p.sum+=b;if(p.pos>=p.f.len)p.st=8;break;case 8:p.rxcrc=b;p.st=9;break;case 9:{p.rxcrc|=(uint16_t)b<<8;uint16_t want=(uint16_t)~p.sum;p.st=0;if(p.rxcrc==want){out->len=p.f.len;out->src=p.f.src;out->dst=p.f.dst;out->cmd=p.f.cmd;out->index=p.f.index;for(uint8_t i=0;i<p.f.len;i++)out->data[i]=p.f.data[i];return 1;}break;}default:p.st=0;break;}return 0;}
void nb_send(uint8_t dst,uint8_t cmd,uint8_t index,const uint8_t*data,uint8_t len){uint16_t sum=(uint16_t)len+0x20u+dst+cmd+index;uart1_putc(0x5A);uart1_putc(0xA5);uart1_putc(len);uart1_putc(0x20);uart1_putc(dst);uart1_putc(cmd);uart1_putc(index);for(uint8_t i=0;i<len;i++){uint8_t v=data?data[i]:0;sum+=v;uart1_putc(v);}uint16_t c=(uint16_t)~sum;uart1_putc((uint8_t)c);uart1_putc((uint8_t)(c>>8));}
void nb_ack(uint8_t dst,uint8_t result){nb_send(dst,0x0Bu,result,0,0);}
