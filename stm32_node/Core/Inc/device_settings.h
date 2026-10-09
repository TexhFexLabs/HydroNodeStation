#ifndef DEVICE_SETTINGS_H
#define DEVICE_SETTINGS_H
/* Send interval and battery thresholds the backend may change (firmware 2.1,
 * Power-Sync). Pure byte work, no HAL: the NVM record, downlink 0x14 and the
 * settings report on fPort 6. All values big endian, voltages pack mV.
 *
 * Downlink fPort 2, 12 bytes:
 *   0 0x14 | 1 revision (1..255) | 2 interval s | 4 save | 6 recovery | 8 standby | 10 resume
 * Report fPort 6, 18 bytes:
 *   0 layout (1) | 1 fw major | 2 fw minor | 3 hardware | 4 revision (0 = factory)
 *   5 power state | 6 interval s | 8 save | 10 recovery | 12 standby | 14 resume | 16 flags */
#include <stdint.h>
#include <stdbool.h>
#include "power_policy.h"

#define SETTINGS_CMD                0x14U
#define SETTINGS_DOWNLINK_LEN       12U
#define SETTINGS_REPORT_PORT        6U
#define SETTINGS_REPORT_LEN         18U
#define SETTINGS_REPORT_LAYOUT      1U
#define SETTINGS_REPORT_PERIOD_S    86400U
#define SETTINGS_RECORD_LEN         16U
#define SETTINGS_RECORD_VERSION     1U
#define SETTINGS_INTERVAL_MIN_S     30U
#define SETTINGS_INTERVAL_MAX_S     3600U

#define SETTINGS_FLAG_IWDG_STDBY    (1U << 0)  /* Standby possible */
#define SETTINGS_FLAG_FROM_NVM      (1U << 1)  /* values came from the stored record */
#define SETTINGS_FLAG_PULSE         (1U << 2)  /* pulse counters built in */

typedef struct {
    uint8_t revision;          /* 0 = factory values, 1..255 = set by downlink 0x14 */
    uint16_t interval_s;
    power_thresholds_t th;
} device_settings_t;

/* What the report says besides the settings. */
typedef struct {
    uint8_t fw_major, fw_minor, hardware;
    power_mode_t mode;
    uint16_t flags;
} settings_report_info_t;

typedef enum { SETTINGS_LOADED, SETTINGS_EMPTY, SETTINGS_CORRUPT } settings_load_t;

void DeviceSettings_Defaults(device_settings_t *s, uint16_t interval_s);
bool DeviceSettings_IntervalValid(uint32_t interval_s);
bool DeviceSettings_Valid(const device_settings_t *s);
void DeviceSettings_EncodeRecord(const device_settings_t *s, uint8_t out[SETTINGS_RECORD_LEN]);
/* EMPTY: erased flash, nothing stored yet. CORRUPT: a record that fails its
 * version, CRC or rules. Only LOADED writes *s. */
settings_load_t DeviceSettings_DecodeRecord(const uint8_t in[SETTINGS_RECORD_LEN], device_settings_t *s);
/* False for a wrong length, revision 0 or values outside the rules; *s is
 * written only on success. */
bool DeviceSettings_ParseDownlink(const uint8_t *payload, uint8_t size, device_settings_t *s);
void DeviceSettings_BuildReport(const device_settings_t *s, const settings_report_info_t *info,
                                uint8_t out[SETTINGS_REPORT_LEN]);
#endif
