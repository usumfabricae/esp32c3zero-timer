import React from 'react'
import './Header.css'

function Header({ isConnected, isConnecting, connectionMethod, lastSyncTime, hasPendingCommands, onMenuClick, onConnect, onDisconnect }) {
  
  const getStatusText = () => {
    if (isConnecting) return 'Connecting...';
    if (!isConnected) return 'Disconnected';
    if (connectionMethod === 'iot') return 'Cloud Connected';
    return 'Local Connected';
  };

  const getStatusClass = () => {
    if (isConnecting) return 'connecting';
    if (!isConnected) return 'disconnected';
    if (connectionMethod === 'iot') return 'connected cloud';
    return 'connected local';
  };

  const formatRelativeTime = (date) => {
    if (!date) return null;
    const now = new Date();
    const diffMs = now - date;
    const diffSec = Math.floor(diffMs / 1000);
    if (diffSec < 60) return `${diffSec}s ago`;
    const diffMin = Math.floor(diffSec / 60);
    if (diffMin < 60) return `${diffMin} min ago`;
    const diffHr = Math.floor(diffMin / 60);
    return `${diffHr}h ago`;
  };

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
        <div className={`connection-status ${getStatusClass()}`}>
          <span className="status-indicator"></span>
          <span className="status-text">{getStatusText()}</span>
          {isConnected && connectionMethod === 'iot' && lastSyncTime && (
            <span className="sync-time" title={`Last sync: ${lastSyncTime.toLocaleString()}`}>
              {formatRelativeTime(lastSyncTime)}
            </span>
          )}
          {isConnected && hasPendingCommands && (
            <span className="pending-badge" title="Commands pending — device will apply on next sync">
              Pending
            </span>
          )}
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
