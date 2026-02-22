import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import mkcert from 'vite-plugin-mkcert'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [
    react(),
    mkcert() // Automatically creates and uses self-signed certificates
  ],
  server: {
    https: true, // Enable HTTPS
    port: 3000,
    host: '0.0.0.0', // Allow access from network
    open: true
  },
  build: {
    outDir: 'dist',
    sourcemap: true
  }
})
