import { describe, expect, it } from 'vitest'
import { loadData, readImport, saveData, STORAGE_KEY, validateData } from './storage'
import { EMPTY_DATA } from './types'

describe('local data', () => {
  it('round trips versioned data', () => {
    const data = { ...EMPTY_DATA, people: [{ id: 'p1', name: 'Alice', archived: false }] }
    saveData(data)
    expect(loadData()).toEqual(data)
  })

  it('falls back safely when stored data is corrupt', () => {
    localStorage.setItem(STORAGE_KEY, '{bad')
    expect(loadData()).toEqual(EMPTY_DATA)
  })

  it('rejects future schema versions', () => {
    expect(() => validateData({ ...EMPTY_DATA, schemaVersion: 2 })).toThrow('newer')
  })

  it('rejects malformed imports without changing local data', async () => {
    const before = { ...EMPTY_DATA, people: [{ id: 'p1', name: 'Alice', archived: false }] }
    saveData(before)
    await expect(readImport(new File(['nope'], 'bad.json'))).rejects.toThrow('valid JSON')
    expect(loadData()).toEqual(before)
  })

  it('accepts image data inside a replacement backup', async () => {
    const backup = {
      ...EMPTY_DATA,
      people: [{ id: 'p1', name: 'Alice', archived: false }],
      seasons: [{
        id: 's1', name: '2026', archived: false, pastas: [{ id: 'd1', name: 'Rigatoni', archived: false, image: 'data:image/jpeg;base64,YQ==' }],
        sauces: [], proteins: [], soups: [], salad: { id: 'salad', name: 'Salad', archived: false },
      }],
    }
    await expect(readImport(new File([JSON.stringify(backup)], 'backup.json'))).resolves.toEqual(backup)
  })

  it('rejects dangling references and unsafe image formats', () => {
    const dangling = {
      ...EMPTY_DATA,
      trips: [{ id: 't1', seasonId: 'missing', date: '2026-01-01', participantIds: [], plates: [] }],
    }
    expect(() => validateData(dangling)).toThrow('invalid plates or participants')
    const unsafe = {
      ...EMPTY_DATA,
      seasons: [{
        id: 's1', name: '2026', archived: false,
        pastas: [{ id: 'd1', name: 'Rigatoni', archived: false, image: 'data:image/svg+xml,<svg />' }],
        sauces: [], proteins: [], soups: [], salad: { id: 'salad', name: 'Salad', archived: false },
      }],
    }
    expect(() => validateData(unsafe)).toThrow('invalid menu dish')
  })
})
