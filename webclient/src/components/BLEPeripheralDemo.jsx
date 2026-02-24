import { useEffect, useState } from 'react';
import { useBLEPeripheral } from '../hooks/useBLEPeripheral';
import './BLEPeripheralDemo.css';

function BLEPeripheralDemo() {
  const peripheral = useBLEPeripheral();
  const [deviceName, setDeviceName] = useState('ESP32C3_Timer');
  const [isInitialized, setIsInitialized] = useState(false);

  // Initialize on mount
  useEffect(() => {
    const init = async () => {
      const success = await peripheral.initialize();
      setIsInitialized(success);
    };
    init();
  }, []);

  const handleStartAdvertising = async () => {
    try {
      await peripheral.startAdvertising(deviceName);
    } catch (err) {
      console.error('Failed to start advertising:', err);
    }
  };

  const handleStopAdvertising = async () => {
    try {
      await peripheral.stopAdvertising();
    } catch (err) {
      console.error('Failed to stop advertising:', err);
    }
  };

  const handleUpdateTemperature = async () => {
    const temp = 18 + Math.random() * 10; // Random temp between 18-28°C
    await peripheral.updateTemperature(temp);
  };

  const handleUpdateBattery = async () => {
    const level = Math.floor(Math.random() * 100);
    await peripheral.updateBatteryLevel(level);
  };

  const handleUpdateTime = async () => {
    await peripheral.updateCurrentTime(new Date());
  };

  return (
    <div className="ble-peripheral-demo">
      <h2>BLE Peripheral Demo</h2>

      {peripheral.error && (
        <div className="error-message">
          Error: {peripheral.error}
        </div>
      )}

      <div className="status-section">
        <h3>Status</h3>
        <div className="status-grid">
          <div className="status-item">
            <span className="label">Initialized:</span>
            <span className={`value ${isInitialized ? 'success' : 'error'}`}>
              {isInitialized ? '✓' : '✗'}
            </span>
          </div>
          <div className="status-item">
            <span className="label">Advertising:</span>
            <span className={`value ${peripheral.isAdvertising ? 'success' : 'error'}`}>
              {peripheral.isAdvertising ? '✓' : '✗'}
            </span>
          </div>
          <div className="status-item">
            <span className="label">Connected:</span>
            <span className={`value ${peripheral.isConnected ? 'success' : 'error'}`}>
              {peripheral.isConnected ? '✓' : '✗'}
            </span>
          </div>
          {peripheral.connectedDevice && (
            <div className="status-item full-width">
              <span className="label">Device:</span>
              <span className="value">{peripheral.connectedDevice}</span>
            </div>
          )}
        </div>
      </div>

      <div className="control-section">
        <h3>Advertising Control</h3>
        <div className="input-group">
          <label htmlFor="deviceName">Device Name:</label>
          <input
            id="deviceName"
            type="text"
            value={deviceName}
            onChange={(e) => setDeviceName(e.target.value)}
            disabled={peripheral.isAdvertising}
          />
        </div>
        <div className="button-group">
          <button
            onClick={handleStartAdvertising}
            disabled={!isInitialized || peripheral.isAdvertising}
            className="btn-primary"
          >
            Start Advertising
          </button>
          <button
            onClick={handleStopAdvertising}
            disabled={!peripheral.isAdvertising}
            className="btn-secondary"
          >
            Stop Advertising
          </button>
        </div>
      </div>

      <div className="device-state-section">
        <h3>Device State</h3>
        <div className="state-grid">
          <div className="state-item">
            <span className="label">Temperature:</span>
            <span className="value">
              {peripheral.deviceState.temperature.toFixed(1)}°C
            </span>
          </div>
          <div className="state-item">
            <span className="label">Battery:</span>
            <span className="value">
              {peripheral.deviceState.batteryLevel}%
            </span>
          </div>
          <div className="state-item">
            <span className="label">Relay State:</span>
            <span className="value">
              {['Off', 'Low', 'High'][peripheral.deviceState.relayState]}
            </span>
          </div>
          <div className="state-item">
            <span className="label">Time:</span>
            <span className="value">
              {peripheral.deviceState.currentTime.toLocaleTimeString()}
            </span>
          </div>
        </div>
      </div>

      <div className="update-section">
        <h3>Update Values</h3>
        <div className="button-group">
          <button
            onClick={handleUpdateTemperature}
            disabled={!peripheral.isConnected}
            className="btn-update"
          >
            Update Temperature
          </button>
          <button
            onClick={handleUpdateBattery}
            disabled={!peripheral.isConnected}
            className="btn-update"
          >
            Update Battery
          </button>
          <button
            onClick={handleUpdateTime}
            disabled={!peripheral.isConnected}
            className="btn-update"
          >
            Update Time
          </button>
        </div>
        <p className="hint">
          {peripheral.isConnected
            ? 'Click buttons to update values and send notifications'
            : 'Connect a BLE client to enable updates'}
        </p>
      </div>

      <div className="info-section">
        <h3>How to Test</h3>
        <ol>
          <li>Click "Start Advertising" to make this device discoverable</li>
          <li>Use nRF Connect app on another device to scan and connect</li>
          <li>Explore the services and characteristics</li>
          <li>Subscribe to notifications for temperature, time, and battery</li>
          <li>Click update buttons to send notifications to connected client</li>
          <li>Try writing to relay state or thresholds from nRF Connect</li>
        </ol>
      </div>
    </div>
  );
}

export default BLEPeripheralDemo;
