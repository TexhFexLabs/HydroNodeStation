#include "nvm_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t flash[2][NVM_PAGE_BYTES], legacy[NVM_PAYLOAD_BYTES];
static bool migrated, power_good=true, marker_fails;
static int budget=-1;
static bool step(void) { if(budget<0)return true; if(!budget)return false;budget--;return true; }
bool NvmPort_Read(uint32_t slot,uint32_t offset,void *data,uint32_t size)
{ memcpy(data,flash[slot]+offset,size);return true; }
bool NvmPort_Erase(uint32_t slot)
{ if(!step())return false;memset(flash[slot],0xff,NVM_PAGE_BYTES);return true; }
bool NvmPort_Program(uint32_t slot,uint32_t offset,const void *data,uint32_t size)
{
    const uint8_t *source=data;
    for(uint32_t i=0;i<size;i+=8)
    {
        if(!step())return false;
        for(unsigned j=0;j<8;j++) {
            assert(flash[slot][offset+i+j]==0xff);
            flash[slot][offset+i+j]=source[i+j];
        }
    }
    return true;
}
bool NvmPort_LoadLegacy(void *data) {memcpy(data,legacy,sizeof legacy);return true;}
bool NvmPort_WasMigrated(void) {return migrated;}
bool NvmPort_MarkMigrated(void) {if(marker_fails||!step())return false;migrated=true;return true;}
bool NvmPort_CanWrite(void) {return power_good;}
int main(void)
{
    uint32_t old=25,new=26,read;
    memset(legacy,0xff,sizeof legacy);memcpy(legacy,&old,4);
    /* Interrupt every erase/program/commit/migration stage, including first migration. */
    for(unsigned migration=0;migration<2;migration++)
    for(int cut=0;cut<104;cut++)
    {
        memset(flash,0xff,sizeof flash);migrated=false;budget=-1;
        assert(Nvm_Init());
        if(!migration)assert(Nvm_Write(0,&old,4));
        budget=cut;
        bool saved=Nvm_Write(0,&new,4);
        budget=-1;
        assert(Nvm_Init());assert(Nvm_Read(0,&read,4));
        assert(read==old || read==new);
        if(saved)assert(read==new);
    }
    memset(flash,0xff,sizeof flash);migrated=false;
    assert(Nvm_Init());assert(Nvm_Write(0,&old,4));assert(Nvm_Write(0,&new,4));
    flash[1][16]^=1; /* newest snapshot corrupt: choose previous valid one */
    assert(Nvm_Init());assert(Nvm_Read(0,&read,4));assert(read==old);
    flash[0][16]^=1;
    assert(!Nvm_Init()); /* migrated device must not silently roll back to legacy */
    memset(flash,0xff,sizeof flash);migrated=false;assert(Nvm_Init());
    power_good=false;assert(!Nvm_Write(0,&new,4));assert(Nvm_LastError()==NVM_ERR_POWER);power_good=true;
    assert(!Nvm_Write(NVM_PAYLOAD_BYTES-1,&new,4));assert(Nvm_LastError()==NVM_ERR_ARGS);
    assert(!Nvm_Read(UINT32_MAX,&read,4));assert(Nvm_LastError()==NVM_ERR_ARGS);
    /* Each write step reports its own detail code, so a field fault is actionable. */
    assert(Nvm_Write(0,&new,4)&&Nvm_LastError()==NVM_ERR_NONE);
    budget=0;assert(!Nvm_Write(0,&old,4));assert(Nvm_LastError()==NVM_ERR_ERASE);
    budget=1;assert(!Nvm_Write(0,&old,4));assert(Nvm_LastError()==NVM_ERR_PROGRAM);
    budget=-1;
    /* A marker that cannot be programmed must not report a committed write as
     * lost: the caller resets the MCU on a reported loss. */
    memset(flash,0xff,sizeof flash);migrated=false;
    assert(Nvm_Init());
    marker_fails=true;
    assert(Nvm_Write(0,&new,4));assert(Nvm_LastError()==NVM_ERR_NONE);
    marker_fails=false;
    assert(Nvm_Init());assert(Nvm_Read(0,&read,4));assert(read==new);
    puts("NVM: 208 power-cut cases, CRC fallback, no stale legacy rollback, voltage, bounds, step-level error codes and unwritable migration marker passed");
}
