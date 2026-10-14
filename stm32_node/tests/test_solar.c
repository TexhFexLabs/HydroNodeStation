#include "solar.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    solar_acc_t acc;
    solar_block_t b;
    Solar_Reset(&acc);
    Solar_Block(&acc, &b);
    /* Nothing measured: every field is missing. */
    assert(b.v_mv == 0xFFFF && b.i_01ma == 0xFFFF && b.p_mw == 0xFFFF &&
           b.e_01mwh == 0xFFFF && b.sun_ratio == 0xFFFF && b.adc_mv == 0xFFFF);
    /* 45 samples at 5000 mV / 200.0 mA = 1 W for 6 min (45 * 8 s): 100 mWh, all sunny. */
    for (int i = 0; i < 45; i++)
        Solar_Add(&acc, &(solar_sample_t){ true, 5000U, 2000U, true, 5100U });
    Solar_Block(&acc, &b);
    assert(b.v_mv == 5000U && b.i_01ma == 2000U && b.p_mw == 1000U);
    assert(b.e_01mwh == 1000U && b.sun_ratio == 10000U && b.adc_mv == 5100U);
    /* Diffuse clouds under load: 5.0 V / 40.0 mA = 200 mW (< 360 mW P_SUN), ADC 6.6 V (> V_SUN).
     * Under load, only power decides: no sunshine! */
    Solar_Reset(&acc);
    Solar_Add(&acc, &(solar_sample_t){ true, 5000U, 400U, true, 6600U });
    Solar_Block(&acc, &b);
    assert(b.sun_ratio == 0U && b.p_mw == 200U);
    /* Shade: 4.2 V / 1.0 mA = 4.2 mW (< 360 mW P_SUN); ADC 4.3 V (< 6.5 V V_SUN). */
    Solar_Reset(&acc);
    Solar_Add(&acc, &(solar_sample_t){ true, 4200U, 10U, true, 4300U });
    Solar_Block(&acc, &b);
    assert(b.sun_ratio == 0U && b.p_mw == 4U && b.e_01mwh == 0U);
    /* Diffuse clouds in idle (full battery): 0 mA, ADC 6450 mV <= 6500 mV V_SUN: no sunshine! */
    Solar_Reset(&acc);
    Solar_Add(&acc, &(solar_sample_t){ true, 6400U, 0U, true, 6450U });
    Solar_Block(&acc, &b);
    assert(b.sun_ratio == 0U && b.p_mw == 0U);
    /* Full sun in idle (full battery): 0 mA, ADC 6750 mV > 6500 mV V_SUN: sunshine! */
    Solar_Reset(&acc);
    Solar_Add(&acc, &(solar_sample_t){ true, 6700U, 0U, true, 6750U });
    Solar_Block(&acc, &b);
    assert(b.sun_ratio == 10000U && b.p_mw == 0U);
    /* INA226 missing: its fields are missing, ADC decides via V_SUN. */
    Solar_Reset(&acc);
    Solar_Add(&acc, &(solar_sample_t){ false, 0U, 0U, true, 6000U });
    Solar_Block(&acc, &b);
    assert(b.v_mv == 0xFFFF && b.i_01ma == 0xFFFF && b.p_mw == 0xFFFF && b.e_01mwh == 0xFFFF);
    assert(b.adc_mv == 6000U && b.sun_ratio == 0U);
    Solar_Reset(&acc);
    Solar_Add(&acc, &(solar_sample_t){ false, 0U, 0U, true, 6800U });
    Solar_Block(&acc, &b);
    assert(b.adc_mv == 6800U && b.sun_ratio == 10000U);
    /* Long gap without an accepted block: sums saturate below the sentinel. */
    Solar_Reset(&acc);
    for (int i = 0; i < 3000; i++)
        Solar_Add(&acc, &(solar_sample_t){ true, 6900U, 12000U, true, 6900U });
    Solar_Block(&acc, &b);
    assert(b.e_01mwh == 0xFFFE && b.sun_ratio == 10000U && b.p_mw == 8280U && b.v_mv == 6900U);
    puts("Solar: means, V x I power, energy in 0.1 mWh, sun ratio, sentinels and saturation passed");
    return 0;
}
