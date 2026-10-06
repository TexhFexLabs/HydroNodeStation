#ifndef FAULT_REPORT_H
#define FAULT_REPORT_H
#include <stdint.h>
#include <stdbool.h>
#include "fault_codes.h"
/* Fault and event queue for fPort 99 (TD_2_0_15). Pure logic, no I/O.
 * RAM only: nothing survives a reset except the restart reason, which the
 * boot entry 0x0540 carries. */
#define FAULT_QUEUE_LEN           16U
#define FAULT_FRAME_MAX_ENTRIES   8U
#define FAULT_FRAME_MAX_LEN       (FAULT_FRAME_MAX_ENTRIES * 4U)
#define FAULT_READ_ERROR_LIMIT    3U
#define FAULT_COMPONENTS          0x11U
/* Sending budget: urgent entries go with the next regular uplink, the rest
 * waits until an hour has passed since the last fault frame. */
#define FAULT_NORMAL_INTERVAL_S   3600U
/* A part whose read errors open again within this time after the last
 * report stays open until it has read well for this long. */
#define FAULT_FLAP_HOLD_S         (6U * 3600U)
/* 0x0431/0x0432 are counted and reported once per this period. */
#define FAULT_COUNTER_PERIOD_S    86400U

typedef struct { uint16_t code, detail; } fault_entry_t;

/* States (categories 0x01-0x03): queued once on entry and once on exit
 * (bit 15); repeated raises while active add nothing. */
void Fault_SetState(uint16_t code, bool active, uint16_t detail);
bool Fault_IsActive(uint16_t code);
/* Events (categories 0x04-0x06): queued on every call. */
void Fault_Event(uint16_t code, uint16_t detail);
/* Counted events (0x0431, 0x0432): one entry per FAULT_COUNTER_PERIOD_S with
 * the count since the last one, nothing when it stayed 0. */
void Fault_Count(uint16_t code);

/* MCU monotonic seconds; call from the main loop before anything else here.
 * Also flushes the counted events when their period is over. */
void Fault_Tick(uint32_t now_s);
/* Urgent: boot info (0x0540-0x0542, 0x0544), rejoin 0x0430, part missing
 * (0x01), supply (0x03), command acknowledgements (0x06). */
bool Fault_IsUrgent(uint16_t code);
/* A frame should go now: something urgent is queued, or anything is queued
 * and FAULT_NORMAL_INTERVAL_S passed since the last accepted frame. */
bool Fault_FrameDue(void);

/* Component bookkeeping for categories 0x01/0x02. Read errors that come back
 * within FAULT_FLAP_HOLD_S of their last report keep the state open until the
 * part has read well for FAULT_FLAP_HOLD_S. */
void Fault_ComponentInit(uint8_t comp, bool found, uint8_t i2c_addr);
void Fault_ComponentResult(uint8_t comp, bool ok, uint32_t i2c_error);

/* Up to 8 entries for one fPort 99 frame (an overflow marker 0x054F comes
 * first when entries were dropped). Returns the byte length, 0 if empty. */
uint8_t Fault_BuildFrame(uint8_t *buf);
/* The modem accepted the frame last built: drop its entries. */
void Fault_FrameAccepted(void);
uint8_t Fault_Pending(void);

/* 0x0540 detail low byte from RCC->CSR. */
uint8_t Fault_CompressResetFlags(uint32_t csr);
/* Test hook: back to power-on state. */
void Fault_Reset(void);
#endif
