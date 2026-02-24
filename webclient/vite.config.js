import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import mkcert from 'vite-plugin-mkcert'

// https://vitejs.dev/config/
export default defineConfig(({ mode }) => ({
  plugins: [react(), mkcert()],
  // Use root path for Capacitor builds, GitHub Pages path for web
  base: process.env.CAPACITOR ? '/' : '/esp32c3zero-timer/',
  server: {
    https: true,
    port: 3000,
    host: '0.0.0.0' // Expose on all network interfaces
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    sourcemap: false
  },
  // Ensure PWA files are copied to dist
  publicDir: 'public'
}))
