import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// The bundle is folded into the server jar under /web and served from the same
// origin as the API, so paths stay relative.  In development vite serves the UI
// and proxies the API and the websocket through to the Kotlin server.
export default defineConfig({
  plugins: [react()],
  base: './',
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    sourcemap: false,
  },
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:7070',
        changeOrigin: true,
        ws: true,
      },
    },
  },
})
