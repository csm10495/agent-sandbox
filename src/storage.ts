import { EMPTY_DATA, type AppData } from './types'

export const STORAGE_KEY = 'never-ending-pasta-tracker'

const isArray = (value: unknown): value is unknown[] => Array.isArray(value)
const isRecord = (value: unknown): value is Record<string, unknown> => Boolean(value) && typeof value === 'object'
const hasString = (value: Record<string, unknown>, key: string) => typeof value[key] === 'string' && value[key] !== ''
const validImage = (value: unknown) =>
  value === undefined || (typeof value === 'string' && /^data:image\/(?:jpeg|png|webp);base64,[a-z0-9+/=\s]+$/i.test(value))

function validateDish(value: unknown) {
  if (!isRecord(value) || !hasString(value, 'id') || !hasString(value, 'name') || typeof value.archived !== 'boolean' || !validImage(value.image)) {
    throw new Error('The backup contains an invalid menu dish.')
  }
}

export function validateData(value: unknown): AppData {
  if (!value || typeof value !== 'object') throw new Error('The backup is not an object.')
  const data = value as Record<string, unknown>
  if (data.schemaVersion !== 1) {
    throw new Error(
      typeof data.schemaVersion === 'number' && data.schemaVersion > 1
        ? 'This backup was created by a newer app version.'
        : 'Unsupported or missing backup version.',
    )
  }
  if (!isArray(data.people) || !isArray(data.seasons) || !isArray(data.trips)) {
    throw new Error('The backup is missing required collections.')
  }
  const all = [...data.people, ...data.seasons, ...data.trips]
  if (all.some((item) => !isRecord(item) || !hasString(item, 'id'))) {
    throw new Error('The backup contains an item without a valid ID.')
  }
  const ids = all.map((item) => (item as Record<string, unknown>).id)
  if (new Set(ids).size !== ids.length) throw new Error('The backup contains duplicate IDs.')
  for (const person of data.people) {
    if (!isRecord(person) || !hasString(person, 'name') || typeof person.archived !== 'boolean' || !validImage(person.photo)) {
      throw new Error('The backup contains an invalid person.')
    }
  }
  const personIds = new Set(data.people.map((person) => (person as Record<string, unknown>).id))
  const seasonIds = new Set(data.seasons.map((season) => (season as Record<string, unknown>).id))
  for (const season of data.seasons) {
    if (!isRecord(season) || !hasString(season, 'name') || typeof season.archived !== 'boolean') {
      throw new Error('The backup contains an invalid season.')
    }
    for (const key of ['pastas', 'sauces', 'proteins', 'soups']) {
      if (!isArray(season[key])) throw new Error(`A season has invalid ${key}.`)
      season[key].forEach(validateDish)
    }
    validateDish(season.salad)
  }
  for (const trip of data.trips as Array<Record<string, unknown>>) {
    if (!hasString(trip, 'seasonId') || !seasonIds.has(trip.seasonId) || !hasString(trip, 'date') || !isArray(trip.plates) || !isArray(trip.participantIds)) {
      throw new Error('A trip has invalid plates or participants.')
    }
    if (trip.participantIds.some((personId) => typeof personId !== 'string' || !personIds.has(personId))) {
      throw new Error('A trip references an unknown person.')
    }
    for (const plate of trip.plates) {
      if (!isRecord(plate) || !hasString(plate, 'id') || !hasString(plate, 'personId') ||
        !trip.participantIds.includes(plate.personId) ||
        !['pasta', 'soup', 'salad'].includes(String(plate.kind)) ||
        !['eaten', 'taken-home', 'left-behind'].includes(String(plate.outcome)) ||
        !hasString(plate, 'dishName') || !hasString(plate, 'createdAt') || !validImage(plate.photo)) {
        throw new Error('A trip contains an invalid plate.')
      }
      if (plate.kind === 'pasta' &&
        (!['small', 'large'].includes(String(plate.size)) || !hasString(plate, 'pastaName') || !hasString(plate, 'sauceName'))) {
        throw new Error('A pasta plate is missing its size, pasta, or sauce.')
      }
    }
  }
  return structuredClone(value) as AppData
}

export function loadData(): AppData {
  const saved = localStorage.getItem(STORAGE_KEY)
  if (!saved) return structuredClone(EMPTY_DATA)
  try {
    return validateData(JSON.parse(saved))
  } catch {
    return structuredClone(EMPTY_DATA)
  }
}

export function saveData(data: AppData) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(data))
}

export function exportData(data: AppData) {
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' })
  const url = URL.createObjectURL(blob)
  const anchor = document.createElement('a')
  anchor.href = url
  anchor.download = `pasta-tracker-${new Date().toISOString().slice(0, 10)}.json`
  anchor.click()
  URL.revokeObjectURL(url)
}

export async function readImport(file: File): Promise<AppData> {
  let parsed: unknown
  try {
    parsed = JSON.parse(await file.text())
  } catch {
    throw new Error('That file is not valid JSON.')
  }
  return validateData(parsed)
}
