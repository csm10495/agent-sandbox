export type Outcome = 'eaten' | 'taken-home' | 'left-behind'
export type PlateKind = 'pasta' | 'soup' | 'salad'
export type PastaSize = 'large' | 'small'

export interface Person {
  id: string
  name: string
  archived: boolean
}

export interface Dish {
  id: string
  name: string
  image?: string
  archived: boolean
}

export interface Season {
  id: string
  name: string
  startDate?: string
  endDate?: string
  notes?: string
  archived: boolean
  pastas: Dish[]
  sauces: Dish[]
  proteins: Dish[]
  soups: Dish[]
  salad: Dish
}

export interface Plate {
  id: string
  personId: string
  kind: PlateKind
  size?: PastaSize
  pastaId?: string
  pastaName?: string
  sauceId?: string
  sauceName?: string
  proteinId?: string
  proteinName?: string
  dishId?: string
  dishName: string
  outcome: Outcome
  photo?: string
  notes?: string
  createdAt: string
}

export interface Trip {
  id: string
  seasonId: string
  date: string
  location?: string
  notes?: string
  participantIds: string[]
  plates: Plate[]
}

export interface AppData {
  schemaVersion: 1
  people: Person[]
  seasons: Season[]
  trips: Trip[]
}

export const EMPTY_DATA: AppData = {
  schemaVersion: 1,
  people: [],
  seasons: [],
  trips: [],
}

export const id = () => crypto.randomUUID()
