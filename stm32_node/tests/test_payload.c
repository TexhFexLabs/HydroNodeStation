#include "payload.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t be(const uint8_t *b, unsigned i) { return (uint16_t)(b[i] << 8 | b[i + 1]); }

int main(void)
{
    sensor_t s;
    memset(&s, 0, sizeof s);
    s.temperature = -512; s.humidity = 4520; s.pressure = 10132; s.battery_voltage = 3987;
    s.uvi_x100 = 350; s.co2_ppm = 612; s.pm10_0 = 123; s.typ_size = 650;
    payload_block_t b = { 5123, 1234, 632, 77, 3600, 5180, 8765, -321, 2345, 0xFFFF, 0xFFFF };
    uint8_t buf[PAYLOAD_MAX_LEN];

    assert(Payload_Port(0) == 2 && Payload_Port(SENSOR_FLAG_CO2) == 3);
    assert(Payload_Port(SENSOR_FLAG_SPS30) == 4 && Payload_Port(SENSOR_FLAG_CO2 | SENSOR_FLAG_SPS30) == 4);

    /* Port 2: base + block = 32 bytes, block at offset 10. */
    assert(Payload_Encode(2, &s, &b, buf) == 32);
    assert(be(buf, 0) == (uint16_t)-512 && be(buf, 6) == 3987 && be(buf, 8) == 350);
    assert(be(buf, 10) == 5123 && be(buf, 12) == 1234 && be(buf, 14) == 632 && be(buf, 16) == 77);
    assert(be(buf, 18) == 3600 && be(buf, 20) == 5180 && be(buf, 22) == 8765);
    assert(be(buf, 24) == (uint16_t)-321 && be(buf, 26) == 2345);
    assert(be(buf, 28) == 0xFFFF && be(buf, 30) == 0xFFFF);
    /* Port 3: CO2 at offset 10, block shifted to offset 12, 34 bytes. */
    assert(Payload_Encode(3, &s, &b, buf) == 34);
    assert(be(buf, 10) == 612 && be(buf, 12) == 5123 && be(buf, 32) == 0xFFFF);
    /* Port 4: unchanged 32-byte layout without block. */
    assert(Payload_Encode(4, &s, &b, buf) == 32);
    assert(be(buf, 10) == 612 && be(buf, 18) == 123 && be(buf, 30) == 650);
    /* Sentinels pass through untouched. */
    s.temperature = INT16_MIN; s.humidity = 0xFFFF;
    b = (payload_block_t){ 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, INT16_MIN, INT16_MIN, 0xFFFF, 0xFFFF };
    assert(Payload_Encode(2, &s, &b, buf) == 32);
    assert(be(buf, 0) == 0x8000 && be(buf, 2) == 0xFFFF && be(buf, 24) == 0x8000 && be(buf, 26) == 0x8000);
    assert(Payload_Encode(5, &s, &b, buf) == 0);
    puts("Payload: ports 2/3/4 at 32/34/32 bytes, block offsets, sentinels and port selection passed");
    return 0;
}
