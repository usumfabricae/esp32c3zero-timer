import React, { useState, useEffect } from 'react'
import ScheduleGrid from './ScheduleGrid.jsx'
import Notification from './Notification.jsx'
import './TimerProgramming.css'

function TimerProgramming({ ble }) {
  const { deviceData, isConnected, readSchedule } = ble;
  const [localSchedule, setLocalSchedule] = useState(null);
  const [modified, setModified] = useState(false);
  const [loading, setLoading] = useState(false);
  const [notification, setNotification] = useState(null);

  // Read schedule when component mounts
  useEffect(() => {
    const loadSchedule = async () => {
      if (isConnected && !deviceData.schedule) {
        setLoading(true);
        try {
          await readSchedule();
        } catch (error) {
          console.error('Failed to read schedule:', error);
          setNotification({
            type: 'error',
            message: `Failed to load schedule: ${error.message}`
          });
        } finally {
          setLoading(false);
        }
      }
    };

    loadSchedule();
  }, [isConnected, deviceData.schedule, readSchedule]);

  // Initialize local schedule from device data
  useEffect(() => {
    if (deviceData.schedule && !localSchedule) {
      setLocalSchedule([...deviceData.schedule]);
      setModified(false);
    }
  }, [deviceData.schedule, localSchedule]);

  // Handle cell click to cycle through modes
  const handleCellClick = (day, hour) => {
    if (!localSchedule) {
      return;
    }

    // Cycle through H -> L -> O -> H
    const currentMode = localSchedule[day][hour];
    let nextMode;
    if (currentMode === 'H') {
      nextMode = 'L';
    } else if (currentMode === 'L') {
      nextMode = 'O';
    } else {
      nextMode = 'H';
    }

    // Update local schedule
    const newSchedule = [...localSchedule];
    newSchedule[day] = 
      newSchedule[day].substring(0, hour) + 
      nextMode + 
      newSchedule[day].substring(hour + 1);
    
    setLocalSchedule(newSchedule);
    setModified(true);
  };

  // Handle save - write schedule for each modified day
  const handleSave = async () => {
    if (!localSchedule || !isConnected) {
      return;
    }

    // Show loading notification
    setNotification({
      type: 'loading',
      message: 'Saving schedule...'
    });

    try {
      // Write schedule for each day (0-6)
      for (let day = 0; day < 7; day++) {
        await ble.writeSchedule(day, localSchedule[day]);
      }

      // Show success notification
      setNotification({
        type: 'success',
        message: 'Schedule saved successfully!',
        duration: 3000
      });

      // Clear modified flag
      setModified(false);
    } catch (error) {
      console.error('Failed to save schedule:', error);
      
      // Show error notification
      setNotification({
        type: 'error',
        message: `Failed to save schedule: ${error.message}`
      });

      // Retain previous schedule on failure - revert local changes
      if (deviceData.schedule) {
        setLocalSchedule([...deviceData.schedule]);
        setModified(false);
      }
    }
  };

  const closeNotification = () => {
    setNotification(null);
  };

  if (loading) {
    return (
      <div className="timer-programming">
        <div className="loading-container">
          <div className="loading-spinner"></div>
          <p>Loading schedule...</p>
        </div>
      </div>
    );
  }

  if (!isConnected) {
    return (
      <div className="timer-programming">
        <div className="not-connected">
          <p>Not connected to device. Please connect to view and edit the schedule.</p>
        </div>
      </div>
    );
  }

  if (!localSchedule) {
    return (
      <div className="timer-programming">
        <div className="no-data">
          <p>No schedule data available. Please try refreshing.</p>
        </div>
      </div>
    );
  }

  return (
    <div className="timer-programming">
      <div className="timer-programming-header">
        <h2>Timer Programming</h2>
        <p className="instructions">
          Click on any cell to cycle through modes: H (High) → L (Low) → O (Off)
        </p>
      </div>

      <ScheduleGrid 
        schedule={localSchedule}
        onCellClick={handleCellClick}
      />

      {modified && (
        <div className="save-section">
          <button 
            className="save-button"
            onClick={handleSave}
            disabled={!isConnected}
          >
            Save Schedule
          </button>
          <p className="save-hint">
            You have unsaved changes. Click "Save Schedule" to write them to the device.
          </p>
        </div>
      )}

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

export default TimerProgramming
