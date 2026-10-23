#include <assert.h>
#include <stdio.h>
#include "drive_runtime.h"
#include "commissioning_guard.h"

int main(void) {
    const uint16_t provisional=COMM_GUARD_CURRENT_SCALE_HW|COMM_GUARD_ADC_TIMING_HW;
    assert(drive_start_blockers(provisional|COMM_GUARD_THROTTLE_NOT_IDLE,1)==0);
    assert(drive_start_blockers(provisional,0)==provisional);
    for(unsigned bit=0;bit<15;bit++) {
        unsigned b=1u<<bit;
        if(!(b&(provisional|COMM_GUARD_THROTTLE_NOT_IDLE)))
            assert(drive_start_blockers((uint16_t)b,1)==b);
    }
    /* Exhaust all combinations of owner and mode booleans. */
    for(unsigned flags=0;flags<128;flags++) {
        unsigned id_allowed=(flags>>0)&1,id_requested=(flags>>1)&1,id_owner=(flags>>2)&1;
        unsigned id_state=(flags>>3)&1,drive_allowed=(flags>>4)&1;
        unsigned powered=(flags>>5)&1,request=(flags>>6)&1;
        unsigned expected=id_owner ? (id_allowed&&id_requested&&id_state&&!powered&&!request) :
                                    (drive_allowed&&powered&&request&&!id_requested);
        assert(drive_armed_owner_valid(id_allowed,id_requested,id_owner,id_state,
                                      drive_allowed,powered,request)==expected);
    }
    for(int ceiling=0;ceiling<=40;ceiling++) {
        int prev=0;
        for(unsigned t=0;t<256;t++) {
            int value=drive_throttle_counts(t,0,3,ceiling);
            assert(value>=prev && value<=12 && value<=ceiling);
            assert(drive_throttle_counts(t,1,3,ceiling)==0);
            if(t<=3)assert(value==0);
            prev=value;
        }
        assert(prev==(ceiling>12?12:ceiling));
    }
    assert(drive_throttle_counts(255,0,255,12)==0);
    assert(drive_speed_kmh10(-400,15,763)==44);
    assert(drive_speed_kmh10(400,15,763)==44);
    assert(drive_speed_kmh10(-400,0,763)==0);
    assert(drive_speed_kmh10(-400,15,0)==0);
    assert(drive_speed_kmh10(-32768,1,1800)==0);
    /* Software-closure boundary coverage. Physical wheel/motor calibration
     * remains a separate hardware qualification. */
    assert(drive_speed_kmh10(0,15,763)==0);
    assert(drive_speed_kmh10(-1024,15,763)>0);
    assert(drive_speed_kmh10(1024,15,763)==drive_speed_kmh10(-1024,15,763));
    assert(drive_speed_kmh10(1025,15,763)==0);
    assert(drive_speed_kmh10(400,15,1800)>0);
    assert(drive_speed_kmh10(400,15,1801)==0);
    assert(drive_throttle_counts(255,0,3,12)==12);
    assert(drive_throttle_counts(255,1,3,12)==0);
    puts("PASS: software closure: qualifiers, owner truth table, throttle/brake and speed domain edges");
    return 0;
}
