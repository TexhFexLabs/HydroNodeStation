#ifndef PULSE_COUNTER_H
#define PULSE_COUNTER_H
#include <stdint.h>
#include <stdbool.h>
/* Generic contact counters on P4 (TD_2_0_13), built in only with
 * PULSE_COUNTERS_ENABLED (sys_conf.h).
 * Hardware per channel: 1 kOhm series, 1 MOhm pull-up to VCC (independent
 * of 3V3SWITCHABLE), 1 nF to GND. The RC filter suits contacts up to about
 * 50 Hz (rain gauge, reed anemometer); no promise for fast signals.
 * Channel 1 = PA4 (EXTI4), channel 2 = PA5 (EXTI9_5). LPTIM2 on PA5 would
 * count without waking, but LPTIM2 is not functional in STOP2 on the
 * STM32WL, so both channels count by EXTI with a software lockout. */
#define PULSE_CNT1_LOCKOUT_MS  50U   /* rain tipping bucket */
#define PULSE_CNT2_LOCKOUT_MS  2U    /* anemometer reed contact */
#define PULSE_MISSING          0xFFFFU
#define PULSE_MAX              0xFFFEU

typedef struct { uint32_t total[2]; uint32_t committed[2]; uint32_t last_ms[2]; bool seen[2]; } pulse_counter_t;

/* Pins: EXTI on the falling edge without internal pull-up (that would pull
 * 60-132 uA through a closed contact). No-op when the counters are off. */
void PulseCounter_Init(void);
/* Counts since the last committed block, saturating at 0xFFFE;
 * 0xFFFF for both when the counters are not built in. */
void PulseCounter_Read(uint16_t *cnt1, uint16_t *cnt2);
/* Call when the modem accepted the block that carried PulseCounter_Read(). */
void PulseCounter_Commit(uint16_t cnt1, uint16_t cnt2);

/* Pure logic, exposed for the host test. */
bool PulseCounter_Edge(pulse_counter_t *pc, uint8_t ch, uint32_t now_ms, uint32_t lockout_ms);
uint16_t PulseCounter_Since(const pulse_counter_t *pc, uint8_t ch);
void PulseCounter_Advance(pulse_counter_t *pc, uint8_t ch, uint16_t reported);
#endif
