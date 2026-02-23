import React, { useState } from 'react'
import './RelayGauge.css'

function RelayGauge({ state, overrideEndTime, onToggle, disabled }) {
  const isOn = state === true || state === 1;
  const [duration, setDuration] = useState(60); // Default 60 minutes

  const handleToggle = () => {
    if (!disabled && onToggle) {
      onToggle(!isOn, duration);
    }
  };

  const durationOptions = [
    { value: 15, label: '15 min' },
    { value: 30, label: '30 min' },
    { value: 60, label: '1 hour' },
    { value: 120, label: '2 hours' },
    { value: 180, label: '3 hours' },
    { value: 240, label: '4 hours' },
  ];

  return (
    <div className="relay-gauge">
      <div className="relay-label">Relay Control</div>
      
      <div className={`relay-visual ${isOn ? 'on' : 'off'}`}>
        <div className="relay-indicator">
          <div className="relay-indicator-inner"></div>
        </div>
        <div className="relay-status-text">
          {isOn ? 'ON' : 'OFF'}
        </div>
      </div>

      {overrideEndTime && (
        <div className="relay-override-info">
          <span className="override-label">Override until:</span>
          <span className="override-time">{overrideEndTime}</span>
        </div>
      )}
      
      <div className="relay-controls">
        <div className="duration-selector">
          <label htmlFor="duration-select">Duration:</label>
          <select 
            id="duration-select"
            value={duration} 
            onChange={(e) => setDuration(Number(e.target.value))}
            disabled={disabled}
          >
            {durationOptions.map(opt => (
              <option key={opt.value} value={opt.value}>
                {opt.label}
              </option>
            ))}
          </select>
        </div>

        <div className="relay-toggle-container">
          <label className="toggle-switch">
            <input
              type="checkbox"
              checked={isOn}
              onChange={handleToggle}
              disabled={disabled}
              aria-label="Toggle relay"
            />
            <span className="toggle-slider"></span>
          </label>
          <span className="toggle-label">
            {disabled ? 'Disconnected' : 'Manual Control'}
          </span>
        </div>
      </div>
    </div>
  )
}

export default RelayGauge
