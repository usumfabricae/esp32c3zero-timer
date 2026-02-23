/**
 * Utility module for encoding and decoding BLE characteristic values
 * TODO: Implement all encoding/decoding functions
 */

class DataFormatter {
  // Decoders
  /**
   * Decode temperature from 2-byte signed 16-bit little-endian format
   * @param {DataView} dataView - DataView containing 2 bytes
   * @returns {number} Temperature in Celsius
   */
  static decodeTemperature(dataView) {
    const rawValue = dataView.getInt16(0, true) // true = little-endian
    return rawValue / 100.0
  }

  /**
   * Decode current time from 10-byte Bluetooth SIG Current Time format
   * @param {DataView} dataView - DataView containing 10 bytes
   * @returns {Object} Object with year, month, day, hour, minute, second, dayOfWeek
   */
  static decodeCurrentTime(dataView) {
    const year = dataView.getUint16(0, true)    // Bytes 0-1: Year (little-endian)
    const month = dataView.getUint8(2)          // Byte 2: Month (1-12)
    const day = dataView.getUint8(3)            // Byte 3: Day (1-31)
    const hour = dataView.getUint8(4)           // Byte 4: Hour (0-23)
    const minute = dataView.getUint8(5)         // Byte 5: Minute (0-59)
    const second = dataView.getUint8(6)         // Byte 6: Second (0-59)
    const dayOfWeek = dataView.getUint8(7)      // Byte 7: Day of week (1=Mon, 7=Sun)
    // Bytes 8-9: Fractions256 and Adjust Reason (not used)
    
    return {
      year,
      month,
      day,
      hour,
      minute,
      second,
      dayOfWeek
    }
  }

  /**
   * Decode schedule from CRLF-separated text
   * @param {string} text - Multi-line text with CRLF separators
   * @returns {Array<string>} Array of 7 strings (24 characters each)
   */
  static decodeSchedule(text) {
    // Split by CRLF (\r\n)
    const lines = text.split('\r\n').filter(line => line.length > 0)
    return lines
  }

  /**
   * Decode temperature thresholds from 4-byte format (2x signed 16-bit little-endian)
   * @param {DataView} dataView - DataView containing 4 bytes
   * @returns {Object} Object with high and low temperature values
   */
  static decodeTemperatureThresholds(dataView) {
    const high = dataView.getInt16(0, true) // First 2 bytes - direct value
    const low = dataView.getInt16(2, true)  // Next 2 bytes - direct value
    return { high, low }
  }

  // Encoders
  /**
   * Encode relay state with duration to ASCII string format "x,y"
   * @param {boolean} state - Relay state (true=ON, false=OFF)
   * @param {number} durationMinutes - Duration in minutes (default 60)
   * @returns {Uint8Array} ASCII string "x,y" where x=state, y=duration in 15-min units
   */
  static encodeRelayState(state, durationMinutes = 60) {
    const stateValue = state ? 1 : 0;
    const durationUnits = Math.round(durationMinutes / 15);
    const str = `${stateValue},${durationUnits}`;
    const encoder = new TextEncoder();
    return encoder.encode(str);
  }

  /**
   * Decode relay state from ASCII string format
   * @param {DataView} dataView - DataView containing ASCII string
   * @returns {Object} Object with state (boolean), overrideEndTime (string or null)
   */
  static decodeRelayState(dataView) {
    const decoder = new TextDecoder('utf-8');
    const str = decoder.decode(dataView);
    
    // Format can be "x" or "x,hh:mm"
    const parts = str.split(',');
    const state = parts[0] === '1';
    const overrideEndTime = parts.length > 1 ? parts[1] : null;
    
    return { state, overrideEndTime };
  }

  /**
   * Encode schedule to 25-byte array (day index + 24 characters)
   * @param {number} day - Day index (0-6, Mon-Sun)
   * @param {string} scheduleString - 24-character string (H/L/O)
   * @returns {Uint8Array} 25-byte array
   */
  static encodeSchedule(day, scheduleString) {
    const buffer = new Uint8Array(25)
    buffer[0] = day // First byte is day index
    
    // Next 24 bytes are the schedule characters
    for (let i = 0; i < 24; i++) {
      buffer[i + 1] = scheduleString.charCodeAt(i)
    }
    
    return buffer
  }

  /**
   * Encode temperature thresholds to 4-byte format (2x signed 16-bit little-endian)
   * @param {number} high - High temperature threshold in Celsius
   * @param {number} low - Low temperature threshold in Celsius
   * @returns {Uint8Array} 4-byte array
   */
  static encodeTemperatureThresholds(high, low) {
    const buffer = new ArrayBuffer(4)
    const view = new DataView(buffer)
    const highRaw = Math.round(high) // Direct value, no multiplication
    const lowRaw = Math.round(low)   // Direct value, no multiplication
    view.setInt16(0, highRaw, true) // First 2 bytes
    view.setInt16(2, lowRaw, true)  // Next 2 bytes
    return new Uint8Array(buffer)
  }

  /**
   * Encode temperature calibration value to 2-byte signed 16-bit little-endian format
   * @param {number} temp - Temperature in Celsius (or -999 for reset)
   * @returns {Uint8Array} 2-byte array
   */
  static encodeTemperatureCalibration(temp) {
    const buffer = new ArrayBuffer(2)
    const view = new DataView(buffer)
    const rawValue = Math.round(temp) // Direct value in °C, no multiplication
    view.setInt16(0, rawValue, true) // true = little-endian
    return new Uint8Array(buffer)
  }

  // Validators
  /**
   * Validate schedule string format
   * @param {string} scheduleString - Schedule string to validate
   * @returns {boolean} True if valid (24 characters, each H/L/O)
   */
  static validateScheduleString(scheduleString) {
    if (typeof scheduleString !== 'string') return false
    if (scheduleString.length !== 24) return false
    return /^[HLO]{24}$/.test(scheduleString)
  }

  /**
   * Validate temperature thresholds
   * @param {number} high - High temperature threshold
   * @param {number} low - Low temperature threshold
   * @returns {boolean} True if valid (high > low)
   */
  static validateTemperatureThresholds(high, low) {
    if (typeof high !== 'number' || typeof low !== 'number') return false
    if (isNaN(high) || isNaN(low)) return false
    return high > low
  }

  /**
   * Validate day index
   * @param {number} day - Day index to validate
   * @returns {boolean} True if valid (0-6)
   */
  static validateDay(day) {
    if (typeof day !== 'number') return false
    if (isNaN(day)) return false
    return day >= 0 && day <= 6 && Number.isInteger(day)
  }
}

export default DataFormatter
