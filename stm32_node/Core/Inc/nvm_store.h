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
