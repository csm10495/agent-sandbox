import { defineConfig } from 'vitest/config'
import react from '@vitejs/plugin-react'
import { VitePWA } from 'vite-plugin-pwa'

const base = process.env.BASE_PATH || '/'

export default defineConfig({
  base,
  plugins: [
    react(),
    VitePWA({
      registerType: 'prompt',
      injectRegister: false,
      includeAssets: ['pasta-icon.svg', 'pasta-icon.ico'],
      manifest: {
        name: 'Never Ending Pasta Tracker',
        short_name: 'Pasta Tracker',
        description: 'Track every plate from Never Ending Pasta Bowl outings.',
        theme_color: '#8f2d23',
        background_color: '#fff8e8',
        display: 'standalone',
        start_url: base,
        scope: base,
        icons: [
          {
            src: `${base}pasta-icon.svg`,
            sizes: 'any',
            type: 'image/svg+xml',
            purpose: 'any maskable',
          },
        ],
      },
      workbox: {
        navigateFallback: `${base}index.html`,
        globPatterns: ['**/*.{js,css,html,svg}'],
        cleanupOutdatedCaches: true,
      },
      devOptions: { enabled: true },
    }),
  ],
  test: {
    environment: 'jsdom',
    setupFiles: './src/test/setup.ts',
    css: true,
    exclude: ['e2e/**', '**/node_modules/**', '**/dist/**'],
  },
})
