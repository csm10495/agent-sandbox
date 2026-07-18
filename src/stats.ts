import type { AppData, Plate } from './types'

export interface RankedItem {
  label: string
  count: number
}

const rank = (labels: string[]): RankedItem[] => {
  const counts = new Map<string, number>()
  labels.forEach((label) => counts.set(label, (counts.get(label) ?? 0) + 1))
  return [...counts].map(([label, count]) => ({ label, count })).sort((a, b) => b.count - a.count || a.label.localeCompare(b.label))
}

export function getStats(data: AppData, seasonId = '', personId = '') {
  const trips = data.trips.filter((trip) => !seasonId || trip.seasonId === seasonId)
  const plates = trips.flatMap((trip) =>
    trip.plates.filter((plate) => !personId || plate.personId === personId),
  )
  const pasta = plates.filter((plate) => plate.kind === 'pasta')
  const personName = new Map(data.people.map((person) => [person.id, person.name]))
  const combinations = pasta.map(
    (plate) => `${plate.pastaName} + ${plate.sauceName}${plate.proteinName ? ` + ${plate.proteinName}` : ''}`,
  )
  const perPerson = rank(plates.map((plate) => personName.get(plate.personId) ?? 'Unknown'))
  const outingCounts = trips.flatMap((trip) =>
    trip.participantIds.map((id) => ({
      label: `${personName.get(id) ?? 'Unknown'} · ${trip.date}`,
      count: trip.plates.filter((plate) => plate.personId === id).length,
    })),
  ).sort((a, b) => b.count - a.count)

  return {
    trips: trips.length,
    plates: plates.length,
    pasta: pasta.length,
    soup: plates.filter((plate) => plate.kind === 'soup').length,
    salad: plates.filter((plate) => plate.kind === 'salad').length,
    perPerson,
    combinations: rank(combinations),
    outcomes: rank(plates.map((plate) => plate.outcome.replace('-', ' '))),
    largestOuting: outingCounts[0],
    average: trips.length ? plates.length / trips.length : 0,
  }
}

export function plateLabel(plate: Plate) {
  if (plate.kind !== 'pasta') return plate.dishName
  return `${plate.size === 'large' ? 'Large' : 'Small'} ${plate.pastaName} · ${plate.sauceName}${plate.proteinName ? ` · ${plate.proteinName}` : ''}`
}
