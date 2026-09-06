"""Exercise actual SCD41 driver including rejected stabilization results."""
import subprocess

def run(root,tmp):
    stub=tmp/'scd-stubs';stub.mkdir()
    (stub/'platform.h').write_text('#include <stdint.h>\n#include <stddef.h>\n')
    (stub/'i2c.h').write_text((tmp/'sps-stubs/i2c.h').read_text())
    (stub/'sys_conf.h').write_text('''#define SCD41_I2C_TIMEOUT_MS 100U
#define SCD41_WAKEUP_DELAY_MS 30U
#define SCD41_SINGLE_SHOT_RHT_WAIT_MS 50U
#define SCD41_TEMPERATURE_OFFSET_C_X100 0U
#define SCD41_SENSOR_ALTITUDE_M 0U
#define SCD41_AMBIENT_PRESSURE_MBAR 0U
#define SCD41_ASC_ENABLED 0U
#define SCD41_PERSIST_SETTINGS 0U
''')
    (tmp/'scd-model.c').write_text('''#include "scd41.h"
#include "i2c.h"
#include <assert.h>
#include <stdio.h>
I2C_HandleTypeDef hi2c2={I2C2};
static unsigned cmd;static int ready=1;
void MX_I2C2_Init(void) {hi2c2.Instance=I2C2;}
int HAL_I2C_GetState(I2C_HandleTypeDef *h) {(void)h;return 1;}
void HAL_Delay(uint32_t ms) {(void)ms;}
static void word(uint8_t *p,unsigned value) {
 p[0]=value>>8;p[1]=value;
 uint8_t crc=255;
 for(int i=0;i<2;i++) {crc^=p[i];for(int b=0;b<8;b++)crc=(crc&128)?(crc<<1)^0x31:crc<<1;}
 p[2]=crc;
}
int HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h,uint16_t a,uint8_t *b,uint16_t n,uint32_t t)
{(void)h;(void)a;(void)n;(void)t;cmd=((unsigned)b[0]<<8)|b[1];return 0;}
int HAL_I2C_Master_Receive(I2C_HandleTypeDef *h,uint16_t a,uint8_t *b,uint16_t n,uint32_t t)
{
 (void)h;(void)a;(void)n;(void)t;
 if(cmd==0xe4b8) {word(b,ready);return 0;}
 if(cmd==0xec05) {word(b,425);word(b+3,30000);word(b+6,30000);return 0;}
 return -1;
}
int main(void) {
 uint16_t ppm=0;
 assert(SCD41_Init()==0);
 assert(SCD41_StartCo2SingleShot()==0);
 assert(SCD41_ReadCo2SingleShot(&ppm,0,0)!=0); /* not the useful shot */
 assert(SCD41_StartCo2SingleShot()==0);ready=0;
 assert(SCD41_DiscardAndRestartCo2SingleShot()!=0);ready=1;
 assert(SCD41_ReadCo2SingleShot(&ppm,0,0)!=0);
 assert(SCD41_StartCo2SingleShot()==0);
 assert(SCD41_DiscardAndRestartCo2SingleShot()==0);
 assert(SCD41_ReadCo2SingleShot(&ppm,0,0)==0 && ppm==425);
 assert(SCD41_ReadCo2SingleShot(&ppm,0,0)!=0);
 puts("SCD41 actual driver: failed/not-ready stabilization cannot become a published useful shot");
}
''')
    exe=tmp/'scd'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(stub),'-I'+str(root/'Core/Inc'),
                    str(tmp/'scd-model.c'),str(root/'Core/Src/scd41.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
