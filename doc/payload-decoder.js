/* HydroNodeStation01 decoder: environmental ports preserve their fixed layout. */
function decodeUplink(input) {
  const bytes = input.bytes;
  const port = input.fPort;
  const lengths = {2: 10, 3: 12, 4: 32, 5: 22};
  if (!Array.isArray(bytes) || bytes.length !== lengths[port] ||
      bytes.some(b => !Number.isInteger(b) || b < 0 || b > 255)) {
    return {errors: ["Invalid HydroNode port, length or byte value"]};
  }
  const u16 = i => bytes[i] * 256 + bytes[i + 1];
  const u32 = i => u16(i) * 65536 + u16(i + 2);
  const value = (i, scale = 1) => u16(i) === 65535 ? null : u16(i) / scale;
  if (port === 5) {
    if (bytes[0] !== 1) return {errors: ["Unsupported diagnostic version"]};
    return {data: {version: bytes[0], power_mode: bytes[1], uptime_s: u32(2),
      boot_count: u32(6), reset_flags: u32(10), last_fault: u16(14),
      tx_errors: u16(16), sensor_errors: u16(18), sensor_valid_mask: u16(20)}};
  }
  const rawTemperature = u16(0);
  const data = {
    temperature: rawTemperature === 32768 ? null :
      (rawTemperature >= 32768 ? rawTemperature - 65536 : rawTemperature) / 100,
    humidity: value(2, 100), pressure: value(4, 10), battery_mv: value(6), uvi: value(8, 100)
  };
  if (port >= 3) data.co2_ppm = value(10);
  if (port === 4) {
    const names = ["pm1_0", "pm2_5", "pm4_0", "pm10", "nc0_5", "nc1_0", "nc2_5", "nc4_0", "nc10"];
    names.forEach((name, i) => { data[name] = value(12 + i * 2, 10); });
    data.typ_size_um = value(30, 1000);
  }
  return {data};
}
if (typeof module !== "undefined") module.exports = {decodeUplink};
