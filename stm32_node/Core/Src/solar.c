#include "solar.h"

static uint16_t sat16(uint64_t v)
{
    return (v > SOLAR_MAX_U16) ? (uint16_t)SOLAR_MAX_U16 : (uint16_t)v;
}

static uint16_t mean16(uint64_t sum, uint32_t n)
{
    return (n == 0U) ? (uint16_t)SOLAR_MISSING_U16 : sat16((sum + n / 2U) / n);
}

void Solar_Reset(solar_acc_t *acc)
{
    *acc = (solar_acc_t){ 0 };
}

void Solar_Add(solar_acc_t *acc, const solar_sample_t *s)
{
    uint32_t p_uw = 0U;
    if (s->ina_valid)
    {
        /* mV x 0.1 mA = 0.1 uW */
        p_uw = ((uint32_t)s->bus_mv * s->current_01ma + 5U) / 10U;
        acc->ina_n++;
        acc->sum_mv += s->bus_mv;
        acc->sum_01ma += s->current_01ma;
        acc->sum_uw += p_uw;
    }
    if (s->adc_valid)
    {
        acc->adc_n++;
        acc->sum_adc_mv += s->adc_mv;
    }
    if (s->ina_valid || s->adc_valid)
    {
        acc->any_n++;
        if ((s->ina_valid && p_uw > SOLAR_P_SUN_MW * 1000U) ||
            (s->adc_valid && s->adc_mv > SOLAR_V_SUN_MV))
        {
            acc->sun_n++;
        }
    }
}

void Solar_Block(const solar_acc_t *acc, solar_block_t *out)
{
    out->v_mv = mean16(acc->sum_mv, acc->ina_n);
    out->i_01ma = mean16(acc->sum_01ma, acc->ina_n);
    out->p_mw = (acc->ina_n == 0U) ? (uint16_t)SOLAR_MISSING_U16
                                   : sat16((acc->sum_uw + 500U * acc->ina_n) / (1000U * (uint64_t)acc->ina_n));
    /* 0.1 mWh = 360000 uW s; each sample stands for SOLAR_SAMPLE_S. */
    out->e_01mwh = (acc->ina_n == 0U) ? (uint16_t)SOLAR_MISSING_U16
                                      : sat16((acc->sum_uw * SOLAR_SAMPLE_S + 180000U) / 360000U);
    out->sun_s = (acc->any_n == 0U) ? (uint16_t)SOLAR_MISSING_U16 : sat16((uint64_t)acc->sun_n * SOLAR_SAMPLE_S);
    out->adc_mv = mean16(acc->sum_adc_mv, acc->adc_n);
}
