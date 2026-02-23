import React from 'react'
import './Sidebar.css'

function Sidebar({ isOpen, currentView, onNavigate }) {
  const menuItems = [
    { id: 'dashboard', label: 'Dashboard', icon: '📊' },
    { id: 'timer', label: 'Timer Programming', icon: '⏰' },
    { id: 'temperature', label: 'Temperature Settings', icon: '🌡️' },
    { id: 'config', label: 'Device Configuration', icon: '⚙️' }
  ];

  return (
    <>
      {isOpen && <div className="sidebar-overlay" onClick={() => onNavigate(currentView)} />}
      <aside className={`sidebar ${isOpen ? 'open' : ''}`}>
        <nav className="sidebar-nav">
          <ul className="menu-list">
            {menuItems.map(item => (
              <li key={item.id}>
                <button
                  className={`menu-item ${currentView === item.id ? 'active' : ''}`}
                  onClick={() => onNavigate(item.id)}
                >
                  <span className="menu-icon">{item.icon}</span>
                  <span className="menu-label">{item.label}</span>
                </button>
              </li>
            ))}
          </ul>
        </nav>
      </aside>
    </>
  )
}

export default Sidebar
