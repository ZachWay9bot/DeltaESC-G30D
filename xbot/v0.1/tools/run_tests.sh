#!/usr/bin/env bash
set -euo pipefail
cc -std=c11 -O2 -Wall -Wextra -Werror -Isrc src/bemf_detector.c tools/bemf_detector_host_test.c -o /tmp/xbot_bemf_test
/tmp/xbot_bemf_test
! grep -R --line-number -E 'TIM_BDTR\([^)]*\).*\||TIM_CCER\([^)]*\).*\|' src/main.c
! grep -R --line-number 'gpio_cfg_nibble(GPIOB_BASE,1,0x[2Bb])' src/main.c
grep -Fq 'gpio_cfg_nibble(GPIOB_BASE,1,0x0u)' src/main.c
grep -Fq 'RCC_APB2RSTR|=(1u<<11)' src/main.c
echo 'PASS: passive-only source invariants'
