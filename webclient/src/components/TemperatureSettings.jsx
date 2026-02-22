import React, { useState, useEffect } from 'react'
import Notification from './Notification.jsx'
import './TemperatureSettings.css'

function TemperatureSettings({ ble }) {
  const { deviceData, isConnected, readTemperatureThresholds } = ble;
  
  // State for threshold inputs
  const [high, setHigh] = useState('');
  const [low, setLow] = useState('');
  
  // State for calibration input
  const [calibration, setCalibration] = useState('');
  
  // State for notifications
  const [notification, setNotification] = useState(null);

  // Read thresholds when component mounts or when connected
  useEffect(() => {
    const loadThresholds = async () => {
      if (isConnected) {
        try {
          await readTemperatureThresholds();
        } catch (error) {
          console.error('Failed to read thresholds:', error);
        }
      }
    };

    loadThresholds();
  }, [isConnected, readTemperatureThresholds]);

  // Update input fields when deviceData.thresholds changes
  useEffect(() => {
    if (deviceData.thresholds) {
      setHigh(deviceData.thresholds.high.toString());
      setLow(deviceData.thresholds.low.toString());
    }
  }, [deviceData.thresholds]);

  const closeNotification = () => {
    setNotification(null);
  };

  // Handler for saving temperature thresholds
  const handleSaveThresholds = async () => {
    const highValue = parseFloat(high);
    const lowValue = parseFloat(low);

    // Validate that high > low
    if (isNaN(highValue) || isNaN(lowValue)) {
      setNotification({
        type: 'error',
        message: 'Please enter valid temperature values'
      });
      return;
    }

    if (highValue <= lowValue) {
      setNotification({
        type: 'error',
        message: 'High temperature must be greater than low temperature'
      });
      return;
    }

    // Show loading notification
    setNotification({
      type: 'loading',
      message: 'Saving temperature thresholds...'
    });

    try {
      await ble.writeTemperatureThresholds(highValue, lowValue);
      
      // Show success notification
      setNotification({
        type: 'success',
        message: 'Temperature thresholds saved successfully',
        duration: 3000
      });
    } catch (error) {
      // Show error notification
      setNotification({
        type: 'error',
        message: `Failed to save thresholds: ${error.message}`
      });
    }
  };

  // Handler for calibrating temperature
  const handleCalibrate = async () => {
    const calibrationValue = parseFloat(calibration);

    if (isNaN(calibrationValue)) {
      setNotification({
        type: 'error',
        message: 'Please enter a valid calibration temperature'
      });
      return;
    }

    // Show loading notification
    setNotification({
      type: 'loading',
      message: 'Calibrating temperature sensor...'
    });

    try {
      await ble.writeTemperatureCalibration(calibrationValue);
      
      // Clear input after successful calibration
      setCalibration('');
      
      // Show success notification
      setNotification({
        type: 'success',
        message: 'Temperature sensor calibrated successfully',
        duration: 3000
      });
    } catch (error) {
      // Show error notification
      setNotification({
        type: 'error',
        message: `Failed to calibrate sensor: ${error.message}`
      });
    }
  };

  // Handler for resetting calibration
  const handleResetCalibration = async () => {
    // Show loading notification
    setNotification({
      type: 'loading',
      message: 'Resetting temperature calibration...'
    });

    try {
      // Write -999 to reset calibration
      await ble.writeTemperatureCalibration(-999);
      
      // Show success notification
      setNotification({
        type: 'success',
        message: 'Temperature calibration reset successfully',
        duration: 3000
      });
    } catch (error) {
      // Show error notification
      setNotification({
        type: 'error',
        message: `Failed to reset calibration: ${error.message}`
      });
    }
  };

  return (
    <div className="temperature-settings">
      <h2 className="page-title">Temperature Settings</h2>

      <div className="settings-container">
        {/* Temperature Thresholds Section */}
        <div className="settings-section">
          <h3 className="section-title">Temperature Thresholds</h3>
          <p className="section-description">
            Set the high and low temperature limits for automatic relay control.
          </p>

          <div className="form-group">
            <label htmlFor="high-temp">
              High Temperature (°C)
            </label>
            <input
              id="high-temp"
              type="number"
              step="0.1"
              value={high}
              onChange={(e) => setHigh(e.target.value)}
              disabled={!isConnected}
              placeholder="e.g., 22.0"
            />
          </div>

          <div className="form-group">
            <label htmlFor="low-temp">
              Low Temperature (°C)
            </label>
            <input
              id="low-temp"
              type="number"
              step="0.1"
              value={low}
              onChange={(e) => setLow(e.target.value)}
              disabled={!isConnected}
              placeholder="e.g., 18.0"
            />
          </div>

          <button
            className="btn btn-primary"
            onClick={handleSaveThresholds}
            disabled={!isConnected}
          >
            Save Thresholds
          </button>
        </div>

        {/* Temperature Calibration Section */}
        <div className="settings-section">
          <h3 className="section-title">Temperature Calibration</h3>
          <p className="section-description">
            Calibrate the temperature sensor by entering the actual temperature.
          </p>

          <div className="form-group">
            <label htmlFor="calibration-temp">
              Actual Temperature (°C)
            </label>
            <input
              id="calibration-temp"
              type="number"
              step="0.1"
              value={calibration}
              onChange={(e) => setCalibration(e.target.value)}
              disabled={!isConnected}
              placeholder="e.g., 20.5"
            />
          </div>

          <div className="button-group">
            <button
              className="btn btn-primary"
              onClick={handleCalibrate}
              disabled={!isConnected || !calibration}
            >
              Calibrate
            </button>
            <button
              className="btn btn-secondary"
              onClick={handleResetCalibration}
              disabled={!isConnected}
            >
              Reset Calibration
            </button>
          </div>
        </div>
      </div>

      {notification && (
        <Notification
          type={notification.type}
          message={notification.message}
          duration={notification.duration}
          onClose={closeNotification}
        />
      )}
    </div>
  )
}

export default TemperatureSettings
