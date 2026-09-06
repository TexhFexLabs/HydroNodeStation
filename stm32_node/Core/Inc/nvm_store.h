#ifndef NVM_STORE_H
#define NVM_STORE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define NVM_CONTEXT_BYTES 0x2D8U
#define NVM_CONFIG_OFFSET NVM_CONTEXT_BYTES
#define NVM_PAYLOAD_BYTES 768U
#define NVM_PAGE_BYTES 2048U
/* Two new pages at 0x0803E000/0x0803E800. Last two pages remain legacy input. */
#define NVM_BASE 0x0803E000UL
/* Which step of the last Nvm_* call failed. Reported by the fault blink code,
 * because a bare "NVM failed" cannot be acted on in the field.
 * 1 is reserved for the caller's own context-range rejection. */
enum { NVM_ERR_NONE=0, NVM_ERR_CONTEXT=1, NVM_ERR_ARGS=2, NVM_ERR_POWER=3,
       NVM_ERR_ERASE=4, NVM_ERR_PROGRAM=5, NVM_ERR_VERIFY=6, NVM_ERR_COMMIT=7,
       NVM_ERR_CONFIRM=8, NVM_ERR_MIGRATION=9, NVM_ERR_LEGACY=10 };
uint32_t Nvm_LastError(void);
bool Nvm_Init(void);
bool Nvm_Read(uint32_t offset, void *data, uint32_t size);
bool Nvm_Write(uint32_t offset, const void *data, uint32_t size);
bool NvmPort_Read(uint32_t slot, uint32_t offset, void *data, uint32_t size);
bool NvmPort_Erase(uint32_t slot);
bool NvmPort_Program(uint32_t slot, uint32_t offset, const void *data, uint32_t size);
bool NvmPort_LoadLegacy(void *data);
bool NvmPort_WasMigrated(void);
bool NvmPort_MarkMigrated(void);
bool NvmPort_CanWrite(void);
bool NvmPort_HandleEcc(void);
#endif
