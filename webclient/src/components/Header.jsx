import React from 'react'
import './Header.css'

function Header({ isConnected, isConnecting, onMenuClick, onConnect, onDisconnect }) {
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
        <div className={`connection-status ${isConnected ? 'connected' : isConnecting ? 'connecting' : 'disconnected'}`}>
          <span className="status-indicator"></span>
          <span className="status-text">
            {isConnected ? 'Connected' : isConnecting ? 'Connecting...' : 'Disconnected'}
          </span>
        </div>
        
        <button 
          className={`connect-button ${isConnected ? 'disconnect' : ''} ${isConnecting ? 'connecting' : ''}`}
          onClick={isConnected ? onDisconnect : onConnect}
          disabled={isConnecting}
        >
          {isConnected ? 'Disconnect' : isConnecting ? 'Connecting...' : 'Connect'}
        </button>
      </div>
    </header>
  )
}

export default Header
