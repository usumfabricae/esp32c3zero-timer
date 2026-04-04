import { useState, useEffect } from 'react';
import './Settings.css';

function Settings({ 
  isConnected, 
  wifiSsid,
  connectionMethod,
  deviceData,
  onUpdateWifiSsid,
  onUpdateWifiPassword,
  onUpdateBlePasskey,
  onConfigureIoT,
  onCalibrateTemperature,
  onCalibrateBattery
}) {
  const [ssid, setSsid] = useState('');
  const [password, setPassword] = useState('');
  const [passkey, setPasskey] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [showPasskey, setShowPasskey] = useState(false);
  const [notification, setNotification] = useState(null);

  // Calibration state
  const [calibrationTemp, setCalibrationTemp] = useState('');
  const [batteryCalibration, setBatteryCalibration] = useState('');

  // IoT credential state
  const [iotThingName, setIotThingName] = useState('');
  const [iotAccessKeyId, setIotAccessKeyId] = useState('');
  const [iotSecretKey, setIotSecretKey] = useState('');
  const [iotRegion, setIotRegion] = useState('eu-central-1');
  const [iotEndpoint, setIotEndpoint] = useState('');
  const [showSecretKey, setShowSecretKey] = useState(false);
  const [iotConfigured, setIotConfigured] = useState(false);

  useEffect(() => {
    if (wifiSsid) {
      setSsid(wifiSsid);
    }
  }, [wifiSsid]);

  // Load saved IoT config on mount
  useEffect(() => {
    try {
      const saved = localStorage.getItem('iot_config');
      if (saved) {
        const config = JSON.parse(saved);
        setIotThingName(config.thingName || '');
        setIotAccessKeyId(config.credentials?.accessKeyId || '');
        setIotRegion(config.region || 'eu-central-1');
        setIotEndpoint(config.endpoint || '');
        // Don't load secret key into state for security — just mark as configured
        if (config.credentials?.accessKeyId) {
          setIotConfigured(true);
          // Re-apply config to hook
          onConfigureIoT?.(config);
        }
      }
    } catch (e) {
      console.warn('[Settings] Failed to load IoT config:', e);
    }
  }, []);

  const showNotification = (message, type = 'success') => {
    setNotification({ message, type });
    setTimeout(() => setNotification(null), 3000);
  };

  const handleUpdateSsid = async () => {
    if (!ssid.trim()) {
      showNotification('WiFi SSID cannot be empty', 'error');
      return;
    }
    if (ssid.length > 32) {
      showNotification('WiFi SSID must be 32 characters or less', 'error');
      return;
    }

    try {
      await onUpdateWifiSsid(ssid);
      showNotification('WiFi SSID updated successfully');
    } catch (error) {
      showNotification(`Failed to update SSID: ${error.message}`, 'error');
    }
  };

  const handleUpdatePassword = async () => {
    if (!password.trim()) {
      showNotification('WiFi Password cannot be empty', 'error');
      return;
    }
    if (password.length > 64) {
      showNotification('WiFi Password must be 64 characters or less', 'error');
      return;
    }

    try {
      await onUpdateWifiPassword(password);
      showNotification('WiFi Password updated successfully');
      setPassword('');
      setShowPassword(false);
    } catch (error) {
      showNotification(`Failed to update password: ${error.message}`, 'error');
    }
  };

  const handleUpdatePasskey = async () => {
    if (!passkey.trim()) {
      showNotification('BLE Passkey cannot be empty', 'error');
      return;
    }
    if (!/^\d{6}$/.test(passkey)) {
      showNotification('BLE Passkey must be exactly 6 digits', 'error');
      return;
    }

    try {
      await onUpdateBlePasskey(passkey);
      showNotification('BLE Passkey updated successfully. Reconnect to use new passkey.');
      setPasskey('');
      setShowPasskey(false);
    } catch (error) {
      showNotification(`Failed to update passkey: ${error.message}`, 'error');
    }
  };

  const handleSaveIoTConfig = () => {
    if (!iotThingName.trim()) {
      showNotification('Thing Name is required', 'error');
      return;
    }
    if (!iotAccessKeyId.trim() || (!iotSecretKey.trim() && !iotConfigured)) {
      showNotification('AWS Access Key ID and Secret Access Key are required', 'error');
      return;
    }

    const config = {
      thingName: iotThingName.trim(),
      region: iotRegion.trim() || 'eu-central-1',
      ...(iotEndpoint.trim() && { endpoint: iotEndpoint.trim() }),
      credentials: {
        accessKeyId: iotAccessKeyId.trim(),
        // Use new secret if provided, otherwise keep existing
        secretAccessKey: iotSecretKey.trim() || JSON.parse(localStorage.getItem('iot_config') || '{}')?.credentials?.secretAccessKey || ''
      }
    };

    if (!config.credentials.secretAccessKey) {
      showNotification('Secret Access Key is required', 'error');
      return;
    }

    try {
      localStorage.setItem('iot_config', JSON.stringify(config));
      onConfigureIoT?.(config);
      setIotConfigured(true);
      setIotSecretKey(''); // Clear from state after saving
      setShowSecretKey(false);
      showNotification('IoT Core credentials saved. They will be used on next Connect.');
    } catch (e) {
      showNotification(`Failed to save IoT config: ${e.message}`, 'error');
    }
  };

  const handleClearIoTConfig = () => {
    localStorage.removeItem('iot_config');
    setIotThingName('');
    setIotAccessKeyId('');
    setIotSecretKey('');
    setIotRegion('eu-central-1');
    setIotEndpoint('');
    setIotConfigured(false);
    onConfigureIoT?.(null);
    showNotification('IoT Core credentials cleared. Device will use BLE only.');
  };

  // --- Calibration handlers ---

  const handleCalibrateTemperature = async () => {
    const value = parseFloat(calibrationTemp);
    if (isNaN(value)) {
      showNotification('Please enter a valid calibration temperature', 'error');
      return;
    }
    try {
      await onCalibrateTemperature(value);
      setCalibrationTemp('');
      showNotification('Temperature sensor calibrated successfully');
    } catch (error) {
      showNotification(`Failed to calibrate sensor: ${error.message}`, 'error');
    }
  };

  const handleResetTemperatureCalibration = async () => {
    try {
      await onCalibrateTemperature(-999);
      showNotification('Temperature calibration reset successfully');
    } catch (error) {
      showNotification(`Failed to reset calibration: ${error.message}`, 'error');
    }
  };

  const handleCalibrateBattery = async () => {
    const value = parseFloat(batteryCalibration);
    if (isNaN(value)) {
      showNotification('Please enter a valid battery voltage in millivolts', 'error');
      return;
    }
    if (value < 2500 || value > 4500) {
      showNotification('Battery voltage must be between 2500mV and 4500mV', 'error');
      return;
    }
    try {
      await onCalibrateBattery(value);
      setBatteryCalibration('');
      showNotification('Battery sensor calibrated successfully');
    } catch (error) {
      showNotification(`Failed to calibrate battery: ${error.message}`, 'error');
    }
  };

  const handleResetBatteryCalibration = async () => {
    try {
      await onCalibrateBattery(-1);
      showNotification('Battery calibration reset successfully');
    } catch (error) {
      showNotification(`Failed to reset battery calibration: ${error.message}`, 'error');
    }
  };

  const calibrationSections = (
    <>
      <div className="settings-section">
        <h3>🌡️ Temperature Calibration</h3>
        <p className="settings-description">
          Calibrate the temperature sensor by entering the actual temperature.
        </p>
        <div className="settings-field">
          <label htmlFor="calibration-temp">Actual Temperature (°C)</label>
          <input
            id="calibration-temp"
            type="number"
            step="0.1"
            value={calibrationTemp}
            onChange={(e) => setCalibrationTemp(e.target.value)}
            disabled={!isConnected}
            placeholder="e.g., 20.5"
          />
        </div>
        <div className="settings-input-group" style={{ marginTop: '16px' }}>
          <button
            onClick={handleCalibrateTemperature}
            className="settings-button"
            disabled={!isConnected || !calibrationTemp}
          >
            Calibrate
          </button>
          <button
            onClick={handleResetTemperatureCalibration}
            className="settings-button settings-button-danger"
            disabled={!isConnected}
          >
            Reset Calibration
          </button>
        </div>
      </div>

      <div className="settings-section">
        <h3>🔋 Battery Calibration</h3>
        <p className="settings-description">
          Calibrate the battery sensor by entering the actual battery voltage in millivolts.
          Current battery: {deviceData?.batteryVoltage ? `${deviceData.batteryVoltage}mV (${deviceData.batteryLevel}%)` : 'N/A'}
        </p>
        <div className="settings-field">
          <label htmlFor="calibration-battery">Actual Battery Voltage (mV)</label>
          <input
            id="calibration-battery"
            type="number"
            step="1"
            value={batteryCalibration}
            onChange={(e) => setBatteryCalibration(e.target.value)}
            disabled={!isConnected}
            placeholder="e.g., 4200"
          />
          <span className="settings-hint">Typical range: 3000mV (empty) to 4200mV (full)</span>
        </div>
        <div className="settings-input-group" style={{ marginTop: '16px' }}>
          <button
            onClick={handleCalibrateBattery}
            className="settings-button"
            disabled={!isConnected || !batteryCalibration}
          >
            Calibrate
          </button>
          <button
            onClick={handleResetBatteryCalibration}
            className="settings-button settings-button-danger"
            disabled={!isConnected}
          >
            Reset Calibration
          </button>
        </div>
      </div>
    </>
  );

  const iotConfigSection = (
    <div className="settings-section">
      <h3>☁️ AWS IoT Core</h3>
      <p className="settings-description">
        Configure AWS IoT Core credentials for cloud connectivity. When configured, the app will try IoT Core first, then fall back to BLE.
        {connectionMethod === 'iot' && <span className="settings-active-badge">Active</span>}
      </p>

      <div className="settings-field">
        <label htmlFor="iot-thing-name">Thing Name</label>
        <input
          id="iot-thing-name"
          type="text"
          value={iotThingName}
          onChange={(e) => setIotThingName(e.target.value)}
          placeholder="e.g. esp32timer-a1b2c3"
        />
        <span className="settings-hint">AWS IoT Thing name for your device</span>
      </div>

      <div className="settings-field">
        <label htmlFor="iot-access-key">Access Key ID</label>
        <input
          id="iot-access-key"
          type="text"
          value={iotAccessKeyId}
          onChange={(e) => setIotAccessKeyId(e.target.value)}
          placeholder="AKIA..."
        />
      </div>

      <div className="settings-field">
        <label htmlFor="iot-secret-key">Secret Access Key</label>
        <div className="settings-input-group">
          <input
            id="iot-secret-key"
            type={showSecretKey ? 'text' : 'password'}
            value={iotSecretKey}
            onChange={(e) => setIotSecretKey(e.target.value)}
            placeholder={iotConfigured ? '(saved — enter new value to change)' : 'Enter secret access key'}
          />
          <button
            onClick={() => setShowSecretKey(!showSecretKey)}
            className="settings-button-icon"
            title={showSecretKey ? 'Hide' : 'Show'}
          >
            {showSecretKey ? '👁️' : '👁️‍🗨️'}
          </button>
        </div>
      </div>

      <div className="settings-field">
        <label htmlFor="iot-region">Region</label>
        <input
          id="iot-region"
          type="text"
          value={iotRegion}
          onChange={(e) => setIotRegion(e.target.value)}
          placeholder="eu-central-1"
        />
        <span className="settings-hint">AWS region (default: eu-central-1)</span>
      </div>

      <div className="settings-field">
        <label htmlFor="iot-endpoint">Endpoint (optional)</label>
        <input
          id="iot-endpoint"
          type="text"
          value={iotEndpoint}
          onChange={(e) => setIotEndpoint(e.target.value)}
          placeholder="xxxxx-ats.iot.region.amazonaws.com"
        />
        <span className="settings-hint">Leave empty to use default endpoint</span>
      </div>

      <div className="settings-input-group" style={{ marginTop: '16px' }}>
        <button
          onClick={handleSaveIoTConfig}
          className="settings-button"
          disabled={!iotThingName.trim() || !iotAccessKeyId.trim()}
        >
          {iotConfigured ? 'Update Credentials' : 'Save Credentials'}
        </button>
        {iotConfigured && (
          <button
            onClick={handleClearIoTConfig}
            className="settings-button settings-button-danger"
          >
            Clear Credentials
          </button>
        )}
      </div>

      {iotConfigured && (
        <div className="settings-warning" style={{ marginTop: '12px', background: '#d4edda', borderColor: '#28a745', color: '#155724' }}>
          ✅ IoT Core credentials configured. Click Connect to use cloud mode.
        </div>
      )}
    </div>
  );

  if (!isConnected) {
    return (
      <div className="settings">
        <h2>Device Settings</h2>

        {notification && (
          <div className={`settings-notification ${notification.type}`}>
            {notification.message}
          </div>
        )}

        {iotConfigSection}

        <div className="settings-disconnected">
          <p>Connect to device to access calibration, WiFi, and BLE settings</p>
        </div>
      </div>
    );
  }

  return (
    <div className="settings">
      <h2>Device Settings</h2>

      {notification && (
        <div className={`settings-notification ${notification.type}`}>
          {notification.message}
        </div>
      )}

      {calibrationSections}

      <div className="settings-section">
        <h3>WiFi Configuration</h3>
        <p className="settings-description">
          Configure WiFi network credentials. Device will use these settings on next boot.
        </p>

        <div className="settings-field">
          <label htmlFor="wifi-ssid">WiFi SSID</label>
          <div className="settings-input-group">
            <input
              id="wifi-ssid"
              type="text"
              value={ssid}
              onChange={(e) => setSsid(e.target.value)}
              placeholder="Enter WiFi network name"
              maxLength={32}
            />
            <button 
              onClick={handleUpdateSsid}
              className="settings-button"
              disabled={!ssid.trim()}
            >
              Update SSID
            </button>
          </div>
          <span className="settings-hint">Max 32 characters</span>
        </div>

        <div className="settings-field">
          <label htmlFor="wifi-password">WiFi Password</label>
          <div className="settings-input-group">
            <input
              id="wifi-password"
              type={showPassword ? 'text' : 'password'}
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              placeholder="Enter WiFi password"
              maxLength={64}
            />
            <button
              onClick={() => setShowPassword(!showPassword)}
              className="settings-button-icon"
              title={showPassword ? 'Hide password' : 'Show password'}
            >
              {showPassword ? '👁️' : '👁️‍🗨️'}
            </button>
            <button 
              onClick={handleUpdatePassword}
              className="settings-button"
              disabled={!password.trim()}
            >
              Update Password
            </button>
          </div>
          <span className="settings-hint">Max 64 characters</span>
        </div>

        <div className="settings-warning">
          ⚠️ Device must be rebooted for WiFi changes to take effect
        </div>
      </div>

      <div className="settings-section">
        <h3>BLE Security</h3>
        <p className="settings-description">
          Change the BLE pairing passkey. You will need to unpair and re-pair the device.
        </p>

        <div className="settings-field">
          <label htmlFor="ble-passkey">BLE Passkey</label>
          <div className="settings-input-group">
            <input
              id="ble-passkey"
              type={showPasskey ? 'text' : 'password'}
              value={passkey}
              onChange={(e) => setPasskey(e.target.value.replace(/\D/g, '').slice(0, 6))}
              placeholder="Enter 6-digit passkey"
              maxLength={6}
              pattern="\d{6}"
            />
            <button
              onClick={() => setShowPasskey(!showPasskey)}
              className="settings-button-icon"
              title={showPasskey ? 'Hide passkey' : 'Show passkey'}
            >
              {showPasskey ? '👁️' : '👁️‍🗨️'}
            </button>
            <button 
              onClick={handleUpdatePasskey}
              className="settings-button"
              disabled={passkey.length !== 6}
            >
              Update Passkey
            </button>
          </div>
          <span className="settings-hint">Must be exactly 6 digits (000000-999999)</span>
        </div>

        <div className="settings-warning">
          ⚠️ After changing passkey, you must unpair and re-pair the device
        </div>
      </div>

      {iotConfigSection}

      <div className="settings-info">
        <h4>ℹ️ Important Notes</h4>
        <ul>
          <li>All settings are saved to device non-volatile storage</li>
          <li>WiFi credentials are used for NTP time synchronization</li>
          <li>Check device serial monitor for confirmation of changes</li>
          <li>Reboot device after WiFi changes for them to take effect</li>
        </ul>
      </div>
    </div>
  );
}

export default Settings;
