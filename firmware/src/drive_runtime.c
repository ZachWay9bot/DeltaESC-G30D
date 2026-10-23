#include "drive_runtime.h"
#include "commissioning_guard.h"

uint16_t drive_start_blockers(uint16_t mask, uint8_t rolling_rc) {
    uint16_t except=COMM_GUARD_THROTTLE_NOT_IDLE;
    /* Same explicit low-current RC exception already used by motor_hw_arm.
     * Never pretend the physical current scale/timing were qualified. */
    if(rolling_rc)except|=COMM_GUARD_CURRENT_SCALE_HW|COMM_GUARD_ADC_TIMING_HW;
    return (uint16_t)(mask & (uint16_t)~except);
}

uint8_t drive_armed_owner_valid(uint8_t id_allowed,uint8_t id_requested,
    uint8_t id_owner,uint8_t id_state_valid,uint8_t drive_allowed,
    uint8_t powered,uint8_t request) {
    if(id_owner)return (uint8_t)(id_allowed && id_requested && id_state_valid && !powered && !request);
    return (uint8_t)(drive_allowed && powered && request && !id_requested);
}

int16_t drive_throttle_counts(uint8_t throttle,uint8_t brake,
                             uint8_t idle_max,int16_t ceiling) {
    if(brake || throttle<=idle_max || idle_max==255u || ceiling<=0)return 0;
    /* Keep the existing first-RC torque envelope. Larger vehicle currents
     * require measurements, not a larger number hidden in the input code. */
    if(ceiling>12)ceiling=12;
    return (int16_t)(((uint32_t)(throttle-idle_max)*(uint16_t)ceiling)/(255u-idle_max));
}

uint16_t drive_speed_kmh10(int16_t step,uint8_t pp,uint16_t circ) {
    int32_t magnitude=step;
    if(magnitude<0)magnitude=-magnitude;
    if(!pp || !circ || circ>1800u || magnitude>1024)return 0u;
    /* Actual current loop is 4 kHz. Keep arithmetic inside 32 bits and
     * avoid introducing a Cortex-M3 64-bit divide runtime dependency. */
    const uint32_t electrical_millihz=((uint32_t)magnitude*4000000u)/65536u;
    const uint32_t mm_per_second=((electrical_millihz/pp)*circ)/1000u;
    return (uint16_t)((mm_per_second*36u)/1000u);
}
