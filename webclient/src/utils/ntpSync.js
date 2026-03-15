/**
 * NTP Time Synchronization Utility
 * 
 * Fetches accurate time from public time APIs.
 * Falls back to phone time if all servers are unavailable.
 */

// Public time API endpoints (in order of preference)
// Note: ESP32 uses 0.it.pool.ntp.org via SNTP protocol, but mobile apps need HTTP APIs
const NTP_ENDPOINTS = [
  'https://timeapi.io/api/Time/current/zone?timeZone=Europe/Rome', // timeapi.io - Italy timezone
  'https://worldtimeapi.org/api/timezone/Europe/Rome', // WorldTimeAPI - Italy timezone
  'https://worldtimeapi.org/api/ip', // WorldTimeAPI - Auto-detect timezone
];

/**
 * Fetch current time from time server
 * @returns {Promise<Date>} Current time from server
 * @throws {Error} If all time servers fail
 */
export const fetchNTPTime = async () => {
  console.log('[NTP] Fetching time from time server...');
  
  for (const endpoint of NTP_ENDPOINTS) {
    try {
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 5000); // 5 second timeout
      
      const response = await fetch(endpoint, {
        signal: controller.signal,
        cache: 'no-store'
      });
      
      clearTimeout(timeoutId);
      
      if (!response.ok) {
        console.warn(`[NTP] Server ${endpoint} returned ${response.status}`);
        continue;
      }
      
      const data = await response.json();
      
      // Handle timeapi.io format
      if (data.dateTime) {
        const ntpTime = new Date(data.dateTime);
        console.log(`[NTP] Time fetched successfully from ${endpoint}:`, ntpTime.toISOString());
        return ntpTime;
      }
      
      // Handle WorldTimeAPI format
      if (data.datetime) {
        const ntpTime = new Date(data.datetime);
        console.log(`[NTP] Time fetched successfully from ${endpoint}:`, ntpTime.toISOString());
        return ntpTime;
      }
      
      console.warn(`[NTP] Invalid response from ${endpoint}:`, data);
    } catch (error) {
      console.warn(`[NTP] Failed to fetch from ${endpoint}:`, error.message);
      // Continue to next endpoint
    }
  }
  
  // All time servers failed
  throw new Error('All time servers unavailable');
};

/**
 * Get current time with NTP fallback to phone time
 * @returns {Promise<{time: Date, source: 'ntp'|'phone'}>}
 */
export const getCurrentTime = async () => {
  try {
    const ntpTime = await fetchNTPTime();
    return {
      time: ntpTime,
      source: 'ntp'
    };
  } catch (error) {
    console.warn('[NTP] Falling back to phone time:', error.message);
    return {
      time: new Date(),
      source: 'phone'
    };
  }
};

/**
 * Calculate milliseconds until 3 seconds before next minute
 * @param {Date} currentTime - Current time (from NTP or phone)
 * @returns {number} Milliseconds until XX:XX:57
 */
export const getMillisecondsUntilScanTime = (currentTime) => {
  const seconds = currentTime.getSeconds();
  const milliseconds = currentTime.getMilliseconds();
  
  // We want to start scanning at XX:XX:57
  const targetSecond = 57;
  let secondsUntilTarget;
  
  if (seconds < targetSecond) {
    // Next scan is in this minute
    secondsUntilTarget = targetSecond - seconds;
  } else {
    // Next scan is in next minute
    secondsUntilTarget = (60 - seconds) + targetSecond;
  }
  
  // Convert to milliseconds and subtract current milliseconds
  return (secondsUntilTarget * 1000) - milliseconds;
};
