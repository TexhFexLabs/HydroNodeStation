#ifndef FAULT_CODES_H
#define FAULT_CODES_H
/* Fault and event codes on fPort 99 (TD_2_0_15, documented in
 * doc/CONFIGURATION.md). Each entry: code u16 + detail u16, big-endian.
 * Code bit 15: 1 = resolved, 0 = occurred. Bits 14..8: category.
 * Bits 7..0: component or event. Wire format: never renumber. */
#define FAULT_RESOLVED            0x8000U
#define FAULT_CODE(cat, id)       ((uint16_t)((((uint16_t)(cat)) << 8) | (uint16_t)(id)))
#define FAULT_CATEGORY(code)      ((uint8_t)(((code) >> 8) & 0x7FU))

/* Categories. 0x01-0x03 are states (reported on entry and on exit),
 * 0x04-0x06 are events (reported once per occurrence). */
#define FAULT_CAT_NOT_FOUND       0x01U  /* detail: 7-bit I2C address; resolved: 0 */
#define FAULT_CAT_READ_ERROR      0x02U  /* detail: HAL_I2C error bits; resolved: failed attempts */
#define FAULT_CAT_SUPPLY          0x03U  /* detail: battery mV, 0xFFFF unknown */
#define FAULT_CAT_RADIO           0x04U
#define FAULT_CAT_SYSTEM          0x05U
#define FAULT_CAT_COMMAND         0x06U  /* id: command byte; detail: result (TD_2_0_16) */

/* Components for categories 0x01 and 0x02. */
#define FAULT_COMP_SHT45          0x01U
#define FAULT_COMP_BMP390         0x02U
#define FAULT_COMP_LTR390         0x03U
#define FAULT_COMP_SCD41          0x04U
#define FAULT_COMP_SPS30          0x05U
#define FAULT_COMP_MAX17048       0x06U
#define FAULT_COMP_INA226         0x07U
#define FAULT_COMP_SOLAR_ADC      0x08U
#define FAULT_COMP_TEMP_SENSOR    0x09U
#define FAULT_COMP_I2C_BUS        0x10U  /* bus recovery failed */
/* ADD YOUR SENSOR: 0x0A-0x0F are free (above 0x10 raise FAULT_COMPONENTS in
 * fault_report.h). Add the id to the backend table DeviceFaultCodes and to
 * PARTS in doc/payload-decoder.js too. See doc/ADDING_SENSORS.md. */

/* Command acknowledgement results (category 0x06 detail, TD_2_0_16).
 * 0x0614 answers the settings command 0x14 (firmware 2.1): 0 applied, 1 invalid,
 * 3 not saved; the fPort 6 settings report follows in every case. */
#define FAULT_CMD_OK              0U  /* executed (0x10: new interval in effect now; 0x14: settings applied) */
#define FAULT_CMD_INVALID         1U  /* length or parameter invalid */
#define FAULT_CMD_REFUSED         2U  /* refused because of battery or power mode */
#define FAULT_CMD_SAVE_FAILED     3U  /* could not be stored */
#define FAULT_CMD_UNKNOWN         4U  /* unknown command byte */
#define FAULT_CMD_NOT_ACCEPTED    5U  /* modem did not accept the reply (0x12) */
#define FAULT_CMD_FAILED          6U  /* execution failed (0x11: SPS30 did not respond) */

/* Supply states (category 0x03). Standby shows up as 0x0540 reason 8. */
#define FAULT_SUPPLY_SAVE         0x20U
#define FAULT_SUPPLY_RECOVERY     0x21U
#define FAULT_SUPPLY_INVALID      0x23U  /* three invalid battery readings */

/* Radio events (category 0x04). */
#define FAULT_RADIO_REJOIN        0x30U  /* detail: days without LinkCheckAns */
#define FAULT_RADIO_TX_FAILED     0x31U  /* detail: TXDONE "not sent" count in the last day */
#define FAULT_RADIO_TX_REJECTED   0x32U  /* detail: rejected uplink requests in the last day */

/* System events (category 0x05). */
#define FAULT_SYS_BOOT            0x40U  /* detail: reason << 8 | compressed reset flags */
#define FAULT_SYS_VERSION         0x41U  /* detail: major << 8 | minor */
#define FAULT_SYS_LSE_RETRY       0x42U  /* detail: attempts */
#define FAULT_SYS_NVM             0x43U  /* detail: last NVM error code */
#define FAULT_SYS_NO_IWDG_STDBY   0x44U  /* option byte IWDG_STDBY not cleared */
#define FAULT_SYS_OVERFLOW        0x4FU  /* detail: dropped entries */

/* 0x0540 detail high byte: restart reason from RTC backup DR6, see
 * runtime_health.h: 0 none, 1 HAL, 2 CPU, 3 modem, 4 no progress, 5 NVM,
 * 6 LSE, 7 command FF, 8 Standby wake-up.
 * Low byte: RCC reset flags, compressed. */
#define FAULT_RST_PIN             (1U << 0)
#define FAULT_RST_BOR             (1U << 1)
#define FAULT_RST_SOFTWARE        (1U << 2)
#define FAULT_RST_IWDG            (1U << 3)
#define FAULT_RST_WWDG            (1U << 4)
#define FAULT_RST_LOW_POWER       (1U << 5)
#define FAULT_RST_OPTION_BYTE     (1U << 6)
#endif
