#include "device_settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Bytes from tests/vectors/station-settings-vectors.json, the file the backend
 * parser (hydronode-backend, StationSettingsReportTest) reads as well. */
static const uint8_t REPORT_FACTORY[] = {0x01,0x02,0x01,0x02,0x00,0x00,0x01,0x2C,0x0D,0xAC,0x0C,0xE4,0x0C,0x80,0x0E,0x10,0x00,0x01};
static const uint8_t REPORT_REV7_SAVE[] = {0x01,0x02,0x01,0x02,0x07,0x01,0x02,0x58,0x0D,0x7A,0x0C,0xB2,0x0C,0x1C,0x0D,0xDE,0x00,0x03};
static const uint8_t REPORT_REV255_RECOVERY[] = {0x01,0x02,0x01,0x02,0xFF,0x02,0x0E,0x10,0x0D,0xAC,0x0C,0xE4,0x0C,0x80,0x0E,0x10,0x00,0x07};
static const uint8_t DOWNLINK_REV7[] = {0x14,0x07,0x02,0x58,0x0D,0x7A,0x0C,0xB2,0x0C,0x1C,0x0D,0xDE};
static const uint8_t DOWNLINK_REV1[] = {0x14,0x01,0x01,0x2C,0x0D,0xAC,0x0C,0xE4,0x0C,0x80,0x0E,0x10};

static int same(const device_settings_t *a, const device_settings_t *b)
{
    return a->revision == b->revision && a->interval_s == b->interval_s && a->th.save == b->th.save &&
           a->th.recovery == b->th.recovery && a->th.standby == b->th.standby && a->th.resume == b->th.resume;
}

static settings_report_info_t info(power_mode_t mode, uint16_t flags)
{
    return (settings_report_info_t){.fw_major = 2, .fw_minor = 1, .hardware = 2, .mode = mode, .flags = flags};
}

int main(void)
{
    device_settings_t s, d;
    uint8_t out[SETTINGS_REPORT_LEN];

    /* Report bytes are the backend vectors. */
    DeviceSettings_Defaults(&s, 300);
    settings_report_info_t i = info(POWER_NORMAL, SETTINGS_FLAG_IWDG_STDBY);
    DeviceSettings_BuildReport(&s, &i, out);
    assert(memcmp(out, REPORT_FACTORY, sizeof out) == 0);
    assert(DeviceSettings_ParseDownlink(DOWNLINK_REV7, sizeof DOWNLINK_REV7, &d));
    assert(d.revision == 7 && d.interval_s == 600 && d.th.save == 3450 && d.th.recovery == 3250);
    assert(d.th.standby == 3100 && d.th.resume == 3550);
    i = info(POWER_SAVE, SETTINGS_FLAG_IWDG_STDBY | SETTINGS_FLAG_FROM_NVM);
    DeviceSettings_BuildReport(&d, &i, out);
    assert(memcmp(out, REPORT_REV7_SAVE, sizeof out) == 0);
    s.revision = 255; s.interval_s = 3600;
    i = info(POWER_RECOVERY, SETTINGS_FLAG_IWDG_STDBY | SETTINGS_FLAG_FROM_NVM | SETTINGS_FLAG_PULSE);
    DeviceSettings_BuildReport(&s, &i, out);
    assert(memcmp(out, REPORT_REV255_RECOVERY, sizeof out) == 0);
    assert(DeviceSettings_ParseDownlink(DOWNLINK_REV1, sizeof DOWNLINK_REV1, &d));
    assert(d.revision == 1 && d.interval_s == 300 && d.th.resume == 3600);

    /* Refused downlinks leave the target untouched. */
    uint8_t bad[SETTINGS_DOWNLINK_LEN];
    memcpy(bad, DOWNLINK_REV7, sizeof bad);
    d = s;
    assert(!DeviceSettings_ParseDownlink(bad, sizeof bad - 1U, &d));      /* 11 bytes */
    assert(!DeviceSettings_ParseDownlink(bad, 0, &d));
    assert(!DeviceSettings_ParseDownlink(NULL, sizeof bad, &d));
    bad[1] = 0;  assert(!DeviceSettings_ParseDownlink(bad, sizeof bad, &d));  /* revision 0 */
    bad[1] = 7; bad[2] = 0; bad[3] = 29;                                       /* 29 s */
    assert(!DeviceSettings_ParseDownlink(bad, sizeof bad, &d));
    bad[2] = 0x0E; bad[3] = 0x11;                                              /* 3601 s */
    assert(!DeviceSettings_ParseDownlink(bad, sizeof bad, &d));
    bad[2] = 0x02; bad[3] = 0x58; bad[9] = 0x6A;                               /* standby 3178 ok */
    assert(DeviceSettings_ParseDownlink(bad, sizeof bad, &d));
    d = s;
    bad[8] = 0x0C; bad[9] = 0x9F;                                              /* standby 3231, recovery 3250 */
    assert(!DeviceSettings_ParseDownlink(bad, sizeof bad, &d));
    assert(same(&d, &s));

    /* NVM record: round trip, erased, CRC, version, rules. */
    uint8_t rec[SETTINGS_RECORD_LEN];
    assert(DeviceSettings_ParseDownlink(DOWNLINK_REV7, sizeof DOWNLINK_REV7, &d));
    DeviceSettings_EncodeRecord(&d, rec);
    device_settings_t loaded;
    assert(DeviceSettings_DecodeRecord(rec, &loaded) == SETTINGS_LOADED);
    assert(same(&loaded, &d));
    memset(rec, 0xFF, sizeof rec);
    assert(DeviceSettings_DecodeRecord(rec, &loaded) == SETTINGS_EMPTY);
    DeviceSettings_EncodeRecord(&d, rec);
    rec[5] ^= 0x01;
    assert(DeviceSettings_DecodeRecord(rec, &loaded) == SETTINGS_CORRUPT);
    DeviceSettings_EncodeRecord(&d, rec);
    rec[0] = 2;
    assert(DeviceSettings_DecodeRecord(rec, &loaded) == SETTINGS_CORRUPT);
    device_settings_t broken = d;
    broken.th.standby = broken.th.recovery;                                   /* valid CRC, broken rule */
    DeviceSettings_EncodeRecord(&broken, rec);
    assert(DeviceSettings_DecodeRecord(rec, &loaded) == SETTINGS_CORRUPT);
    assert(same(&loaded, &d));                                                 /* untouched on failure */
    /* Factory record: version 1, revision 0, interval, reserved zeros. */
    DeviceSettings_Defaults(&s, 180);
    DeviceSettings_EncodeRecord(&s, rec);
    assert(rec[0] == 1 && rec[1] == 0 && rec[2] == 0x00 && rec[3] == 0xB4 && rec[12] == 0 && rec[13] == 0);
    assert(DeviceSettings_DecodeRecord(rec, &loaded) == SETTINGS_LOADED && loaded.interval_s == 180);

    assert(DeviceSettings_IntervalValid(30) && DeviceSettings_IntervalValid(3600));
    assert(!DeviceSettings_IntervalValid(29) && !DeviceSettings_IntervalValid(3601));
    puts("Device settings: report = backend vectors, downlink 0x14 parsing and refusals, NVM record round trip, erased, CRC, version and rules passed");
    return 0;
}
