import { useEffect, useState } from 'react';
import './App.css';
import BLEPeripheralDemo from './components/BLEPeripheralDemo.jsx';
import { Capacitor } from '@capacitor/core';

function AppPeripheralDemo() {
  const [isSupported, setIsSupported] = useState(true);

  useEffect(() => {
    // Check if running on Android
    const isAndroid = Capacitor.getPlatform() === 'android';
    
    if (!isAndroid) {
      setIsSupported(false);
      console.error('BLE Peripheral is only supported on Android');
    }
  }, []);

  if (!isSupported) {
    return (
      <div className="app compatibility-error">
        <div className="error-container">
          <h1>Platform Not Supported</h1>
          <p className="error-message">
            BLE Peripheral functionality is only available on Android devices.
          </p>
          <p className="error-details">
            Please install and run this app on an Android device (API 24+).
          </p>
          <p className="error-note">
            iOS and Web platforms do not support BLE peripheral mode.
          </p>
        </div>
      </div>
    );
  }

  return (
    <div className="app">
      <header className="app-header">
        <h1>BLE Peripheral Demo</h1>
        <p className="subtitle">Android BLE Server Test Application</p>
      </header>
      
      <main className="main-content peripheral-demo-main">
        <BLEPeripheralDemo />
      </main>
      
      <footer className="app-footer">
        <p>ESP32C3 Timer - BLE Peripheral Mode</p>
      </footer>
    </div>
  );
}

export default AppPeripheralDemo;
