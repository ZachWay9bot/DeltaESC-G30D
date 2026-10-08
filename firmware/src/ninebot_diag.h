#pragma once
#include <stdint.h>

#define DELTAESC_CMD_CONFIG 0x7Du
#define DELTAESC_ARG_HELLO  0x00u
#define DELTAESC_ARG_DIAG   0x20u

void ninebot_diag_init(void);
void ninebot_diag_poll(void);
