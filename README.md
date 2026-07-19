# Never Ending Pasta Tracker

A playful, private, mobile-first tracker for Never Ending Pasta Bowl outings. Configure local diners and promotion seasons, record pasta, soup, and salad plates, attach compressed photos, and compare local statistics. The app has no backend or accounts.

## Run locally

Requires Node.js 24 and npm.

```sh
npm install
npm run dev
```

Open the URL printed by Vite. To test the static production build:

```sh
npm run build
npm run preview
```

The production PWA works fully offline after its first successful load. Settings includes a manual update check and safe update/reload action.

## Data and backups

All records and compressed photos are stored in browser `localStorage`. Export JSON regularly from **Settings → Export JSON backup**. Import validates a backup before asking to replace all current data; a failed or cancelled import leaves current data untouched.

Browser storage is finite and browser/site-data clearing removes local records. Large photo collections can reach the browser quota.

## Validation

```sh
npm run lint
npm test
npm run build
npx playwright install chromium
npm run test:e2e
```

E2E tests exercise phone and desktop layouts at both `/` and `/agent-sandbox/`, including offline startup.

## GitHub Pages

The static build supports the repository subpath:

```sh
npm run build:pages
npm run preview:pages
```

The workflow in `.github/workflows/deploy-pages.yml` tests and deploys `dist` from `main`. In repository settings, select **GitHub Actions** as the Pages source. The configured base path, manifest scope, app start URL, assets, navigation fallback, and service worker all use `/agent-sandbox/`.

For another repository name, set `BASE_PATH=/your-repository/` when building and update the workflow command accordingly. The app uses state-based navigation rather than URL routes, so direct GitHub Pages loads do not require server rewrites.
