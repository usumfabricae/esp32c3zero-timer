import { useState, useEffect } from 'react';
import './Settings.css';

function Settings({ 
  isConnected, 
  wifiSsid,
  onUpdateWifiSsid,
  onUpdateWifiPassword,
  onUpdateBlePasskey
}) {
  const [ssid, setSsid] = useState('');
  const [password, setPassword] = useState('');
  const [passkey, setPasskey] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [showPasskey, setShowPasskey] = useState(false);
  const [notification, setNotification] = useState(null);

  useEffect(() => {
    if (wifiSsid) {
      setSsid(wifiSsid);
    }
  }, [wifiSsid]);

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

  if (!isConnected) {
    return (
      <div className="settings">
        <div className="settings-disconnected">
          <p>Connect to device to access settings</p>
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
