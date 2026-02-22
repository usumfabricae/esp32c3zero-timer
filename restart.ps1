# Reset ESP32-C3 by toggling DTR/RTS lines
$port = new-Object System.IO.Ports.SerialPort COM12,115200,None,8,one
$port.Open()
$port.DtrEnable = $false
$port.RtsEnable = $true
Start-Sleep -Milliseconds 100
$port.RtsEnable = $false
$port.Close()
Write-Host "Device reset command sent"
