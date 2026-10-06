#include "payload.h"

static void put16(uint8_t *buf, uint8_t *n, uint16_t v)
{
    buf[(*n)++] = (uint8_t)(v >> 8);
    buf[(*n)++] = (uint8_t)v;
}

/* ADD YOUR SENSOR (send): ports 2/3/4 keep their lengths (backend layouts
 * depend on them). Give your data its own port, e.g. 6 = port 2 + your bytes,
 * select it here and encode it in Payload_Encode(); at most 51 bytes (DR0).
 * See doc/ADDING_SENSORS.md. */
uint8_t Payload_Port(uint8_t sensor_flags)
{
    if ((sensor_flags & SENSOR_FLAG_SPS30) != 0U) { return PAYLOAD_PORT_FULL; }
    if ((sensor_flags & SENSOR_FLAG_CO2) != 0U) { return PAYLOAD_PORT_CO2; }
    return PAYLOAD_PORT_BASE;
}

uint8_t Payload_Encode(uint8_t port, const sensor_t *s, const payload_block_t *b, uint8_t *buf)
{
    uint8_t n = 0U;
    if (port < PAYLOAD_PORT_BASE || port > PAYLOAD_PORT_FULL) { return 0U; }
    put16(buf, &n, (uint16_t)s->temperature);
    put16(buf, &n, s->humidity);
    put16(buf, &n, s->pressure);
    put16(buf, &n, s->battery_voltage);
    put16(buf, &n, s->uvi_x100);
    if (port >= PAYLOAD_PORT_CO2) { put16(buf, &n, s->co2_ppm); }
    if (port == PAYLOAD_PORT_FULL)
    {
        const uint16_t pm[10] = { s->pm1_0, s->pm2_5, s->pm4_0, s->pm10_0, s->nc_0_5,
                                  s->nc_1_0, s->nc_2_5, s->nc_4_0, s->nc_10_0, s->typ_size };
        for (uint8_t i = 0U; i < 10U; ++i) { put16(buf, &n, pm[i]); }
        return n;
    }
    put16(buf, &n, b->solar_mv);
    put16(buf, &n, b->solar_01ma);
    put16(buf, &n, b->solar_mw);
    put16(buf, &n, b->solar_01mwh);
    put16(buf, &n, b->sun_s);
    put16(buf, &n, b->solar_adc_mv);
    put16(buf, &n, b->soc_x100);
    put16(buf, &n, (uint16_t)b->crate_x100);
    put16(buf, &n, (uint16_t)b->board_t_x100);
    put16(buf, &n, b->count1);
    put16(buf, &n, b->count2);
    return n;
}
