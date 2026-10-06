#ifndef POWER_POLICY_H
#define POWER_POLICY_H
#include <stdint.h>
#include <stdbool.h>
/* Conservative defaults for 1S LiPo. Validate against the actual cell. */
#define POWER_STOP_MV 3300U
#define POWER_SAVE_MV 3500U
#define POWER_NORMAL_MV 3650U
#define POWER_RESTART_MV 3600U
#define POWER_RESTART_STABLE_S 60U
#define POWER_CHECK_S 60U
/* Deep discharge: below this, twice in a row and valid, RECOVERY becomes
 * Standby with an RTC wake-up (TD_2_0_18). Validate against the cell
 * datasheet and its protection circuit. */
#define POWER_STANDBY_MV 3200U
typedef enum { POWER_NORMAL, POWER_SAVE, POWER_RECOVERY } power_mode_t;
typedef struct { power_mode_t mode; uint32_t recovery_since; uint8_t stable, failures, low; bool standby; } power_policy_t;
void PowerPolicy_Init(power_policy_t *p, uint16_t mv, bool valid);
void PowerPolicy_Update(power_policy_t *p, uint16_t mv, bool valid, uint32_t now);
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
