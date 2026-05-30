/**
 ******************************************************************************
 * @file    ltr390.h
 * @brief   LTR390 UV/ALS sensor driver - public API
 *
 * Driver configuration:
 * - I2C address: 0x53
 * - UV mode (UVS)
 * - Gain: x3
 * - Resolution: 16-bit
 *
 * UV Index formula (LTR390 datasheet, section 5):
 *   UVI = raw / (2300 * gain_factor * integ_factor)
 * For gain x3 (factor=3) and 16-bit resolution (factor=0.25):
 *   UVI = raw / 1725.0
 * Reported as UVI * 100 (integer, no floating point).
 ******************************************************************************
 */

#ifndef LTR390_H
#define LTR390_H

#include <stdint.h>

#define LTR390_OK             ( 0)
#define LTR390_ERR_I2C        (-1)
#define LTR390_ERR_ID         (-2)
#define LTR390_ERR_PARAM      (-3)
#define LTR390_ERR_INIT       (-4)
#define LTR390_ERR_TIMEOUT    (-5)

typedef struct
{
    uint16_t uvi_x100;  /* UV Index * 100 (e.g. 350 = UVI 3.50) */
} LTR390_Data_t;

int32_t LTR390_Init(void);
int32_t LTR390_ReadUV(LTR390_Data_t *data);

#endif /* LTR390_H */
