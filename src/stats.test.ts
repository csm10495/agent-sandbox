import { describe, expect, it } from 'vitest'
import { getStats, plateLabel } from './stats'
import type { AppData, Plate } from './types'

const pasta = (id: string, personId: string, pastaName = 'Rigatoni'): Plate => ({
  id, personId, kind: 'pasta', size: 'small', pastaName, sauceName: 'Alfredo',
  proteinName: 'Meatballs', dishName: `${pastaName} + Alfredo`, outcome: 'eaten', createdAt: '2026-01-01',
})

const data: AppData = {
  schemaVersion: 1,
  people: [{ id: 'a', name: 'Alice', archived: false }, { id: 'b', name: 'Bob', archived: true }],
  seasons: [],
  trips: [
    { id: 't1', seasonId: 's1', date: '2026-01-01', participantIds: ['a', 'b'], plates: [pasta('1', 'a'), pasta('2', 'a'), pasta('3', 'b', 'Spaghetti')] },
    { id: 't2', seasonId: 's2', date: '2026-02-01', participantIds: ['a'], plates: [pasta('4', 'a')] },
  ],
}

describe('statistics', () => {
  it('calculates totals, records, common combinations, and archived people', () => {
    const stats = getStats(data)
    expect(stats.trips).toBe(2)
    expect(stats.plates).toBe(4)
    expect(stats.perPerson[0]).toEqual({ label: 'Alice', count: 3 })
    expect(stats.combinations[0].count).toBe(3)
    expect(stats.largestOuting).toEqual({ label: 'Alice · 2026-01-01', count: 2 })
  })

  it('filters by season and person', () => {
    expect(getStats(data, 's1', 'b').plates).toBe(1)
  })

  it('uses stable alphabetical ordering for ties', () => {
    expect(getStats(data, 's1').perPerson.map((item) => item.label)).toEqual(['Alice', 'Bob'])
  })

  it('formats a pasta plate', () => {
    expect(plateLabel(pasta('1', 'a'))).toBe('Small Rigatoni · Alfredo · Meatballs')
  })
})
