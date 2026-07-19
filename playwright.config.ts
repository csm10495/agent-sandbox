import { defineConfig, devices } from '@playwright/test'

export default defineConfig({
  testDir: './e2e',
  fullyParallel: false,
  retries: process.env.CI ? 2 : 0,
  reporter: process.env.CI ? 'github' : 'list',
  use: { trace: 'on-first-retry' },
  projects: [
    {
      name: 'mobile-root',
      use: { ...devices['Pixel 7'], baseURL: 'http://127.0.0.1:4173/' },
    },
    {
      name: 'desktop-subpath',
      use: { ...devices['Desktop Chrome'], baseURL: 'http://127.0.0.1:4174/agent-sandbox/' },
    },
  ],
  webServer: [
    {
      command: 'npm run build && npm run preview -- --host 127.0.0.1 --port 4173',
      port: 4173,
      reuseExistingServer: !process.env.CI,
    },
    {
      command: 'BASE_PATH=/agent-sandbox/ npx vite build --outDir dist-pages && BASE_PATH=/agent-sandbox/ npx vite preview --outDir dist-pages --host 127.0.0.1 --port 4174',
      port: 4174,
      reuseExistingServer: !process.env.CI,
    },
  ],
})
