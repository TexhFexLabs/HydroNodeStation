#ifndef POWER_POLICY_H
#define POWER_POLICY_H
#include <stdint.h>
#include <stdbool.h>
/* Battery thresholds in pack mV (1S LiPo: pack = cell). Defaults for the
 * station's LiPo; changeable by downlink 0x14 (firmware 2.1) and kept in NVM.
 *   NORMAL -> SAVE      below save, back at save + POWER_SAVE_EXIT_MV
 *   -> RECOVERY         below recovery (or 3 invalid readings), radio off
 *   RECOVERY -> Standby two valid readings below standby in a row
 *   RECOVERY -> running resume or more, stable for POWER_RESTART_STABLE_S
 * Validate against the actual cell and its protection circuit. */
typedef struct { uint16_t save, recovery, standby, resume; } power_thresholds_t;
#define POWER_DEFAULT_SAVE_MV       3500U
#define POWER_DEFAULT_RECOVERY_MV   3300U
#define POWER_DEFAULT_STANDBY_MV    3200U
#define POWER_DEFAULT_RESUME_MV     3600U
#define POWER_DEFAULT_THRESHOLDS \
    ((power_thresholds_t){POWER_DEFAULT_SAVE_MV, POWER_DEFAULT_RECOVERY_MV, \
                          POWER_DEFAULT_STANDBY_MV, POWER_DEFAULT_RESUME_MV})
#define POWER_SAVE_EXIT_MV          150U
/* Rules shared with backend, web, apps and ESP firmware (Power-Sync §5). */
#define POWER_PACK_MIN_MV           2800U
#define POWER_PACK_MAX_MV           4200U
#define POWER_GAP_STANDBY_MV        50U   /* standby + 50 <= recovery */
#define POWER_GAP_RECOVERY_MV       50U   /* recovery + 50 <= save */
#define POWER_GAP_RESUME_MV         100U  /* recovery + 100 <= resume */
#define POWER_RESUME_ABOVE_SAVE_MV  400U  /* resume <= save + 400 */
/* Flash writes need a battery that will not brown out mid-erase; fixed, not
 * a setting, so a low recovery threshold never weakens it. */
#define POWER_FLASH_WRITE_MIN_MV    3300U
#define POWER_RESTART_STABLE_S 60U
#define POWER_CHECK_S 60U
typedef enum { POWER_NORMAL, POWER_SAVE, POWER_RECOVERY } power_mode_t;
typedef struct {
    power_thresholds_t th;
    power_mode_t mode;
    uint32_t recovery_since;
    uint8_t stable, failures, low;
    bool standby;
} power_policy_t;
bool PowerPolicy_Validate(const power_thresholds_t *t);
void PowerPolicy_Init(power_policy_t *p, const power_thresholds_t *t, uint16_t mv, bool valid);
/* New thresholds take effect at the next reading; the mode stays until then. */
void PowerPolicy_SetThresholds(power_policy_t *p, const power_thresholds_t *t);
void PowerPolicy_Update(power_policy_t *p, uint16_t mv, bool valid, uint32_t now);
/* The resume threshold travels through Standby in an RTC backup register, so
 * the hourly wake-up compares without reading flash. Unpack falls back to the
 * default for an empty or foreign word. */
uint32_t PowerPolicy_PackResume(uint16_t resume_mv);
uint16_t PowerPolicy_UnpackResume(uint32_t word);
/* Adaptive measurement plan (TD_2_0_17). Rounds count 1..10.
 *   NORMAL: base interval (180 s), CO2 every 5th round, PM every 10th.
 *   SAVE:   twice the base interval, CO2 every 10th round, no PM.
 *   RECOVERY: no radio, no measurements. */
#define POWER_MEASURE_CO2 (1U << 0)
#define POWER_MEASURE_PM  (1U << 1)
#define POWER_ROUNDS      10U
uint8_t PowerPolicy_Measurements(power_mode_t mode, uint8_t round);
uint32_t PowerPolicy_Interval(power_mode_t mode, uint32_t base_s);
#endif
