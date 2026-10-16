#pragma once
#include <stdint.h>
#define NB_MAX_DATA 128u
typedef struct{uint8_t len,src,dst,cmd,index,data[NB_MAX_DATA];} nb_frame_t;
void nb_parser_init(void);
int nb_parser_push(uint8_t b,nb_frame_t*out);
void nb_send(uint8_t dst,uint8_t cmd,uint8_t index,const uint8_t*data,uint8_t len);
void nb_ack(uint8_t dst,uint8_t result);
