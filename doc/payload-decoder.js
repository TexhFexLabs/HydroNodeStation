/* HydroNodeStation decoder, firmware 2.0 (PCB 1.1).
 * Every port has a fixed length. Ports 2 and 3 end in a 22-byte block
 * (solar, fuel gauge, board temperature, counters); port 4 is unchanged.
 * Firmware 1.x frames (port 2: 10 bytes, port 3: 12 bytes) still decode,
 * without the block, so a station keeping its DevEUI stays readable. */
function decodeUplink(input) {
  const bytes = input.bytes;
  const port = input.fPort;
  const lengths = {2: [32, 10], 3: [34, 12], 4: [32], 5: [26]};
  if (!Array.isArray(bytes) || !lengths[port] || !lengths[port].includes(bytes.length) ||
      bytes.some(b => !Number.isInteger(b) || b < 0 || b > 255)) {
    return {errors: ["Invalid HydroNode port, length or byte value"]};
  }
  const u16 = i => bytes[i] * 256 + bytes[i + 1];
  const u32 = i => u16(i) * 65536 + u16(i + 2);
  const value = (i, scale = 1) => u16(i) === 65535 ? null : u16(i) / scale;
  const signed = (i, scale) => u16(i) === 32768 ? null :
    (u16(i) >= 32768 ? u16(i) - 65536 : u16(i)) / scale;
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
