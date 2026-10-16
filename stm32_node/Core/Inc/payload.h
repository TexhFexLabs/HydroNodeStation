#ifndef PAYLOAD_H
#define PAYLOAD_H
#include <stdint.h>
#include "sys_sensors.h"
/* Uplink layouts of firmware 2.2 (PCB 1.1). All fields are big-endian
 * 16-bit values; every port always has the same length.
 *   fPort 2: base (10)                         + block (20) = 30 bytes
 *   fPort 3: base (10) + CO2 (2)               + block (20) = 32 bytes
 *   fPort 4: base (10) + CO2 (2) + PM (18)     + block (20) = 50 bytes
 * Base: T i16 0.01 degC, RH u16 0.01 %, p u16 0.1 hPa, VBAT u16 mV,
 * UV u16 0.01. Port 4 includes SPS30 without typical size (50 bytes <= 51 bytes DR0-DR2).
 * Missing values: 0xFFFF (u16) and 0x8000 (i16). */
#define PAYLOAD_PORT_BASE      2U
#define PAYLOAD_PORT_CO2       3U
#define PAYLOAD_PORT_FULL      4U
#define PAYLOAD_BLOCK_LEN      20U
#define PAYLOAD_MAX_LEN        50U

/* "Since the last block" means since the last block the modem accepted. */
typedef struct
{
    uint16_t solar_mv;       /* INA226 mean, mV */
    uint16_t solar_01ma;     /* INA226 mean, 0.1 mA */
    uint16_t solar_mw;       /* mean V x I, mW */
    uint16_t solar_01mwh;    /* energy since last block, 0.1 mWh */
    uint16_t sun_ratio;      /* sunshine ratio since last block, 0.01 % */
    uint16_t soc_x100;       /* MAX17048 SOC, 0.01 % */
    int16_t  crate_x100;     /* MAX17048 charge rate, 0.01 %/h */
    int16_t  board_t_x100;   /* internal sensor, 0.01 degC */
    uint16_t count1;         /* PA4 pulses since last block, 0xFFFF = off */
    uint16_t count2;         /* PA5 pulses since last block, 0xFFFF = off */
} payload_block_t;

/* Port from the sensors read this round: PM -> 4, CO2 -> 3, else 2. */
uint8_t Payload_Port(uint8_t sensor_flags);
/* Writes the frame for port into buf (PAYLOAD_MAX_LEN bytes); returns its
 * length, 0 for an unknown port. */
uint8_t Payload_Encode(uint8_t port, const sensor_t *s, const payload_block_t *block, uint8_t *buf);
#endif
