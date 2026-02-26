import { useEffect, useState } from 'react'
import './App.css'
import { useBLEUnified } from './hooks/useBLEUnified.js'
import { Capacitor } from '@capacitor/core'
import Header from './components/Header.jsx'
import Sidebar from './components/Sidebar.jsx'
import Dashboard from './components/Dashboard.jsx'
import TimerProgramming from './components/TimerProgramming.jsx'
import TemperatureSettings from './components/TemperatureSettings.jsx'
import Settings from './components/Settings.jsx'
import Notification from './components/Notification.jsx'
import PWAInstallPrompt from './components/PWAInstallPrompt.jsx'

function App() {
  // Initialize unified BLE hook (auto-detects web vs native)
  const ble = useBLEUnified();
  
  // Manage current view state
  const [currentView, setCurrentView] = useState('dashboard');
  
  // Manage sidebar open/close state
  const [sidebarOpen, setSidebarOpen] = useState(false);
  
  // Browser compatibility state
  const [isSupported, setIsSupported] = useState(true);
  
  // Global notification state (for errors and connection status)
  const [globalNotification, setGlobalNotification] = useState(null);
  
  // Disconnection state
  const [lastDisconnectTime, setLastDisconnectTime] = useState(null);
  const [estimatedWakeTime, setEstimatedWakeTime] = useState(null);
  
  // Track if we need to read initial data after connection
  const [needsInitialRead, setNeedsInitialRead] = useState(false);

  // Check for BLE support on mount (Web Bluetooth API or Capacitor)
  useEffect(() => {
    const isNative = Capacitor.isNativePlatform();
    if (!isNative && !navigator.bluetooth) {
      setIsSupported(false);
      console.error('Web Bluetooth API is not supported in this browser');
    }
  }, []);
  
  // Read initial data when characteristics become available after connection
  useEffect(() => {
    const readInitialData = async () => {
      if (!needsInitialRead || !ble.isConnected) {
        return;
      }
      
      // Clear the flag immediately to prevent multiple executions
      setNeedsInitialRead(false);
      
      console.log('[App] Reading initial data after connection...');
      console.log('[App] Characteristics available:', Object.keys(ble.characteristics).length);
      
      setGlobalNotification({
        type: 'loading',
        message: 'Reading initial data...'
      });

      // Small delay to ensure characteristics are fully ready
      await new Promise(resolve => setTimeout(resolve, 100));

      // Read initial data - handle each independently
      const readOperations = [
        { name: 'temperature', fn: () => ble.readTemperature() },
        { name: 'current time', fn: () => ble.readCurrentTime() },
        { name: 'relay state', fn: () => ble.readRelayState() },
        { name: 'schedule', fn: () => ble.readSchedule() },
        { name: 'temperature thresholds', fn: () => ble.readTemperatureThresholds() },
        { name: 'battery level', fn: () => ble.readBatteryLevel() },
        { name: 'WiFi SSID', fn: () => ble.readWifiSsid() }
      ];

      for (const operation of readOperations) {
        try {
          await operation.fn();
          console.log(`[App] Successfully read ${operation.name}`);
        } catch (readError) {
          console.warn(`[App] Failed to read ${operation.name}:`, readError);
          // Continue with other reads even if one fails
        }
      }

      // Enable notifications for temperature and time
      try {
        await ble.setupNotifications();
      } catch (notifError) {
        console.warn('[App] Failed to set up notifications:', notifError);
        // Continue anyway - notifications are optional
      }

      // Show connected status
      setGlobalNotification({
        type: 'success',
        message: 'Connected successfully!',
        duration: 3000
      });
    };

    readInitialData();
  }, [needsInitialRead, ble.isConnected]);
  
  // Handle disconnection - show status and estimated wake time
  useEffect(() => {
    if (!ble.isConnected && !ble.isConnecting && lastDisconnectTime === null && ble.device) {
      // Device was connected but now disconnected
      const now = new Date();
      setLastDisconnectTime(now);
      
      // Calculate estimated wake time (60-second cycle)
      const wakeTime = new Date(now.getTime() + 60000);
      setEstimatedWakeTime(wakeTime);
      
      setGlobalNotification({
        type: 'info',
        message: `Device disconnected (sleeping). Estimated wake time: ${wakeTime.toLocaleTimeString()}`
      });
    } else if (ble.isConnected && lastDisconnectTime !== null) {
      // Reconnected
      setLastDisconnectTime(null);
      setEstimatedWakeTime(null);
    }
  }, [ble.isConnected, ble.isConnecting, ble.device, lastDisconnectTime]);
  
  // Show reconnection prompt after 3 failed attempts
  useEffect(() => {
    if (ble.reconnectAttempts >= 3 && !ble.isConnected) {
      setGlobalNotification({
        type: 'error',
        message: 'Failed to reconnect after 3 attempts. Please click the Connect button to try again.'
      });
    }
  }, [ble.reconnectAttempts, ble.isConnected]);

  // Handle connection button click
  const handleConnect = async () => {
    setGlobalNotification({
      type: 'loading',
      message: 'Scanning for device... (up to 70 seconds - device wakes every 60s)'
    });

    try {
      // Call ble.connect
      await ble.connect();
      
      // Set flag to trigger initial data read via useEffect
      setNeedsInitialRead(true);
    } catch (error) {
      console.error('Connection failed:', error);
      
      // Handle permission denied errors
      if (error.name === 'NotAllowedError' || error.name === 'SecurityError') {
        setGlobalNotification({
          type: 'error',
          message: 'Bluetooth permission denied. Please enable Bluetooth permissions in your browser settings and try again.'
        });
      } else if (error.name === 'NotFoundError') {
        setGlobalNotification({
          type: 'error',
          message: 'No device selected. The device wakes every 60 seconds. Please try again.'
        });
      } else {
        setGlobalNotification({
          type: 'error',
          message: `Connection failed: ${error.message}`
        });
      }
    }
  };

  // Handle disconnection button click
  const handleDisconnect = async () => {
    setGlobalNotification({
      type: 'info',
      message: 'Disconnecting...',
      duration: 2000
    });

    try {
      await ble.disconnect();
      setGlobalNotification({
        type: 'success',
        message: 'Disconnected successfully',
        duration: 3000
      });
    } catch (error) {
      console.error('Disconnection failed:', error);
      setGlobalNotification({
        type: 'error',
        message: `Disconnection failed: ${error.message}`
      });
    }
  };

  // Handle navigation
  const handleNavigate = (view) => {
    setCurrentView(view);
    setSidebarOpen(false);
  };

  // Handle menu toggle
  const handleMenuToggle = () => {
    setSidebarOpen(!sidebarOpen);
  };

  // Close global notification
  const closeGlobalNotification = () => {
    setGlobalNotification(null);
  };

  // Show browser compatibility error if not supported
  if (!isSupported) {
    return (
      <div className="app compatibility-error">
        <div className="error-container">
          <h1>Browser Not Supported</h1>
          <p className="error-message">
            Web Bluetooth API is not supported in this browser.
          </p>
          <p className="error-details">
            Please use Chrome, Edge, or Opera on desktop, or Chrome on Android.
          </p>
          <p className="error-note">
            Note: Web Bluetooth requires HTTPS (or localhost for development).
          </p>
        </div>
      </div>
    );
  }

  return (
    <div className="app">
      <Header 
        isConnected={ble.isConnected}
        isConnecting={ble.isConnecting}
        onMenuClick={handleMenuToggle}
        onConnect={handleConnect}
        onDisconnect={handleDisconnect}
      />
      
      <Sidebar 
        isOpen={sidebarOpen}
        currentView={currentView}
        onNavigate={handleNavigate}
      />
      
      <main className="main-content">
        {currentView === 'dashboard' && <Dashboard ble={ble} />}
        {currentView === 'timer' && <TimerProgramming ble={ble} />}
        {currentView === 'temperature' && <TemperatureSettings ble={ble} />}
        {currentView === 'config' && (
          <Settings 
            isConnected={ble.isConnected}
            wifiSsid={ble.deviceData.wifiSsid}
            onUpdateWifiSsid={ble.writeWifiSsid}
            onUpdateWifiPassword={ble.writeWifiPassword}
            onUpdateBlePasskey={ble.writeBlePasskey}
          />
        )}
      </main>
      
      {globalNotification && (
        <Notification
          type={globalNotification.type}
          message={globalNotification.message}
          duration={globalNotification.duration}
          onClose={closeGlobalNotification}
        />
      )}
      
      {ble.error && !globalNotification && (
        <Notification
          type="error"
          message={ble.error}
          onClose={() => {}}
        />
      )}
      
      <PWAInstallPrompt />
    </div>
  )
}

export default App
