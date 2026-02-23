# ESP32-C3 Timer - Web Interface

A React-based single-page web application for connecting to and controlling the ESP32-C3 temperature-controlled relay timer via Bluetooth Low Energy (BLE) using the Web Bluetooth API.

## Features

- **Real-time Monitoring:**
  - Temperature readings with visual display
  - Battery level with gauge and status indicator
  - Relay state with interactive gauge
  - Current time display
  
- **Device Control:**
  - Manual relay control with configurable duration (15 min to 4 hours)
  - Override end time display when manual control is active
  - Weekly schedule programming (7 days × 24 hours)
  - Temperature threshold configuration (High/Low modes)
  - Temperature sensor calibration
  
- **User Experience:**
  - Automatic reconnection with exponential backoff
  - Real-time notifications for all operations
  - Responsive design for mobile and desktop
  - Connection status indicators
  - Data reload functionality

## Browser Requirements

This application requires a browser with Web Bluetooth API support:
- **Chrome** 56+ (recommended)
- **Edge** 79+
- **Opera** 43+

**Important:** 
- Web Bluetooth requires **HTTPS** in production
- Localhost is allowed for development
- Not supported in Firefox or Safari

## Quick Start

### Prerequisites

- Node.js v14 or higher
- npm or yarn package manager

### Installation

```bash
cd webclient
npm install
```

### Development Server

```bash
npm run dev
```

The application will open at `https://localhost:3000` (HTTPS required for Web Bluetooth)

### Building for Production

```bash
npm run build
```

The production build will be created in the `dist/` directory.

### Preview Production Build

```bash
npm run preview
```

## Project Structure

```
webclient/
├── src/
│   ├── components/           # React UI components
│   │   ├── Dashboard.jsx     # Main dashboard view
│   │   ├── Header.jsx        # App header with connect button
│   │   ├── Sidebar.jsx       # Navigation sidebar
│   │   ├── TemperatureDisplay.jsx  # Temperature gauge
│   │   ├── RelayGauge.jsx    # Relay control gauge
│   │   ├── BatteryGauge.jsx  # Battery level gauge
│   │   ├── ScheduleGrid.jsx  # Weekly schedule editor
│   │   ├── TimerProgramming.jsx    # Schedule programming view
│   │   ├── TemperatureSettings.jsx # Threshold configuration
│   │   └── Notification.jsx  # Toast notifications
│   ├── hooks/
│   │   └── useBLE.js         # Web Bluetooth API hook
│   ├── utils/
│   │   └── dataFormatter.js  # BLE data conversion utilities
│   ├── styles/
│   │   └── global.css        # Global styles
│   ├── App.jsx               # Main app component
│   └── main.jsx              # React entry point
├── public/                   # Static assets
├── index.html                # HTML template
├── vite.config.js            # Vite configuration
├── package.json              # Dependencies and scripts
└── README.md                 # This file
```

## Connecting to ESP32 Device

### Connection Process

1. **Power On Device:** Ensure your ESP32 is powered and within Bluetooth range
2. **Click Connect:** Press the "Connect" button in the web interface
3. **Select Device:** Choose "ESP32C3_Timer" from the browser's device picker
4. **Wait for Data:** Initial data will be read automatically after connection

### Connection Notes

- Device advertises for 10 seconds after boot/wake (configurable in firmware)
- Device wakes every 30-60 seconds from deep sleep (configurable)
- If connection fails, wait for next wake cycle and try again
- Automatic reconnection attempts up to 3 times with exponential backoff
- Connection status shown in header and notifications

### Troubleshooting Connection

**"No device found"**
- Device may be sleeping - wait 30-60 seconds and try again
- Ensure device is powered on
- Check Bluetooth is enabled on your computer

**"Connection failed"**
- Device may have gone to sleep - try again
- Refresh the page and reconnect
- Check browser console for detailed error messages

**"Permission denied"**
- Grant Bluetooth permissions in browser settings
- On some systems, you may need to pair the device first

## Using the Interface

### Dashboard View

The main dashboard displays:
- **Temperature Display:** Current temperature with visual gauge
- **Relay Gauge:** Current relay state with manual control toggle
  - Duration selector (15 min, 30 min, 1 hour, 2 hours, 3 hours, 4 hours)
  - Override end time display when manual control is active
  - Toggle switch for ON/OFF control
- **Battery Gauge:** Battery level percentage and voltage
- **Temperature Thresholds:** Current High/Low temperature settings
- **Today's Schedule:** Visual grid showing today's hourly schedule
- **Reload Button:** Refresh all data from device

### Timer Programming View

Program weekly schedules:
- **7-Day Grid:** Visual editor for all days and hours
- **Three Modes:**
  - H (High): Relay ON if temp < High threshold
  - L (Low): Relay ON if temp < Low threshold
  - O (Off): No automatic control
- **Click to Edit:** Click any cell to cycle through modes
- **Save:** Changes are saved immediately to device

### Temperature Settings View

Configure temperature control:
- **High Threshold:** Temperature for High mode operation
- **Low Threshold:** Temperature for Low mode operation
- **Calibration:** Add calibration points for sensor accuracy
- **Reset Calibration:** Clear all calibration data

## BLE Characteristics Used

The web interface communicates with these BLE characteristics:

### Standard Characteristics (Bluetooth SIG)
- **0x2A6E** - Temperature (Environmental Sensing)
- **0x2A2B** - Current Time
- **0x2A19** - Battery Level (Battery Service)
- **0x2A29** - Manufacturer Name
- **0x2A24** - Model Number
- **0x2A26** - Firmware Revision

### Custom Characteristics
- **0xFF02** - Relay Control (Read/Write)
  - Write format: ASCII string "x,y" where x=state (0/1), y=duration in 15-min units
  - Read format: ASCII string "x,hh:mm" (with override) or "x" (no override)
  - Example write: "1,4" = Turn ON for 60 minutes
  - Example read: "1,14:30" = ON, override until 14:30
- **0xFF05** - Schedule Configuration (Read/Write)
- **0xFF06** - Temperature Thresholds (Read/Write)
- **0xFF07** - Temperature Calibration (Write)

For detailed BLE protocol documentation, see main project README.

## Data Format Utilities

The `dataFormatter.js` utility handles all BLE data conversions:

- **Temperature:** 16-bit signed integer (0.01°C resolution)
- **Time:** 10-byte Bluetooth SIG Current Time format
- **Battery:** 8-bit unsigned integer (percentage)
- **Relay State:** ASCII string format
  - Write: "x,y" where x=state (0/1), y=duration in 15-min units
  - Read: "x,hh:mm" (with override) or "x" (no override)
- **Schedule:** 25-byte format (day + 24 characters)
- **Thresholds:** 4-byte format (2x 16-bit temperatures)

## Notifications

The interface provides real-time feedback:
- **Success:** Green notifications for successful operations
- **Error:** Red notifications for failures
- **Info:** Blue notifications for status updates
- **Loading:** Gray notifications for ongoing operations

Notifications auto-dismiss after 3-5 seconds (configurable).

## Responsive Design

The interface adapts to different screen sizes:
- **Desktop:** Full dashboard with sidebar navigation
- **Tablet:** Responsive grid layout
- **Mobile:** Stacked layout with hamburger menu

## Development Tips

### Hot Module Replacement

Vite provides instant hot module replacement during development. Changes to components will update immediately without full page reload.

### Browser Console

Enable browser console to see:
- BLE connection logs
- Characteristic read/write operations
- Error messages and stack traces
- Performance metrics

### Testing

Test the interface with:
```bash
npm run dev
```

Then connect to your ESP32 device and verify:
1. Connection establishes successfully
2. All data reads correctly
3. Write operations work (relay, schedule, thresholds)
4. Notifications appear for all operations
5. Reconnection works after device sleep

## Production Deployment

### HTTPS Requirement

Web Bluetooth API requires HTTPS. For production:

1. **Generate SSL Certificate:**
   ```bash
   npm run generate-cert
   ```

2. **Configure Web Server:**
   - Use nginx, Apache, or similar
   - Enable HTTPS with valid certificate
   - Serve files from `dist/` directory

3. **Build and Deploy:**
   ```bash
   npm run build
   # Copy dist/ contents to web server
   ```

### Example nginx Configuration

```nginx
server {
    listen 443 ssl http2;
    server_name timer.example.com;
    
    ssl_certificate /path/to/cert.pem;
    ssl_certificate_key /path/to/key.pem;
    
    root /var/www/timer/dist;
    index index.html;
    
    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

## Known Limitations

- **Browser Support:** Limited to Chromium-based browsers
- **Connection Range:** Bluetooth range typically 10-30 meters
- **Device Sleep:** Device sleeps between wake intervals for power saving
- **Single Connection:** Device supports one BLE connection at a time
- **No Offline Mode:** Requires active BLE connection to device

## Future Enhancements

- [ ] Historical data charts
- [ ] Multiple device support
- [ ] Export/import schedules
- [ ] Dark mode theme
- [ ] PWA support for mobile installation
- [ ] Notification sound options

## Troubleshooting

**Interface won't load:**
- Check Node.js and npm are installed
- Run `npm install` to install dependencies
- Check for port conflicts (default: 3000)

**Can't connect to device:**
- Verify device is powered and in range
- Check browser supports Web Bluetooth
- Ensure HTTPS is enabled (or using localhost)
- Try refreshing the page

**Data not updating:**
- Check BLE connection status in header
- Try clicking "Reload Data" button
- Reconnect to device
- Check browser console for errors

**Schedule not saving:**
- Verify BLE connection is active
- Check for error notifications
- Try reconnecting and saving again

## Support

For issues and questions:
- Check main project README for firmware documentation
- Review browser console for detailed error messages
- Ensure firmware and web interface versions are compatible

## License

See main project LICENSE file.

 
 