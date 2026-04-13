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
 * The driver exposes raw UV counts plus a heuristic UV index estimate.
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
    uint32_t uvs_raw;   /* Raw UVS counts (up to 20 bits) */
    float    uvi_est;   /* Heuristic UV index estimate */
} LTR390_Data_t;

int32_t LTR390_Init(void);
int32_t LTR390_ReadUV(LTR390_Data_t *data);

#endif /* LTR390_H */
