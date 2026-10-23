#pragma once
/* Fill only from the owner's measured G30 target. Zeros mean unknown.
 * A built-in profile avoids new persistent/config-flash writes and allows
 * standalone operation after calibration. RAM F0-F5/FF remains available.
 * No value in this file controls IAP or rollback eligibility. */
#ifndef G30_PROFILE_R_UOHM
#define G30_PROFILE_R_UOHM 0u
#endif
#ifndef G30_PROFILE_L_NH
#define G30_PROFILE_L_NH 0u
#endif
#ifndef G30_PROFILE_FLUX_UWB
#define G30_PROFILE_FLUX_UWB 0u
#endif
#ifndef G30_PROFILE_POLE_PAIRS
#define G30_PROFILE_POLE_PAIRS 0u
#endif
#ifndef G30_PROFILE_CIRCUMFERENCE_MM
#define G30_PROFILE_CIRCUMFERENCE_MM 0u
#endif
#ifndef G30_PROFILE_PHASE_OFFSET_U16
#define G30_PROFILE_PHASE_OFFSET_U16 0
#endif
#ifndef G30_PROFILE_CURRENT_MA
#define G30_PROFILE_CURRENT_MA 500u /* existing nominal bench ceiling */
#endif
