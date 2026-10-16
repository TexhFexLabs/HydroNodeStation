#ifndef SOLAR_H
#define SOLAR_H
#include <stdint.h>
#include <stdbool.h>
/* Solar statistics between two payload blocks (TD_2_0_10/11/14, TD_2_2_1). Pure
 * arithmetic, no hardware access: the periodic sampler feeds it, the uplink
 * reads a block from it and resets it once the modem accepted the block. */
/* Base watchdog wake-up slice in seconds (from RUNTIME_MAX_SLEEP_MS / 1000) */
#define SOLAR_WATCHDOG_TICK_S        8U

/* Multiplier for solar sampling interval:
 * 1 = 8 s, 2 = 16 s, 3 = 24 s, 4 = 32 s ... */
#ifndef SOLAR_SAMPLE_WATCHDOG_TICKS
#define SOLAR_SAMPLE_WATCHDOG_TICKS  1U
#endif

/* Effective solar sampling interval in seconds */
#define SOLAR_SAMPLE_S               (SOLAR_SAMPLE_WATCHDOG_TICKS * SOLAR_WATCHDOG_TICK_S)
/* Nennleistung des montierten Solarpanels in mW (z. B. 570 für AnySolar 5.7V 100mA, 3000 für 6V 3W) */
#ifndef SOLAR_PANEL_RATING_MW
#define SOLAR_PANEL_RATING_MW        570U
#endif

/* WMO-Schwelle für direkten Sonnenschein: 12 % der Nennleistung (120 W/m² bei 1000 W/m² STC) */
#define SOLAR_P_SUN_PERCENT          12U
#define SOLAR_P_SUN_MW               ((SOLAR_PANEL_RATING_MW * SOLAR_P_SUN_PERCENT) / 100U)

/* Leerlaufspannungsschwelle bei vollem Akku:
 * Ein 5.7V-Panel liegt bei diffuser Bewölkung im Leerlauf bei ca. 4.8 - 5.4 V.
 * Bei direkter ungetrübter Sonne steigt die Leerlaufspannung auf > 5.5 V. */
#define SOLAR_V_SUN_MV               5500U

/* Mindeststrom (in 0.1 mA) unter dem der Akku als voll/unbelastet gilt (z. B. 10 mA = 100 * 0.1 mA) */
#define SOLAR_I_IDLE_01MA            100U
#define SOLAR_MISSING_U16            0xFFFFU
#define SOLAR_MAX_U16                0xFFFEU

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
    uint16_t e_01mwh;    /* Energy since the last block, 0.1 mWh */
    uint16_t sun_ratio;  /* Sunshine ratio since the last block, 0.01 % */
    uint16_t adc_mv;     /* ADC mean voltage, mV */
} solar_block_t;

void Solar_Reset(solar_acc_t *acc);
void Solar_Add(solar_acc_t *acc, const solar_sample_t *s);
/* Fields without a single valid sample carry 0xFFFF; sums saturate at 0xFFFE. */
void Solar_Block(const solar_acc_t *acc, solar_block_t *out);
#endif
