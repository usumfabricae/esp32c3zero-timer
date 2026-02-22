import React from 'react'
import './TemperatureDisplay.css'

function TemperatureDisplay({ temperature }) {
  const displayValue = temperature !== null && temperature !== undefined 
    ? temperature.toFixed(1) 
    : '--';

  return (
    <div className="temperature-display">
      <div className="temperature-label">Current Temperature</div>
      <div className="temperature-value">
        <span className="temperature-number">{displayValue}</span>
        <span className="temperature-unit">°C</span>
      </div>
    </div>
  )
}

export default TemperatureDisplay
