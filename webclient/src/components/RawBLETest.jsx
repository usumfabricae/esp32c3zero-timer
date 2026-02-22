import React, { useState } from 'react';
import { useBLE } from '../hooks/useBLE';
import DataFormatter from '../utils/dataFormatter';

// Helper function to convert ArrayBuffer to hex string
const bufferToHex = (buffer) => {
  return Array.from(new Uint8Array(buffer))
    .map(b => b.toString(16).padStart(2, '0'))
    .join(' ');
};

// Helper function to format decoded data for display
const formatDecodedData = (name, dataView) => {
  try {
    switch (name) {
      case 'Temperature':
        return `${DataFormatter.decodeTemperature(dataView).toFixed(2)}°C`;
      
      case 'Time': {
        const time = DataFormatter.decodeCurrentTime(dataView);
        const dayNames = ['', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
        return `${time.year}-${String(time.month).padStart(2, '0')}-${String(time.day).padStart(2, '0')} ${String(time.hour).padStart(2, '0')}:${String(time.minute).padStart(2, '0')}:${String(time.second).padStart(2, '0')} (${dayNames[time.dayOfWeek]})`;
      }
      
      case 'Relay State':
        return dataView.getUint8(0) === 0x01 ? 'ON' : 'OFF';
      
      case 'Schedule': {
        // For schedule, we need to decode the text first
        const decoder = new TextDecoder('utf-8');
        const text = decoder.decode(dataView.buffer);
        const schedule = DataFormatter.decodeSchedule(text);
        return `${schedule.length} days`;
      }
      
      case 'Thresholds': {
        const thresholds = DataFormatter.decodeTemperatureThresholds(dataView);
        return `High: ${thresholds.high.toFixed(2)}°C, Low: ${thresholds.low.toFixed(2)}°C`;
      }
      
      case 'Calibration':
        return `${DataFormatter.decodeTemperature(dataView).toFixed(2)}°C`;
      
      default:
        return 'N/A';
    }
  } catch (err) {
    return `Error: ${err.message}`;
  }
};

const RawBLETest = () => {
  const ble = useBLE();
  const [readResults, setReadResults] = useState({});
  const [operationLog, setOperationLog] = useState([]);

  const log = (message) => {
    const timestamp = new Date().toLocaleTimeString();
    const logEntry = `[${timestamp}] ${message}`;
    console.log(logEntry);
    setOperationLog(prev => [...prev, logEntry]);
  };

  const readCharacteristic = async (uuid, name) => {
    try {
      log(`Reading ${name} (${uuid})...`);
      const char = ble.characteristics[uuid];
      if (!char) {
        log(`ERROR: Characteristic ${name} not found`);
        return;
      }

      const value = await char.readValue();
      const hexString = bufferToHex(value.buffer);
      const formatted = formatDecodedData(name, value);
      const result = {
        hex: hexString,
        bytes: value.byteLength,
        dataView: value,
        formatted: formatted
      };

      setReadResults(prev => ({
        ...prev,
        [name]: result
      }));

      log(`${name} read: ${hexString} (${value.byteLength} bytes) = ${formatted}`);
    } catch (err) {
      log(`ERROR reading ${name}: ${err.message}`);
    }
  };

  const writeRelay = async (state) => {
    try {
      const stateStr = state ? 'ON' : 'OFF';
      log(`Writing relay ${stateStr}...`);
      
      const char = ble.characteristics[0xFF02];
      if (!char) {
        log('ERROR: Relay characteristic not found');
        return;
      }

      const value = new Uint8Array([state ? 0x01 : 0x00]);
      await char.writeValue(value);
      
      log(`Relay ${stateStr} written successfully`);
    } catch (err) {
      log(`ERROR writing relay: ${err.message}`);
    }
  };

  return (
    <div style={{ padding: '20px', fontFamily: 'monospace' }}>
      <h1>Raw BLE Test Interface</h1>

      {/* Connection Controls */}
      <div style={{ marginBottom: '20px', padding: '10px', border: '1px solid #ccc' }}>
        <h2>Connection</h2>
        <p>Status: {ble.isConnecting ? 'Connecting...' : ble.isConnected ? 'Connected' : 'Disconnected'}</p>
        {ble.error && <p style={{ color: 'red' }}>Error: {ble.error}</p>}
        
        <button 
          onClick={ble.connect} 
          disabled={ble.isConnected || ble.isConnecting}
          style={{ marginRight: '10px', padding: '10px' }}
        >
          Connect
        </button>
        <button 
          onClick={ble.disconnect} 
          disabled={!ble.isConnected}
          style={{ padding: '10px' }}
        >
          Disconnect
        </button>

        {/* Show available characteristics */}
        {ble.isConnected && (
          <div style={{ marginTop: '10px', padding: '10px', backgroundColor: '#e8f5e9' }}>
            <strong>Available Characteristics:</strong>
            <ul style={{ margin: '5px 0', paddingLeft: '20px' }}>
              {ble.characteristics[0x2A6E] && <li>✓ Temperature (0x2A6E)</li>}
              {ble.characteristics[0x2A2B] && <li>✓ Current Time (0x2A2B)</li>}
              {ble.characteristics[0xFF02] && <li>✓ Relay State (0xFF02)</li>}
              {ble.characteristics[0xFF05] && <li>✓ Schedule (0xFF05)</li>}
              {ble.characteristics[0xFF06] && <li>✓ Temperature Thresholds (0xFF06)</li>}
              {ble.characteristics[0xFF07] && <li>✓ Temperature Calibration (0xFF07)</li>}
            </ul>
            {!ble.characteristics[0xFF07] && (
              <p style={{ color: '#666', fontSize: '12px', margin: '5px 0' }}>
                Note: Temperature Calibration (0xFF07) is not available on this device
              </p>
            )}
          </div>
        )}
      </div>

      {/* Read Operations */}
      {ble.isConnected && (
        <div style={{ marginBottom: '20px', padding: '10px', border: '1px solid #ccc' }}>
          <h2>Read Characteristics</h2>
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '10px', marginBottom: '10px' }}>
            <button 
              onClick={() => readCharacteristic(0x2A6E, 'Temperature')}
              disabled={!ble.characteristics[0x2A6E]}
              title={!ble.characteristics[0x2A6E] ? 'Characteristic not available' : ''}
            >
              Read Temperature (0x2A6E)
            </button>
            <button 
              onClick={() => readCharacteristic(0x2A2B, 'Time')}
              disabled={!ble.characteristics[0x2A2B]}
              title={!ble.characteristics[0x2A2B] ? 'Characteristic not available' : ''}
            >
              Read Time (0x2A2B)
            </button>
            <button 
              onClick={() => readCharacteristic(0xFF02, 'Relay State')}
              disabled={!ble.characteristics[0xFF02]}
              title={!ble.characteristics[0xFF02] ? 'Characteristic not available' : ''}
            >
              Read Relay State (0xFF02)
            </button>
            <button 
              onClick={() => readCharacteristic(0xFF05, 'Schedule')}
              disabled={!ble.characteristics[0xFF05]}
              title={!ble.characteristics[0xFF05] ? 'Characteristic not available' : ''}
            >
              Read Schedule (0xFF05)
            </button>
            <button 
              onClick={() => readCharacteristic(0xFF06, 'Thresholds')}
              disabled={!ble.characteristics[0xFF06]}
              title={!ble.characteristics[0xFF06] ? 'Characteristic not available' : ''}
            >
              Read Thresholds (0xFF06)
            </button>
            <button 
              onClick={() => readCharacteristic(0xFF07, 'Calibration')}
              disabled={!ble.characteristics[0xFF07]}
              title={!ble.characteristics[0xFF07] ? 'Characteristic not available' : ''}
            >
              Read Calibration (0xFF07) {!ble.characteristics[0xFF07] && '(N/A)'}
            </button>
          </div>

          {/* Display Read Results */}
          {Object.keys(readResults).length > 0 && (
            <div style={{ marginTop: '10px', padding: '10px', backgroundColor: '#f0f0f0' }}>
              <h3>Read Results:</h3>
              {Object.entries(readResults).map(([name, result]) => (
                <div key={name} style={{ marginBottom: '15px', padding: '10px', backgroundColor: 'white', borderRadius: '5px' }}>
                  <strong>{name}:</strong>
                  <div style={{ marginLeft: '20px', marginTop: '5px' }}>
                    <div style={{ fontSize: '18px', color: '#2196F3', marginBottom: '5px' }}>
                      📊 {result.formatted}
                    </div>
                    <details style={{ fontSize: '12px', color: '#666' }}>
                      <summary style={{ cursor: 'pointer' }}>Raw Data (debug)</summary>
                      <div style={{ marginLeft: '10px', marginTop: '5px' }}>
                        <div>Hex: {result.hex}</div>
                        <div>Bytes: {result.bytes}</div>
                      </div>
                    </details>
                  </div>
                </div>
              ))}
            </div>
          )}
          
          {/* Display Schedule Grid if available */}
          {readResults['Schedule'] && (() => {
            try {
              const decoder = new TextDecoder('utf-8');
              const text = decoder.decode(readResults['Schedule'].dataView.buffer);
              const schedule = DataFormatter.decodeSchedule(text);
              const dayNames = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'];
              
              return (
                <div style={{ marginTop: '10px', padding: '10px', backgroundColor: '#e3f2fd', borderRadius: '5px' }}>
                  <h3>Schedule Grid (7-Day View):</h3>
                  <div style={{ overflowX: 'auto' }}>
                    <table style={{ borderCollapse: 'collapse', fontSize: '11px', width: '100%' }}>
                      <thead>
                        <tr>
                          <th style={{ border: '1px solid #ccc', padding: '5px', backgroundColor: '#90caf9' }}>Day</th>
                          {Array.from({ length: 24 }, (_, i) => (
                            <th key={i} style={{ border: '1px solid #ccc', padding: '5px', backgroundColor: '#90caf9' }}>
                              {i}
                            </th>
                          ))}
                        </tr>
                      </thead>
                      <tbody>
                        {schedule.map((daySchedule, dayIndex) => (
                          <tr key={dayIndex}>
                            <td style={{ border: '1px solid #ccc', padding: '5px', fontWeight: 'bold', backgroundColor: '#bbdefb' }}>
                              {dayNames[dayIndex]}
                            </td>
                            {Array.from(daySchedule).map((mode, hour) => {
                              const bgColor = mode === 'H' ? '#ffcdd2' : mode === 'L' ? '#fff9c4' : '#e0e0e0';
                              return (
                                <td key={hour} style={{ 
                                  border: '1px solid #ccc', 
                                  padding: '5px', 
                                  textAlign: 'center',
                                  backgroundColor: bgColor,
                                  fontWeight: 'bold'
                                }}>
                                  {mode}
                                </td>
                              );
                            })}
                          </tr>
                        ))}
                      </tbody>
                    </table>
                    <div style={{ marginTop: '10px', fontSize: '12px' }}>
                      <span style={{ backgroundColor: '#ffcdd2', padding: '2px 8px', marginRight: '10px' }}>H = High</span>
                      <span style={{ backgroundColor: '#fff9c4', padding: '2px 8px', marginRight: '10px' }}>L = Low</span>
                      <span style={{ backgroundColor: '#e0e0e0', padding: '2px 8px' }}>O = Off</span>
                    </div>
                  </div>
                </div>
              );
            } catch (err) {
              return <div style={{ color: 'red' }}>Error displaying schedule: {err.message}</div>;
            }
          })()}
        </div>
      )}

      {/* Write Operations */}
      {ble.isConnected && (
        <div style={{ marginBottom: '20px', padding: '10px', border: '1px solid #ccc' }}>
          <h2>Write Relay Control</h2>
          <button 
            onClick={() => writeRelay(true)}
            style={{ marginRight: '10px', padding: '10px', backgroundColor: '#4CAF50', color: 'white' }}
          >
            Relay ON (0x01)
          </button>
          <button 
            onClick={() => writeRelay(false)}
            style={{ padding: '10px', backgroundColor: '#f44336', color: 'white' }}
          >
            Relay OFF (0x00)
          </button>
        </div>
      )}

      {/* Write Schedule */}
      {ble.isConnected && (
        <div style={{ marginBottom: '20px', padding: '10px', border: '1px solid #ccc' }}>
          <h2>Write Schedule</h2>
          <div style={{ marginBottom: '10px' }}>
            <label>
              Day (0-6, Mon-Sun): 
              <input 
                type="number" 
                min="0" 
                max="6" 
                defaultValue="0"
                id="scheduleDay"
                style={{ marginLeft: '10px', padding: '5px', width: '60px' }}
              />
            </label>
          </div>
          <div style={{ marginBottom: '10px' }}>
            <label>
              Schedule (24 chars, H/L/O): 
              <input 
                type="text" 
                maxLength="24"
                defaultValue="HHHHHHHHHHHHHHHHHHHHHHHH"
                id="scheduleString"
                style={{ marginLeft: '10px', padding: '5px', width: '300px', fontFamily: 'monospace' }}
              />
            </label>
          </div>
          <button 
            onClick={async () => {
              try {
                const day = parseInt(document.getElementById('scheduleDay').value);
                const scheduleString = document.getElementById('scheduleString').value;
                
                if (!DataFormatter.validateDay(day)) {
                  log('ERROR: Invalid day (must be 0-6)');
                  return;
                }
                
                if (!DataFormatter.validateScheduleString(scheduleString)) {
                  log('ERROR: Invalid schedule string (must be 24 chars of H/L/O)');
                  return;
                }
                
                log(`Writing schedule for day ${day}: ${scheduleString}...`);
                const char = ble.characteristics[0xFF05];
                if (!char) {
                  log('ERROR: Schedule characteristic not found');
                  return;
                }
                
                const value = DataFormatter.encodeSchedule(day, scheduleString);
                await char.writeValue(value);
                log(`Schedule written successfully for day ${day}`);
              } catch (err) {
                log(`ERROR writing schedule: ${err.message}`);
              }
            }}
            style={{ padding: '10px', backgroundColor: '#2196F3', color: 'white' }}
          >
            Write Schedule
          </button>
        </div>
      )}

      {/* Write Temperature Thresholds */}
      {ble.isConnected && (
        <div style={{ marginBottom: '20px', padding: '10px', border: '1px solid #ccc' }}>
          <h2>Write Temperature Thresholds</h2>
          <div style={{ marginBottom: '10px' }}>
            <label>
              High Temperature (°C): 
              <input 
                type="number" 
                step="0.1"
                defaultValue="22.0"
                id="thresholdHigh"
                style={{ marginLeft: '10px', padding: '5px', width: '80px' }}
              />
            </label>
          </div>
          <div style={{ marginBottom: '10px' }}>
            <label>
              Low Temperature (°C): 
              <input 
                type="number" 
                step="0.1"
                defaultValue="18.0"
                id="thresholdLow"
                style={{ marginLeft: '10px', padding: '5px', width: '80px' }}
              />
            </label>
          </div>
          <button 
            onClick={async () => {
              try {
                const high = parseFloat(document.getElementById('thresholdHigh').value);
                const low = parseFloat(document.getElementById('thresholdLow').value);
                
                if (!DataFormatter.validateTemperatureThresholds(high, low)) {
                  log('ERROR: Invalid thresholds (high must be > low)');
                  return;
                }
                
                log(`Writing thresholds: High=${high}°C, Low=${low}°C...`);
                const char = ble.characteristics[0xFF06];
                if (!char) {
                  log('ERROR: Thresholds characteristic not found');
                  return;
                }
                
                const value = DataFormatter.encodeTemperatureThresholds(high, low);
                await char.writeValue(value);
                log(`Thresholds written successfully`);
              } catch (err) {
                log(`ERROR writing thresholds: ${err.message}`);
              }
            }}
            style={{ padding: '10px', backgroundColor: '#2196F3', color: 'white' }}
          >
            Write Thresholds
          </button>
        </div>
      )}

      {/* Write Temperature Calibration */}
      {ble.isConnected && ble.characteristics[0xFF07] && (
        <div style={{ marginBottom: '20px', padding: '10px', border: '1px solid #ccc' }}>
          <h2>Write Temperature Calibration</h2>
          <div style={{ marginBottom: '10px' }}>
            <label>
              Calibration Temperature (°C): 
              <input 
                type="number" 
                step="0.1"
                defaultValue="20.0"
                id="calibrationTemp"
                style={{ marginLeft: '10px', padding: '5px', width: '80px' }}
              />
            </label>
          </div>
          <button 
            onClick={async () => {
              try {
                const temp = parseFloat(document.getElementById('calibrationTemp').value);
                
                log(`Writing calibration: ${temp}°C...`);
                const char = ble.characteristics[0xFF07];
                if (!char) {
                  log('ERROR: Calibration characteristic not found');
                  return;
                }
                
                const value = DataFormatter.encodeTemperatureCalibration(temp);
                await char.writeValue(value);
                log(`Calibration written successfully`);
              } catch (err) {
                log(`ERROR writing calibration: ${err.message}`);
              }
            }}
            style={{ marginRight: '10px', padding: '10px', backgroundColor: '#2196F3', color: 'white' }}
          >
            Write Calibration
          </button>
          <button 
            onClick={async () => {
              try {
                log(`Resetting calibration (-999)...`);
                const char = ble.characteristics[0xFF07];
                if (!char) {
                  log('ERROR: Calibration characteristic not found');
                  return;
                }
                
                const value = DataFormatter.encodeTemperatureCalibration(-999);
                await char.writeValue(value);
                log(`Calibration reset successfully`);
              } catch (err) {
                log(`ERROR resetting calibration: ${err.message}`);
              }
            }}
            style={{ padding: '10px', backgroundColor: '#ff9800', color: 'white' }}
          >
            Reset Calibration (-999)
          </button>
        </div>
      )}

      {/* Operation Log */}
      <div style={{ padding: '10px', border: '1px solid #ccc' }}>
        <h2>Operation Log</h2>
        <div style={{ 
          maxHeight: '300px', 
          overflowY: 'auto', 
          backgroundColor: '#000', 
          color: '#0f0', 
          padding: '10px',
          fontSize: '12px'
        }}>
          {operationLog.map((entry, index) => (
            <div key={index}>{entry}</div>
          ))}
        </div>
      </div>
    </div>
  );
};

export default RawBLETest;
