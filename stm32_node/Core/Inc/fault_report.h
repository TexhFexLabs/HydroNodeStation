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

typedef struct { uint16_t code, detail; } fault_entry_t;

/* States (categories 0x01-0x03): queued once on entry and once on exit
 * (bit 15); repeated raises while active add nothing. */
void Fault_SetState(uint16_t code, bool active, uint16_t detail);
bool Fault_IsActive(uint16_t code);
/* Events (categories 0x04-0x06): queued on every call. */
void Fault_Event(uint16_t code, uint16_t detail);

/* Component bookkeeping for categories 0x01/0x02. */
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
