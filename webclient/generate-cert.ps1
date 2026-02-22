# Generate self-signed certificate for HTTPS development
Write-Host "Generating self-signed certificate for HTTPS development..." -ForegroundColor Green

# Create certificate
$cert = New-SelfSignedCertificate `
    -Subject "localhost" `
    -DnsName "localhost", "127.0.0.1", "*.local" `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -NotBefore (Get-Date) `
    -NotAfter (Get-Date).AddYears(5) `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -FriendlyName "Vite Dev Server HTTPS" `
    -HashAlgorithm SHA256 `
    -KeyUsage DigitalSignature, KeyEncipherment, DataEncipherment `
    -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.1")

$thumbprint = $cert.Thumbprint
Write-Host "Certificate created with thumbprint: $thumbprint" -ForegroundColor Cyan

# Create certs directory if it doesn't exist
$certsDir = Join-Path $PSScriptRoot "certs"
if (-not (Test-Path $certsDir)) {
    New-Item -ItemType Directory -Path $certsDir | Out-Null
}

# Export certificate to PEM format (for Vite)
$certPath = Join-Path $certsDir "localhost.crt"
$keyPath = Join-Path $certsDir "localhost.key"

# Export certificate
$certBytes = $cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert)
$certPem = "-----BEGIN CERTIFICATE-----`n"
$certPem += [System.Convert]::ToBase64String($certBytes, [System.Base64FormattingOptions]::InsertLineBreaks)
$certPem += "`n-----END CERTIFICATE-----"
$certPem | Out-File -FilePath $certPath -Encoding ASCII

Write-Host "Certificate exported to: $certPath" -ForegroundColor Green

# Note about private key
Write-Host "`nIMPORTANT: Windows doesn't allow direct export of private keys from the certificate store." -ForegroundColor Yellow
Write-Host "We'll use the certificate thumbprint directly in Node.js instead." -ForegroundColor Yellow
Write-Host "`nCertificate Thumbprint: $thumbprint" -ForegroundColor Cyan

# Save thumbprint to a file for easy reference
$thumbprint | Out-File -FilePath (Join-Path $certsDir "thumbprint.txt") -Encoding ASCII

Write-Host "`nTo trust this certificate on your mobile device:" -ForegroundColor Green
Write-Host "1. Copy the file: $certPath" -ForegroundColor White
Write-Host "2. Transfer it to your mobile device" -ForegroundColor White
Write-Host "3. Install it as a trusted certificate" -ForegroundColor White
Write-Host "   - Android: Settings > Security > Install from storage" -ForegroundColor White
Write-Host "   - iOS: Open the file, then Settings > General > Profile > Install" -ForegroundColor White

Write-Host "`nDone! The certificate is ready to use." -ForegroundColor Green
