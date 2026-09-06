#include "nvm_store.h"
#include "platform.h"
#include "max17048.h"
#include "power_policy.h"
#include <string.h>
#define LEGACY_CONTEXT 0x0803F000UL
#define LEGACY_CONFIG 0x0803F800UL
#define MIGRATION_ADDR 0x0803F7F8UL
#define MIGRATED UINT64_C(0x484E4D4947524154)
static volatile bool reading, ecc_error;

bool NvmPort_HandleEcc(void)
{
    if (reading && __HAL_FLASH_GET_FLAG(FLASH_FLAG_ECCD))
    {
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ECCD);
        ecc_error=true;
        return true;
    }
    return false;
}
static bool read_flash(uint32_t address, void *data, uint32_t size)
{
    ecc_error=false;
    reading=true;
    volatile const uint8_t *source=(volatile const uint8_t *)address;
    uint8_t *dest=data;
    for(uint32_t i=0;i<size;i++) dest[i]=source[i];
    reading=false;
    return !ecc_error;
}
bool NvmPort_Read(uint32_t slot,uint32_t offset,void *data,uint32_t size)
{
    if(slot>1U || offset>NVM_PAGE_BYTES || size>NVM_PAGE_BYTES-offset) return false;
    return read_flash(NVM_BASE+slot*NVM_PAGE_BYTES+offset,data,size);
}
static bool erase_page(uint32_t page)
{
    FLASH_EraseInitTypeDef erase={0};
    erase.TypeErase=FLASH_TYPEERASE_PAGES;
    erase.Page=page;
    erase.NbPages=1U;
    uint32_t error=0;
    if(HAL_FLASH_Unlock()!=HAL_OK) return false;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    HAL_StatusTypeDef status=HAL_FLASHEx_Erase(&erase,&error);
    HAL_FLASH_Lock();
    return status==HAL_OK;
}
bool NvmPort_Erase(uint32_t slot)
{
    if(slot>1U || !NvmPort_CanWrite()) return false;
    return erase_page((NVM_BASE-FLASH_BASE)/FLASH_PAGE_SIZE+slot);
}
static bool program(uint32_t address,const void *data,uint32_t size)
{
    if ((address&7U) || (size&7U) || HAL_FLASH_Unlock()!=HAL_OK) return false;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    bool ok=true;
    const uint8_t *source=data;
    for(uint32_t i=0;i<size;i+=8U)
    {
        uint64_t value;
        memcpy(&value,source+i,8);
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,address+i,value)!=HAL_OK) {ok=false;break;}
    }
    HAL_FLASH_Lock();
    return ok;
}
bool NvmPort_Program(uint32_t slot,uint32_t offset,const void *data,uint32_t size)
{
    if(slot>1U || offset>NVM_PAGE_BYTES || size>NVM_PAGE_BYTES-offset) return false;
    return program(NVM_BASE+slot*NVM_PAGE_BYTES+offset,data,size);
}
bool NvmPort_LoadLegacy(void *data)
{
    memset(data,0xFF,NVM_PAYLOAD_BYTES);
    return read_flash(LEGACY_CONTEXT,data,NVM_CONTEXT_BYTES) &&
           read_flash(LEGACY_CONFIG,(uint8_t *)data+NVM_CONFIG_OFFSET,8U);
}
bool NvmPort_WasMigrated(void)
{
    uint64_t marker;
    /* Any programmed/torn migration marker is treated as migrated. */
    return !read_flash(MIGRATION_ADDR,&marker,8U) || marker!=UINT64_MAX;
}
bool NvmPort_MarkMigrated(void)
{
    if(NvmPort_WasMigrated()) return true;
    uint64_t marker=MIGRATED;
    if(program(MIGRATION_ADDR,&marker,8U)) return NvmPort_WasMigrated();
    /* A board upgraded from the legacy layout has this doubleword already
     * programmed with all ones: the old flash_if rewrote whole pages, tail
     * included. It reads erased but cannot be programmed a second time.
     * Erasing its page is safe here, because the caller only marks after
     * committing the snapshot that supersedes the legacy context. */
    if(!NvmPort_CanWrite()) return false;
    if(!erase_page((LEGACY_CONTEXT-FLASH_BASE)/FLASH_PAGE_SIZE)) return false;
    return program(MIGRATION_ADDR,&marker,8U) && NvmPort_WasMigrated();
}
bool NvmPort_CanWrite(void)
{
    MAX17048_Data_t battery;
    return MAX17048_Read(&battery)==MAX17048_OK && battery.voltage_mv>=POWER_STOP_MV;
}
