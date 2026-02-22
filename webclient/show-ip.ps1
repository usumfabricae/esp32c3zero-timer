# Display network IP addresses for mobile device connection
Write-Host "`n=== Network IP Addresses ===" -ForegroundColor Cyan
Write-Host "Use one of these addresses to connect from your mobile device:`n" -ForegroundColor Green

# Get all IPv4 addresses
$ipAddresses = Get-NetIPAddress -AddressFamily IPv4 | 
    Where-Object { $_.IPAddress -notlike "127.*" -and $_.IPAddress -notlike "169.254.*" } |
    Select-Object IPAddress, InterfaceAlias

if ($ipAddresses) {
    foreach ($ip in $ipAddresses) {
        Write-Host "  https://$($ip.IPAddress):3000" -ForegroundColor Yellow
        Write-Host "    Interface: $($ip.InterfaceAlias)" -ForegroundColor Gray
        Write-Host ""
    }
} else {
    Write-Host "  No network IP addresses found!" -ForegroundColor Red
}

Write-Host "=== Instructions ===" -ForegroundColor Cyan
Write-Host "1. Make sure your mobile device is on the same network" -ForegroundColor White
Write-Host "2. Open one of the URLs above in your mobile browser" -ForegroundColor White
Write-Host "3. Accept the security warning (self-signed certificate)" -ForegroundColor White
Write-Host "4. The Web Bluetooth API should now work!" -ForegroundColor White
Write-Host ""
