/**
 ******************************************************************************
 * @file    debug_profile.h
 * @brief   Debug profile: runs when PB4 (DEBUG_SW_Pin) is HIGH at boot.
 *          Reads all sensors every 2 s and dumps values via APP_LOG.
 *          Normal LoRaWAN program is skipped in this mode.
 ******************************************************************************
 */

#ifndef __DEBUG_PROFILE_H
#define __DEBUG_PROFILE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Enter debug profile mode. Never returns.
 *         Pre-condition: MX_LoRaWAN_Init() has been called (trace + sensors
 *         are already initialised by the time this is invoked from main.c).
 */
void DebugProfile_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_PROFILE_H */
