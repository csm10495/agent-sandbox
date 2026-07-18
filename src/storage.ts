import { EMPTY_DATA, type AppData } from './types'

export const STORAGE_KEY = 'never-ending-pasta-tracker'

const isArray = (value: unknown): value is unknown[] => Array.isArray(value)

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
  if (all.some((item) => !item || typeof item !== 'object' || typeof (item as Record<string, unknown>).id !== 'string')) {
    throw new Error('The backup contains an item without a valid ID.')
  }
  for (const trip of data.trips as Array<Record<string, unknown>>) {
    if (!isArray(trip.plates) || !isArray(trip.participantIds)) {
      throw new Error('A trip has invalid plates or participants.')
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
