#ifndef RUNTIME_HEALTH_H
#define RUNTIME_HEALTH_H
#include <stdint.h>
/* Restart reasons kept in RTC backup DR6 across a reset. 7 and 8 are not
 * faults but planned restarts (downlink FF, Standby wake-up); they share the
 * register so the boot report (fPort 99, 0x0540) carries them. */
enum { RUNTIME_FAULT_HAL=1, RUNTIME_FAULT_CPU=2, RUNTIME_FAULT_MODEM=3,
       RUNTIME_FAULT_PROGRESS=4, RUNTIME_FAULT_NVM=5, RUNTIME_FAULT_LSE=6,
       RUNTIME_REASON_COMMAND=7, RUNTIME_REASON_STANDBY=8 };
#define RUNTIME_MAX_SLEEP_MS 8000U
void Runtime_EarlyInit(void);
void Runtime_Init(void);
void Runtime_Process(void);
void Runtime_ExpectProgress(uint32_t timeout_s);
void Runtime_FaultDetail(uint32_t detail);
/* Non-fatal anomalies. Recovery is the progress deadline and the watchdog,
 * not a reset at each detection site; these only keep them visible. */
void Runtime_NotePanic(void);
void Runtime_NoteNvmError(uint32_t detail);
uint16_t Runtime_PanicCount(void);
uint8_t Runtime_NvmErrorCount(void);
uint8_t Runtime_LastNvmError(void);
void Runtime_Fault(uint32_t reason) __attribute__((noreturn));
/* For faults before Runtime_Init (clock setup): stores the reason directly,
 * repeats its blink code on the diagnostic LED for hold_s, then resets. */
void Runtime_EarlyFault(uint32_t reason, uint32_t hold_s) __attribute__((noreturn));
/* Planned restart: store the reason for the boot report, then reset. */
void Runtime_Restart(uint32_t reason) __attribute__((noreturn));
uint32_t Runtime_BootCount(void);
uint32_t Runtime_ResetFlags(void);
uint32_t Runtime_LastFault(void);
uint32_t Runtime_LastFaultDetail(void);
#endif
