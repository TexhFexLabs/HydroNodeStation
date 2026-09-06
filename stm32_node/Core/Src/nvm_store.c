#include "nvm_store.h"
#include <string.h>
#define NVM_MAGIC 0x484E5331U
#define NVM_COMMIT UINT64_C(0x434F4D4D49543121)
typedef struct {
    uint32_t magic, sequence, crc, version;
    uint8_t payload[NVM_PAYLOAD_BYTES];
    uint64_t commit;
} record_t;
_Static_assert(sizeof(record_t) <= NVM_PAGE_BYTES, "NVM record must fit a page");
static record_t current;
static int active = -1;
static bool initialized;
static uint32_t last_error;

uint32_t Nvm_LastError(void) { return last_error; }
static bool fail(uint32_t error) { last_error = error; return false; }

static uint32_t crc(const record_t *r)
{
    uint32_t c = UINT32_MAX;
    const uint8_t *bytes = (const uint8_t *)r;
    for (uint32_t i=0; i<offsetof(record_t, commit); ++i)
    {
        if (i >= offsetof(record_t, crc) && i < offsetof(record_t, version)) continue;
        c ^= bytes[i];
        for (unsigned b=0;b<8;b++) c = (c>>1) ^ (0xEDB88320U & (0U-(c&1U)));
    }
    return ~c;
}
static bool valid(const record_t *r)
{ return r->magic == NVM_MAGIC && r->version == 1U && r->commit == NVM_COMMIT && r->crc == crc(r); }

bool Nvm_Init(void)
{
    record_t a, b;
    bool av = NvmPort_Read(0,0,&a,sizeof a) && valid(&a);
    bool bv = NvmPort_Read(1,0,&b,sizeof b) && valid(&b);
    initialized = false;
    active = -1;
    last_error = NVM_ERR_NONE;
    if (av || bv)
    {
        active = bv && (!av || (int32_t)(b.sequence-a.sequence)>0) ? 1 : 0;
        current = active ? b : a;
    }
    else
    {
        /* Once migrated, never silently roll the DevNonce back to legacy data. */
        if (NvmPort_WasMigrated()) return fail(NVM_ERR_MIGRATION);
        memset(&current, 0xFF, sizeof current);
        if (!NvmPort_LoadLegacy(current.payload)) return fail(NVM_ERR_LEGACY);
        current.sequence = 0U;
    }
    initialized = true;
    return true;
}
bool Nvm_Read(uint32_t offset, void *data, uint32_t size)
{
    if (!initialized || !data || offset > NVM_PAYLOAD_BYTES || size > NVM_PAYLOAD_BYTES-offset)
        return fail(NVM_ERR_ARGS);
    memcpy(data,current.payload+offset,size);
    last_error = NVM_ERR_NONE;
    return true;
}
bool Nvm_Write(uint32_t offset, const void *data, uint32_t size)
{
    if (!initialized || !data || offset > NVM_PAYLOAD_BYTES || size > NVM_PAYLOAD_BYTES-offset)
        return fail(NVM_ERR_ARGS);
    last_error = NVM_ERR_NONE;
    if (active >= 0 && memcmp(current.payload+offset,data,size)==0) return true;
    if (!NvmPort_CanWrite()) return fail(NVM_ERR_POWER);
    record_t next = current;
    memcpy(next.payload+offset,data,size);
    next.magic=NVM_MAGIC; next.version=1U; next.sequence=current.sequence+1U;
    next.crc=crc(&next); next.commit=UINT64_MAX;
    uint32_t slot=active==0 ? 1U : 0U;
    if (!NvmPort_Erase(slot)) return fail(NVM_ERR_ERASE);
    if (!NvmPort_Program(slot,0,&next,offsetof(record_t,commit))) return fail(NVM_ERR_PROGRAM);
    record_t check;
    if (!NvmPort_Read(slot,0,&check,sizeof check) || memcmp(&check,&next,offsetof(record_t,commit)))
        return fail(NVM_ERR_VERIFY);
    next.commit=NVM_COMMIT;
    /* Commit marker is the last programmed doubleword; old page stays intact. */
    if (!NvmPort_Program(slot,offsetof(record_t,commit),&next.commit,sizeof next.commit))
        return fail(NVM_ERR_COMMIT);
    if (!NvmPort_Read(slot,0,&check,sizeof check) || !valid(&check)) return fail(NVM_ERR_CONFIRM);
    current=next; active=(int)slot;
    /* A marker prevents fallback to pre-migration nonces if both new pages fail.
     * The snapshot above is already committed and verified, so the data is safe
     * whatever the marker does. Failing the write here would report a loss that
     * did not happen, and the caller resets the MCU on a reported loss. */
    (void)NvmPort_MarkMigrated();
    return true;
}
