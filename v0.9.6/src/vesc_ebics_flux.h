/*
 * VESC Ortega flux-observer model adapted to STM32F103 fixed-point arithmetic.
 * Algorithm reference: vedderb/bldc motor/foc_math.c foc_observer_update,
 * EBiCS/EBiCS_Firmware Sensorless_VESC Src/FOC.c observer_update.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 DeltaESC contributors; derived algorithm credit Benjamin Vedder.
 * This module does not touch timer registers or release gate outputs.
 */
#pragma once
#include <stdint.h>
typedef struct {
    int32_t x_alpha_nwb;
    int32_t x_beta_nwb;
    int32_t flux_alpha_nwb;
    int32_t flux_beta_nwb;
    uint16_t samples;
    uint8_t valid;
} vesc_ebics_flux_t;
void vesc_ebics_flux_reset(vesc_ebics_flux_t *s);
/* Inputs are phase-neutral commanded voltages (mV), alpha/beta current (mA),
 *  phase resistance uOhm, inductance nH, magnet flux uWb, dt = 1/4000s.
 * Output residual rotor-flux vector (nWb). No phase offset or atan2 here.
 * Returns 0 for out-of-range parameters/input or observer divergence.
 */
uint8_t vesc_ebics_flux_step(vesc_ebics_flux_t *s,
        int32_t va_mv, int32_t vb_mv, int32_t ia_ma, int32_t ib_ma,
        uint32_t r_uohm, uint32_t l_nh, uint32_t flux_uwb);