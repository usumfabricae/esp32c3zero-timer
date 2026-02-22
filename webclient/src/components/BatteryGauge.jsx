import './BatteryGauge.css'

function BatteryGauge({ batteryLevel, voltage }) {
  // Determine battery status
  const getBatteryStatus = () => {
    if (batteryLevel === null || batteryLevel === undefined) {
      return 'unknown';
    }
    if (batteryLevel < 20) return 'critical';
    if (batteryLevel < 30) return 'low';
    if (batteryLevel < 60) return 'medium';
    return 'good';
  };

  const status = getBatteryStatus();

  return (
    <div className="battery-gauge">
      <h3 className="section-title">Battery Status</h3>
      <div className="battery-container">
        <div className="battery-icon">
          <div className={`battery-body ${status}`}>
            <div 
              className="battery-fill"
              style={{ width: `${batteryLevel || 0}%` }}
            ></div>
          </div>
          <div className="battery-terminal"></div>
        </div>
        <div className="battery-info">
          <div className="battery-percentage">
            {batteryLevel !== null && batteryLevel !== undefined
              ? `${batteryLevel}%`
              : '--'}
          </div>
          {voltage !== null && voltage !== undefined && (
            <div className="battery-voltage">
              {(voltage / 1000).toFixed(2)}V
            </div>
          )}
          <div className={`battery-status ${status}`}>
            {status === 'unknown' && 'Unknown'}
            {status === 'critical' && 'Critical'}
            {status === 'low' && 'Low'}
            {status === 'medium' && 'Medium'}
            {status === 'good' && 'Good'}
          </div>
        </div>
      </div>
    </div>
  )
}

export default BatteryGauge
