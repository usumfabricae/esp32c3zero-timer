import React from 'react'
import './Header.css'

function Header({ isConnected, onMenuClick, onConnect }) {
  return (
    <header className="header">
      <button 
        className="hamburger-menu" 
        onClick={onMenuClick}
        aria-label="Toggle menu"
      >
        <span className="hamburger-icon">☰</span>
      </button>
      
      <h1 className="app-title">ESP32 Timer Control</h1>
      
      <div className="header-right">
        <div className={`connection-status ${isConnected ? 'connected' : 'disconnected'}`}>
          <span className="status-indicator"></span>
          <span className="status-text">
            {isConnected ? 'Connected' : 'Disconnected'}
          </span>
        </div>
        
        {!isConnected && (
          <button 
            className="connect-button" 
            onClick={onConnect}
          >
            Connect
          </button>
        )}
      </div>
    </header>
  )
}

export default Header
