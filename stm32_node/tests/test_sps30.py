"""Compile the actual SPS30 driver against a stateful I2C sensor model."""
from pathlib import Path
import subprocess

def run(root, tmp):
    stub=tmp/'sps-stubs';stub.mkdir()
    (stub/'platform.h').write_text('#include <stdint.h>\n#include <stddef.h>\n')
    (stub/'sys_conf.h').write_text('')
    (stub/'i2c.h').write_text('''#include <stdint.h>
typedef struct {void *Instance;} I2C_HandleTypeDef;
extern I2C_HandleTypeDef hi2c2;
#define I2C2 ((void *)1)
#define HAL_I2C_STATE_RESET 0
#define HAL_OK 0
void MX_I2C2_Init(void);
int HAL_I2C_GetState(I2C_HandleTypeDef *h);
int HAL_I2C_Master_Transmit(I2C_HandleTypeDef *,uint16_t,uint8_t *,uint16_t,uint32_t);
int HAL_I2C_Master_Receive(I2C_HandleTypeDef *,uint16_t,uint8_t *,uint16_t,uint32_t);
void HAL_Delay(uint32_t);
''')
    (tmp/'sps-model.c').write_text('''#include "sps30.h"
#include "i2c.h"
#include <assert.h>
#include <stdio.h>
I2C_HandleTypeDef hi2c2={I2C2};
static unsigned now, idle_at, interface_up_at;
static int measuring=1, asleep, missing;
void MX_I2C2_Init(void) {hi2c2.Instance=I2C2;}
int HAL_I2C_GetState(I2C_HandleTypeDef *h) {(void)h;return 1;}
void HAL_Delay(uint32_t delay) {now+=delay;}
int HAL_I2C_Master_Receive(I2C_HandleTypeDef *h,uint16_t a,uint8_t *b,uint16_t n,uint32_t t)
{(void)h;(void)a;(void)b;(void)n;(void)t;return -1;}
int HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h,uint16_t a,uint8_t *b,uint16_t n,uint32_t t)
{
    (void)h;(void)a;(void)n;(void)t;
    if(missing)return -1;
    unsigned cmd=((unsigned)b[0]<<8)|b[1];
    /* Sleep-Mode disables the I2C interface: the command that wakes it is not
       acknowledged, and the interface needs its wake-up time before it can
       answer the next one. */
    if(cmd==0x1103) {if(asleep){asleep=0;interface_up_at=now+5;return -1;}
                     return now<interface_up_at?-1:0;}
    if(cmd==0x0104) {idle_at=now+20;measuring=0;return 0;}
    if(cmd==0x1001) {if(measuring || now<idle_at)return -1;asleep=1;return 0;}
    if(cmd==0x0010) {if(asleep)return -1;measuring=1;return 0;}
    return 0;
}
int main(void)
{
    assert(SPS30_Init()==0 && asleep); /* MCU reset while sensor was measuring */
    assert(SPS30_WakeUp()==0);assert(SPS30_StartMeasurement()==0);
    assert(SPS30_StopMeasurement()==0);assert(SPS30_Sleep()==0 && asleep);
    /* Waking from sleep must end up measuring, not idling with zeroed values. */
    assert(SPS30_WakeUp()==0);assert(SPS30_StartMeasurement()==0 && measuring);
    assert(SPS30_StopMeasurement()==0);assert(SPS30_Sleep()==0 && asleep);
    missing=1;assert(SPS30_WakeUp()!=0);assert(SPS30_Init()!=0);
    puts("SPS30 actual driver: stop-to-idle timing, MCU-only restart and missing-device errors passed");
}
''')
    exe=tmp/'sps'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(stub),'-I'+str(root/'Core/Inc'),
                    str(tmp/'sps-model.c'),str(root/'Core/Src/sps30.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
