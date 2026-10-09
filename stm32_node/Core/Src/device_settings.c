#include "device_settings.h"
#include <stddef.h>

static void put16(uint8_t *b, uint16_t v) { b[0] = (uint8_t)(v >> 8); b[1] = (uint8_t)v; }
static uint16_t get16(const uint8_t *b) { return (uint16_t)((b[0] << 8) | b[1]); }

/* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF. */
static uint16_t crc16(const uint8_t *b, unsigned n)
{
    uint16_t crc = 0xFFFFU;
    while (n--)
    {
        crc ^= (uint16_t)(*b++ << 8);
        for (unsigned bit = 0; bit < 8U; ++bit)
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
    return crc;
}

/* interval + four thresholds, the order of downlink, record and report. */
static void put_values(uint8_t *b, const device_settings_t *s)
{
    put16(b, s->interval_s);
    put16(b + 2, s->th.save);
    put16(b + 4, s->th.recovery);
    put16(b + 6, s->th.standby);
    put16(b + 8, s->th.resume);
}

static void get_values(const uint8_t *b, device_settings_t *s)
{
    s->interval_s = get16(b);
    s->th = (power_thresholds_t){get16(b + 2), get16(b + 4), get16(b + 6), get16(b + 8)};
}

void DeviceSettings_Defaults(device_settings_t *s, uint16_t interval_s)
{
    *s = (device_settings_t){.revision = 0U, .interval_s = interval_s, .th = POWER_DEFAULT_THRESHOLDS};
}

bool DeviceSettings_IntervalValid(uint32_t interval_s)
{
    return interval_s >= SETTINGS_INTERVAL_MIN_S && interval_s <= SETTINGS_INTERVAL_MAX_S;
}

bool DeviceSettings_Valid(const device_settings_t *s)
{
    return DeviceSettings_IntervalValid(s->interval_s) && PowerPolicy_Validate(&s->th);
}

void DeviceSettings_EncodeRecord(const device_settings_t *s, uint8_t out[SETTINGS_RECORD_LEN])
{
    out[0] = SETTINGS_RECORD_VERSION;
    out[1] = s->revision;
    put_values(out + 2, s);
    out[12] = 0U;
    out[13] = 0U;
    put16(out + 14, crc16(out, 14U));
}

settings_load_t DeviceSettings_DecodeRecord(const uint8_t in[SETTINGS_RECORD_LEN], device_settings_t *s)
{
    bool erased = true;
    for (unsigned i = 0; i < SETTINGS_RECORD_LEN; ++i) erased = erased && in[i] == 0xFFU;
    if (erased) return SETTINGS_EMPTY;
    if (in[0] != SETTINGS_RECORD_VERSION || get16(in + 14) != crc16(in, 14U)) return SETTINGS_CORRUPT;
    device_settings_t loaded = {.revision = in[1]};
    get_values(in + 2, &loaded);
    if (!DeviceSettings_Valid(&loaded)) return SETTINGS_CORRUPT;
    *s = loaded;
    return SETTINGS_LOADED;
}

bool DeviceSettings_ParseDownlink(const uint8_t *payload, uint8_t size, device_settings_t *s)
{
    if (payload == NULL || size != SETTINGS_DOWNLINK_LEN || payload[0] != SETTINGS_CMD || payload[1] == 0U)
        return false;
    device_settings_t parsed = {.revision = payload[1]};
    get_values(payload + 2, &parsed);
    if (!DeviceSettings_Valid(&parsed)) return false;
    *s = parsed;
    return true;
}

void DeviceSettings_BuildReport(const device_settings_t *s, const settings_report_info_t *info,
                                uint8_t out[SETTINGS_REPORT_LEN])
{
    out[0] = SETTINGS_REPORT_LAYOUT;
    out[1] = info->fw_major;
    out[2] = info->fw_minor;
    out[3] = info->hardware;
    out[4] = s->revision;
    out[5] = (uint8_t)info->mode;
    put_values(out + 6, s);
    put16(out + 16, info->flags);
}
