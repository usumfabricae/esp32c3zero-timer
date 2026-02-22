import React from 'react'
import './ScheduleGrid.css'

function ScheduleGrid({ schedule, onCellClick }) {
  const dayNames = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday', 'Sunday'];
  const currentHour = new Date().getHours();
  const currentDay = (() => {
    const now = new Date();
    let dayIndex = now.getDay() - 1;
    if (dayIndex === -1) dayIndex = 6; // Sunday
    return dayIndex;
  })();

  // Handle cell click to cycle through modes: H -> L -> O -> H
  const handleCellClick = (day, hour) => {
    if (onCellClick) {
      onCellClick(day, hour);
    }
  };

  // Get mode for a specific day and hour
  const getMode = (day, hour) => {
    if (!schedule || !schedule[day]) {
      return 'O';
    }
    return schedule[day][hour] || 'O';
  };

  return (
    <div className="schedule-grid-container">
      <div className="schedule-grid">
        {/* Header row with hour labels */}
        <div className="schedule-header">
          <div className="schedule-corner">Day / Hour</div>
          {Array.from({ length: 24 }, (_, hour) => (
            <div key={hour} className="schedule-hour-label">
              {hour}
            </div>
          ))}
        </div>

        {/* Rows for each day */}
        {dayNames.map((dayName, day) => (
          <div key={day} className="schedule-row">
            <div className="schedule-day-label">{dayName}</div>
            {Array.from({ length: 24 }, (_, hour) => {
              const mode = getMode(day, hour);
              const isCurrentHour = day === currentDay && hour === currentHour;
              
              return (
                <div
                  key={hour}
                  className={`schedule-cell ${mode.toLowerCase()} ${isCurrentHour ? 'current-hour' : ''}`}
                  onClick={() => handleCellClick(day, hour)}
                  title={`${dayName} ${hour}:00 - ${mode === 'H' ? 'High' : mode === 'L' ? 'Low' : 'Off'}`}
                >
                  {mode}
                </div>
              );
            })}
          </div>
        ))}
      </div>

      {/* Legend */}
      <div className="schedule-legend">
        <div className="legend-item">
          <span className="legend-color h"></span>
          <span>H = High (Relay ON if temp below threshold)</span>
        </div>
        <div className="legend-item">
          <span className="legend-color l"></span>
          <span>L = Low (Relay OFF if temp above threshold)</span>
        </div>
        <div className="legend-item">
          <span className="legend-color o"></span>
          <span>O = Off (Relay always OFF)</span>
        </div>
      </div>
    </div>
  )
}

export default ScheduleGrid
