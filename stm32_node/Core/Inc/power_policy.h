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
typedef enum { POWER_NORMAL, POWER_SAVE, POWER_RECOVERY } power_mode_t;
typedef struct { power_mode_t mode; uint32_t recovery_since; uint8_t stable, failures; } power_policy_t;
void PowerPolicy_Init(power_policy_t *p, uint16_t mv, bool valid);
void PowerPolicy_Update(power_policy_t *p, uint16_t mv, bool valid, uint32_t now);
#endif
