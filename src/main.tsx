import { StrictMode, useEffect, useState } from 'react'
import { createRoot } from 'react-dom/client'
import { registerSW } from 'virtual:pwa-register'
import App from './App'
import './index.css'

export function Root() {
  const [registration, setRegistration] = useState<ServiceWorkerRegistration>()
  const [updateReady, setUpdateReady] = useState(false)
  const [updateSW, setUpdateSW] = useState<((reloadPage?: boolean) => Promise<void>)>(() => async () => undefined)

  useEffect(() => {
    const updater = registerSW({
      immediate: true,
      onNeedRefresh: () => setUpdateReady(true),
      onRegisteredSW: (_url, nextRegistration) => setRegistration(nextRegistration),
    })
    setUpdateSW(() => updater)
  }, [])

  return <App updates={{
    registration,
    updateReady,
    applyUpdate: () => void updateSW(true),
  }} />
}

createRoot(document.getElementById('root')!).render(<StrictMode><Root /></StrictMode>)
