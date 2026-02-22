import React from 'react'
import './RelayGauge.css'

function RelayGauge({ state, onToggle, disabled }) {
  const isOn = state === true || state === 1;

  const handleToggle = () => {
    if (!disabled && onToggle) {
      onToggle(!isOn);
    }
  };

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
  )
}

export default RelayGauge
