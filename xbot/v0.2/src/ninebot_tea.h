#pragma once
#include <stdint.h>
typedef struct{uint8_t key[16];uint8_t iv[8];uint32_t offset;} ninebot_tea_t;
void ninebot_tea_init(ninebot_tea_t*s);
void ninebot_tea_decrypt_block(ninebot_tea_t*s,const uint8_t in[8],uint8_t out[8]);
