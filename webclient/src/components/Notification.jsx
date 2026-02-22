import React, { useEffect, useState } from 'react'
import './Notification.css'

function Notification({ type = 'info', message, duration = null, onClose }) {
  const [isVisible, setIsVisible] = useState(true);

  useEffect(() => {
    // Auto-dismiss success notifications after 3 seconds (or custom duration)
    if (type === 'success' && duration !== null) {
      const timer = setTimeout(() => {
        setIsVisible(false);
        if (onClose) {
          setTimeout(onClose, 300); // Wait for fade-out animation
        }
      }, duration || 3000);

      return () => clearTimeout(timer);
    }
  }, [type, duration, onClose]);

  if (!isVisible) {
    return null;
  }

  const getIcon = () => {
    switch (type) {
      case 'success':
        return '✓';
      case 'error':
        return '✕';
      case 'loading':
        return '⟳';
      case 'info':
      default:
        return 'ℹ';
    }
  };

  return (
    <div className={`notification notification-${type} ${isVisible ? 'visible' : ''}`}>
      <span className="notification-icon">{getIcon()}</span>
      <span className="notification-message">{message}</span>
      {type !== 'loading' && onClose && (
        <button 
          className="notification-close" 
          onClick={() => {
            setIsVisible(false);
            setTimeout(onClose, 300);
          }}
          aria-label="Close notification"
        >
          ×
        </button>
      )}
    </div>
  )
}

export default Notification
