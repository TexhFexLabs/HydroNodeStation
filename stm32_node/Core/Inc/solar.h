#ifndef SOLAR_H
#define SOLAR_H
#include <stdint.h>
#include <stdbool.h>
/* Solar statistics between two payload blocks (TD_2_0_10/11/14). Pure
 * arithmetic, no hardware access: the 30 s sampler feeds it, the uplink
 * reads a block from it and resets it once the modem accepted the block. */
#define SOLAR_SAMPLE_S       30U
/* Sun: P above 12 % of the 570.8 mW panel rating (WMO 120 W/m2), or, when
 * a full battery leaves the charger idle, the panel near open circuit.
 * Starting values, +-20 % without a reference (POWER_ANALYSIS 9.42). */
#define SOLAR_P_SUN_MW       68U
#define SOLAR_V_SUN_MV       5500U
#define SOLAR_MISSING_U16    0xFFFFU
#define SOLAR_MAX_U16        0xFFFEU

typedef struct
{
    bool     ina_valid;
    uint16_t bus_mv;        /* INA226 VBUS */
    uint16_t current_01ma;  /* INA226, >= 0 */
    bool     adc_valid;
    uint16_t adc_mv;        /* Divider at PB2, scaled back to panel volts */
} solar_sample_t;

typedef struct
{
    uint32_t ina_n, adc_n, sun_n, any_n;
    uint64_t sum_mv, sum_01ma, sum_uw, sum_adc_mv;
} solar_acc_t;

typedef struct
{
    uint16_t v_mv;     /* INA226 mean voltage, mV */
    uint16_t i_01ma;   /* INA226 mean current, 0.1 mA */
    uint16_t p_mw;     /* Mean power, mW */
    uint16_t e_01mwh;  /* Energy since the last block, 0.1 mWh */
    uint16_t sun_s;    /* Sunshine since the last block, s */
    uint16_t adc_mv;   /* ADC mean voltage, mV */
} solar_block_t;

void Solar_Reset(solar_acc_t *acc);
void Solar_Add(solar_acc_t *acc, const solar_sample_t *s);
/* Fields without a single valid sample carry 0xFFFF; sums saturate at 0xFFFE. */
void Solar_Block(const solar_acc_t *acc, solar_block_t *out);
#endif
