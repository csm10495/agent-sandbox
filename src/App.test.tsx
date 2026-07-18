import { fireEvent, render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { describe, expect, it, vi } from 'vitest'
import App from './App'
import { EMPTY_DATA } from './types'

describe('app', () => {
  it('creates a tracked person and persists it', async () => {
    const user = userEvent.setup()
    render(<App />)
    await user.click(screen.getByRole('button', { name: /setup/i }))
    await user.type(screen.getByLabelText('Person name'), 'Alice')
    await user.click(screen.getByRole('button', { name: 'Add' }))
    expect(screen.getByText('Alice')).toBeInTheDocument()
    expect(localStorage.getItem('never-ending-pasta-tracker')).toContain('Alice')
  })

  it('reports the offline update-check state', async () => {
    const user = userEvent.setup()
    vi.spyOn(window.navigator, 'onLine', 'get').mockReturnValue(false)
    render(<App />)
    await user.click(screen.getByRole('button', { name: /settings/i }))
    await user.click(screen.getByRole('button', { name: 'Check for update' }))
    expect(screen.getByRole('button', { name: 'Go online to check' })).toBeInTheDocument()
  })

  it('activates an available update', async () => {
    const applyUpdate = vi.fn()
    const user = userEvent.setup()
    render(<App updates={{ updateReady: true, applyUpdate }} />)
    await user.click(screen.getByRole('button', { name: /settings/i }))
    await user.click(screen.getByRole('button', { name: 'Install update & reload' }))
    expect(applyUpdate).toHaveBeenCalledOnce()
  })

  it('keeps data when a replacement import is cancelled', async () => {
    localStorage.setItem('never-ending-pasta-tracker', JSON.stringify({
      ...EMPTY_DATA, people: [{ id: 'p1', name: 'Alice', archived: false }],
    }))
    vi.spyOn(window, 'confirm').mockReturnValue(false)
    render(<App />)
    fireEvent.click(screen.getByRole('button', { name: /settings/i }))
    const input = document.querySelector('input[type=file]') as HTMLInputElement
    const file = new File([JSON.stringify(EMPTY_DATA)], 'empty.json', { type: 'application/json' })
    fireEvent.change(input, { target: { files: [file] } })
    await screen.findByText('Backup & restore')
    expect(localStorage.getItem('never-ending-pasta-tracker')).toContain('Alice')
  })
})
