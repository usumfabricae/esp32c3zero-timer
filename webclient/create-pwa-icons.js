// Node.js script to create PWA icons without external dependencies
// Run with: node create-pwa-icons.js

const fs = require('fs');
const path = require('path');

// Create a simple SVG icon
const createSVG = (size) => `<?xml version="1.0" encoding="UTF-8"?>
<svg width="${size}" height="${size}" viewBox="0 0 ${size} ${size}" xmlns="http://www.w3.org/2000/svg">
  <!-- Background gradient -->
  <defs>
    <linearGradient id="grad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" style="stop-color:#646cff;stop-opacity:1" />
      <stop offset="100%" style="stop-color:#4a5fd8;stop-opacity:1" />
    </linearGradient>
  </defs>
  
  <!-- Background -->
  <rect width="${size}" height="${size}" fill="url(#grad)" rx="${size * 0.1}"/>
  
  <!-- Relay coil -->
  <circle cx="${size/2}" cy="${size/2}" r="${size * 0.15}" 
          fill="none" stroke="white" stroke-width="${size * 0.04}"/>
  
  <!-- Relay contacts -->
  <line x1="${size * 0.3}" y1="${size * 0.25}" 
        x2="${size * 0.6}" y2="${size * 0.15}" 
        stroke="white" stroke-width="${size * 0.04}" stroke-linecap="round"/>
  
  <!-- Contact terminals -->
  <circle cx="${size * 0.3}" cy="${size * 0.25}" r="${size * 0.03}" fill="white"/>
  <circle cx="${size * 0.7}" cy="${size * 0.25}" r="${size * 0.03}" fill="white"/>
  
  <!-- Temperature symbol -->
  <text x="${size/2}" y="${size * 0.78}" 
        font-family="Arial, sans-serif" 
        font-size="${size * 0.2}" 
        font-weight="bold" 
        fill="white" 
        text-anchor="middle">°C</text>
</svg>`;

// Create public directory if it doesn't exist
const publicDir = path.join(__dirname, 'public');
if (!fs.existsSync(publicDir)) {
  fs.mkdirSync(publicDir, { recursive: true });
}

// Create both icon sizes
[192, 512].forEach(size => {
  const svg = createSVG(size);
  const filename = path.join(publicDir, `icon-${size}.svg`);
  fs.writeFileSync(filename, svg);
  console.log(`✓ Created ${filename}`);
});

console.log('\nSVG icons created successfully!');
console.log('\nNote: For best compatibility, convert these to PNG:');
console.log('1. Open each SVG in a browser');
console.log('2. Take a screenshot or use browser dev tools to save as PNG');
console.log('3. Or use an online converter like https://cloudconvert.com/svg-to-png');
console.log('\nAlternatively, commit the SVG files and update manifest.json to use them.');
