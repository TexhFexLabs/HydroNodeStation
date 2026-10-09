/* HydroNodeStation decoder, firmware 2.1 (PCB 1.1).
 * Every port has a fixed length. Ports 2 and 3 end in a 22-byte block
 * (solar, fuel gauge, board temperature, counters); port 4 is unchanged.
 * Firmware 1.x frames (port 2: 10 bytes, port 3: 12 bytes) still decode,
 * without the block, so a station keeping its DevEUI stays readable.
 * Port 99 carries 1 to 8 fault/event entries of 4 bytes (code u16 + detail
 * u16); the code table mirrors stm32_node/Core/Inc/fault_codes.h.
 * Port 6 (firmware 2.1) is the settings report: interval and battery
 * thresholds in effect, 18 bytes (stm32_node/Core/Inc/device_settings.h). */
const PARTS = {1: "SHT45", 2: "BMP390", 3: "LTR390", 4: "SCD41", 5: "SPS30", 6: "MAX17048",
  7: "INA226", 8: "Solar ADC", 9: "Board temperature sensor", 16: "I2C bus"};
const EVENTS = {0x0320: "Power save mode", 0x0321: "Recovery mode, radio off",
  0x0323: "Battery reading invalid", 0x0430: "Rejoined after the link check",
  0x0431: "Uplink not sent", 0x0432: "Uplink request rejected", 0x0540: "Restart",
  0x0541: "Firmware version", 0x0542: "LSE crystal started late", 0x0543: "NVM error",
  0x0544: "Option byte IWDG_STDBY not set, no Standby", 0x054f: "Fault queue overflow"};
const RESTART = ["none", "HAL error", "CPU fault", "modem", "no progress", "NVM", "LSE",
  "command FF", "Standby wake-up"];
const RESET_FLAGS = ["pin", "brown-out", "software", "IWDG", "WWDG", "low-power", "option byte"];
const COMMAND_RESULTS = ["executed", "length or parameter invalid",
  "refused because of battery or power mode", "could not be saved", "unknown command",
  "reply not accepted by the modem", "execution failed"];
const SETTINGS_RESULTS = {0: "applied", 1: "refused, values invalid", 3: "could not be saved"};
const POWER_STATES = ["NORMAL", "SAVE", "RECOVERY", "STANDBY"];

function faultEntry(code, detail) {
  const base = code & 0x7fff, category = (code >> 8) & 0x7f, id = code & 0xff;
  const entry = {code: "0x" + code.toString(16).toUpperCase().padStart(4, "0"),
    category, resolved: (code & 0x8000) !== 0, state: category >= 1 && category <= 3, detail};
  if ((category === 1 || category === 2) && PARTS[id]) {
    entry.part = PARTS[id];
    entry.text = PARTS[id] + (category === 1
      ? (entry.resolved ? " found again" : " not found at start")
      : (entry.resolved ? " reads again" : " read errors, 3 in a row"));
    if (category === 1 && !entry.resolved && detail) entry.i2c_address = detail & 0x7f;
  } else if (base === 0x0540) {
    entry.text = "Restart";
    entry.reason = RESTART[detail >> 8] || "reason " + (detail >> 8);
    entry.reset_flags = RESET_FLAGS.filter((_, bit) => detail & (1 << bit));
  } else if (base === 0x0541) {
    entry.text = "Firmware version";
    entry.version = (detail >> 8) + "." + (detail & 0xff);
  } else if (base === 0x0614) {
    entry.text = "Device settings " + (SETTINGS_RESULTS[detail] || "result " + detail);
  } else if (category === 6) {
    entry.text = "Command 0x" + id.toString(16).toUpperCase().padStart(2, "0") + " " +
      (COMMAND_RESULTS[detail] || "result " + detail);
  } else if (EVENTS[base]) {
    entry.text = EVENTS[base];
  } else {
    entry.text = "Code " + entry.code + ", detail " + detail;
  }
  if (category === 3) entry.battery_mv = detail === 65535 ? null : detail;
  return entry;
}

function decodeUplink(input) {
  const bytes = input.bytes;
  const port = input.fPort;
  const lengths = {2: [32, 10], 3: [34, 12], 4: [32], 5: [26], 6: [18],
    99: [4, 8, 12, 16, 20, 24, 28, 32]};
  if (!Array.isArray(bytes) || !lengths[port] || !lengths[port].includes(bytes.length) ||
      bytes.some(b => !Number.isInteger(b) || b < 0 || b > 255)) {
    return {errors: ["Invalid HydroNode port, length or byte value"]};
  }
  const u16 = i => bytes[i] * 256 + bytes[i + 1];
  const u32 = i => u16(i) * 65536 + u16(i + 2);
  const value = (i, scale = 1) => u16(i) === 65535 ? null : u16(i) / scale;
  const signed = (i, scale) => u16(i) === 32768 ? null :
    (u16(i) >= 32768 ? u16(i) - 65536 : u16(i)) / scale;
  if (port === 99) {
    const entries = [];
    for (let i = 0; i < bytes.length; i += 4) entries.push(faultEntry(u16(i), u16(i + 2)));
    return {data: {entries}};
  }
  if (port === 6) {
    if (bytes[0] !== 1) return {errors: ["Unsupported settings report layout"]};
    return {data: {layout: bytes[0], firmware: bytes[1] + "." + bytes[2], hardware: bytes[3],
      revision: bytes[4], power_state: POWER_STATES[bytes[5]] || "state " + bytes[5],
      interval_s: u16(6), save_mv: u16(8), recovery_mv: u16(10), standby_mv: u16(12),
      resume_mv: u16(14), iwdg_stdby_ok: (bytes[17] & 1) !== 0,
      from_nvm: (bytes[17] & 2) !== 0, pulse_counters: (bytes[17] & 4) !== 0}};
  }
  if (port === 5) {
    if (bytes[0] !== 1) return {errors: ["Unsupported diagnostic version"]};
    return {data: {version: bytes[0], power_mode: bytes[1], uptime_s: u32(2),
      boot_count: u32(6), reset_flags: u32(10),
      last_fault: bytes[14], last_fault_detail: bytes[15],
      tx_errors: u16(16), sensor_errors: u16(18), sensor_valid_mask: u16(20),
      modem_panics: u16(22), nvm_errors: bytes[24], last_nvm_error: bytes[25]}};
  }
  const data = {
    temperature: signed(0, 100),
    humidity: value(2, 100), pressure: value(4, 10), battery_mv: value(6), uvi: value(8, 100)
  };
  if (port >= 3) data.co2_ppm = value(10);
  if (port === 4) {
    const names = ["pm1_0", "pm2_5", "pm4_0", "pm10", "nc0_5", "nc1_0", "nc2_5", "nc4_0", "nc10"];
    names.forEach((name, i) => { data[name] = value(12 + i * 2, 10); });
    data.typ_size_um = value(30, 1000);
  } else if (bytes.length > 12) {
    const b = port === 2 ? 10 : 12;
    data.solar_mv = value(b);
    data.solar_ma = value(b + 2, 10);
    data.solar_mw = value(b + 4);
    data.solar_mwh = value(b + 6, 10);
    data.sun_s = value(b + 8);
    data.solar_adc_mv = value(b + 10);
    data.soc = value(b + 12, 100);
    data.charge_rate = signed(b + 14, 100);
    data.board_temperature = signed(b + 16, 100);
    data.counter1 = value(b + 18);
    data.counter2 = value(b + 20);
  }
  return {data};
}
if (typeof module !== "undefined") module.exports = {decodeUplink};
