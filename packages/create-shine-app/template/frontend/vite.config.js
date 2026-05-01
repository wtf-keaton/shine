import {defineConfig} from 'vite'
import react from '@vitejs/plugin-react'

const host = "localhost"

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],

  server: {
    port: 1745,
    strictPort: true,
    host: host || false,
    hmr: host
        ? {
          protocol: "ws",
          host,
          port: 1746,
        }
        : undefined,
    watch: {
      ignored: [
        "../src/**",
        "../shine/**",
        "../generated/**",
        "../cmake-build-**/**",
        "../build/**"
      ],
    },
  },
})
