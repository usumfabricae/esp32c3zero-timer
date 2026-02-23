import { useState } from 'react'
import TemperatureDisplay from './TemperatureDisplay.jsx'
import RelayGauge from './RelayGauge.jsx'
import BatteryGauge from './BatteryGauge.jsx'
import Notification from './Notification.jsx'
import './Dashboard.css'

function Dashboard({ ble }) {
  const { deviceData, isConnected } = ble;
  const [notification, setNotification] = useState(null);
  const [isReloading, setIsReloading] = useState(false);
  
  // Debug logging
  console.log('[Dashboard] deviceData:', deviceData);

  // Get today's day of week (0 = Monday, 6 = Sunday)
  const getTodaySchedule = () => {
    if (!deviceData.schedule || deviceData.schedule.length === 0) {
      return null;
    }

    const now = new Date();
    // JavaScript getDay() returns 0=Sunday, 1=Monday, etc.
    // Convert to 0=Monday, 6=Sunday
    let dayIndex = now.getDay() - 1;
    if (dayIndex === -1) dayIndex = 6; // Sunday

    // Check if we have data for today
    if (dayIndex < deviceData.schedule.length) {
      const scheduleString = deviceData.schedule[dayIndex];
      // Ensure it's a valid 24-character string
      if (scheduleString && scheduleString.length === 24) {
        return scheduleString;
      }
    }
    
    return null;
  };

  // Get the current day name
  const getTodayDayName = () => {
    const days = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
    const now = new Date();
    return days[now.getDay()];
  };

  const todaySchedule = getTodaySchedule();
  const currentHour = new Date().getHours();

  const handleRelayToggle = async (newState, durationMinutes) => {
    // Show loading notification
    setNotification({
      type: 'loading',
      message: `Turning relay ${newState ? 'ON' : 'OFF'}...`
    });

    try {
      await ble.writeRelayState(newState, durationMinutes);
      
      // Show success notification
      setNotification({
        type: 'success',
        message: `Relay turned ${newState ? 'ON' : 'OFF'} for ${durationMinutes} minutes`,
        duration: 3000
      });

      // Show manual override notification
      setTimeout(() => {
        setNotification({
          type: 'info',
          message: `Manual override active for ${durationMinutes} minutes. Automatic scheduling suspended.`,
          duration: 5000
        });
      }, 3500);
    } catch (error) {
      // Show error notification
      setNotification({
        type: 'error',
        message: `Failed to control relay: ${error.message}`
      });
    }
  };

  const closeNotification = () => {
    setNotification(null);
  };

  const handleReloadData = async () => {
    if (!isConnected) {
      setNotification({
        type: 'error',
        message: 'Not connected to device'
      });
      return;
    }

    setIsReloading(true);
    setNotification({
      type: 'loading',
      message: 'Reloading data from device...'
    });

    try {
      // Read all data from device
      await Promise.all([
        ble.readTemperature(),
        ble.readRelayState(),
        ble.readSchedule(),
        ble.readTemperatureThresholds(),
        ble.readBatteryLevel().catch(err => console.warn('Battery level not available:', err))
      ]);

      setNotification({
        type: 'success',
        message: 'Data reloaded successfully',
        duration: 3000
      });
    } catch (error) {
      setNotification({
        type: 'error',
        message: `Failed to reload data: ${error.message}`
      });
    } finally {
      setIsReloading(false);
    }
  };

  return (
    <div className="dashboard">
      <div className="dashboard-header">
        <button
          className={`reload-button ${isReloading ? 'reloading' : ''}`}
          onClick={handleReloadData}
          disabled={!isConnected || isReloading}
          title="Reload data from device"
        >
          <svg className="reload-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M21.5 2v6h-6M2.5 22v-6h6M2 11.5a10 10 0 0 1 18.8-4.3M22 12.5a10 10 0 0 1-18.8 4.2"/>
          </svg>
          Reload Data
        </button>
      </div>
      <div className="dashboard-grid">
        <div className="dashboard-section temperature-section">
          <TemperatureDisplay temperature={deviceData.temperature} />
        </div>

        <div className="dashboard-section relay-section">
          <RelayGauge
            state={deviceData.relayState}
            overrideEndTime={deviceData.relayOverrideEndTime}
            onToggle={handleRelayToggle}
            disabled={!isConnected}
          />
        </div>

        <div className="dashboard-section battery-section">
          <BatteryGauge
            batteryLevel={deviceData.batteryLevel}
            voltage={deviceData.batteryVoltage}
          />
        </div>

        <div className="dashboard-section thresholds-section">
          <h3 className="section-title">Temperature Thresholds</h3>
          <div className="thresholds-display">
            <div className="threshold-item high">
              <span className="threshold-label">High:</span>
              <span className="threshold-value">
                {deviceData.thresholds?.high !== null && deviceData.thresholds?.high !== undefined
                  ? `${deviceData.thresholds.high.toFixed(1)}°C`
                  : '--'}
              </span>
            </div>
            <div className="threshold-item low">
              <span className="threshold-label">Low:</span>
              <span className="threshold-value">
                {deviceData.thresholds?.low !== null && deviceData.thresholds?.low !== undefined
                  ? `${deviceData.thresholds.low.toFixed(1)}°C`
                  : '--'}
              </span>
            </div>
          </div>
        </div>

        <div className="dashboard-section schedule-section">
          <h3 className="section-title">Today Schedule: {getTodayDayName()}</h3>
          {todaySchedule ? (
            <div className="schedule-display">
              <div className="schedule-grid-today">
                {todaySchedule.split('').map((mode, hour) => (
                  <div
                    key={hour}
                    className={`schedule-cell ${mode.toLowerCase()} ${hour === currentHour ? 'current-hour' : ''}`}
                    title={`${hour}:00 - ${mode === 'H' ? 'High' : mode === 'L' ? 'Low' : 'Off'}`}
                  >
                    <div className="hour-label">{hour}</div>
                    <div className="mode-label">{mode}</div>
                  </div>
                ))}
              </div>
              <div className="schedule-legend">
                <div className="legend-item">
                  <span className="legend-color h"></span>
                  <span>H = High</span>
                </div>
                <div className="legend-item">
                  <span className="legend-color l"></span>
                  <span>L = Low</span>
                </div>
                <div className="legend-item">
                  <span className="legend-color o"></span>
                  <span>O = Off</span>
                </div>
              </div>
            </div>
          ) : (
            <div className="no-data">No schedule data available</div>
          )}
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

export default Dashboard
