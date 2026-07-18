import { useEffect, useMemo, useRef, useState, type FormEvent, type ReactNode } from 'react'
import { compressImage } from './image'
import { exportData, loadData, readImport, saveData, STORAGE_KEY } from './storage'
import { getStats, plateLabel } from './stats'
import {
  id,
  type AppData,
  type Dish,
  type Outcome,
  type PastaSize,
  type PlateKind,
  type Season,
  type Trip,
} from './types'
import './App.css'

type Tab = 'dashboard' | 'trips' | 'setup' | 'settings'
type UpdateState = 'idle' | 'checking' | 'current' | 'available' | 'offline' | 'error'

interface UpdateControls {
  registration?: ServiceWorkerRegistration
  updateReady: boolean
  applyUpdate: () => void
}

const today = () => new Date().toISOString().slice(0, 10)
const active = (items: Dish[]) => items.filter((item) => !item.archived)
const dish = (name: string): Dish => ({ id: id(), name, archived: false })

function Field({ label, children }: { label: string; children: ReactNode }) {
  return <label className="field"><span>{label}</span>{children}</label>
}

function Empty({ children }: { children: ReactNode }) {
  return <div className="empty"><span aria-hidden="true">🍝</span><p>{children}</p></div>
}

function ImageInput({ value, onChange, label = 'Photo' }: {
  value?: string
  onChange: (value?: string) => void
  label?: string
}) {
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState('')
  return <div className="image-input">
    {value && <img src={value} alt={`${label} preview`} />}
    <label className="button secondary compact">
      {busy ? 'Compressing…' : value ? `Replace ${label}` : `Add ${label}`}
      <input
        type="file"
        accept="image/*"
        capture="environment"
        disabled={busy}
        onChange={async (event) => {
          const file = event.target.files?.[0]
          if (!file) return
          setBusy(true)
          setError('')
          try {
            onChange(await compressImage(file))
          } catch (caught) {
            setError(caught instanceof Error ? caught.message : 'Could not process image.')
          } finally {
            setBusy(false)
            event.target.value = ''
          }
        }}
      />
    </label>
    {value && <button className="link danger" type="button" onClick={() => onChange()}>Remove</button>}
    {error && <small className="error">{error}</small>}
  </div>
}

function Dashboard({ data }: { data: AppData }) {
  const [seasonId, setSeasonId] = useState('')
  const [personId, setPersonId] = useState('')
  const stats = useMemo(() => getStats(data, seasonId, personId), [data, personId, seasonId])
  return <section className="page">
    <header className="page-head"><div><p className="eyebrow">The saucy scoreboard</p><h2>Dashboard</h2></div></header>
    <div className="filters">
      <Field label="Season"><select value={seasonId} onChange={(e) => setSeasonId(e.target.value)}>
        <option value="">All seasons</option>{data.seasons.map((season) => <option key={season.id} value={season.id}>{season.name}</option>)}
      </select></Field>
      <Field label="Person"><select value={personId} onChange={(e) => setPersonId(e.target.value)}>
        <option value="">Everyone</option>{data.people.map((person) => <option key={person.id} value={person.id}>{person.name}</option>)}
      </select></Field>
    </div>
    <div className="stat-grid">
      <article><strong>{stats.trips}</strong><span>Outings</span></article>
      <article><strong>{stats.plates}</strong><span>Total plates</span></article>
      <article><strong>{stats.pasta}</strong><span>Pasta</span></article>
      <article><strong>{stats.soup + stats.salad}</strong><span>Soup & salad</span></article>
    </div>
    {stats.plates === 0 ? <Empty>Your stats will appear after the first plate.</Empty> : <div className="dashboard-grid">
      <article className="card">
        <h3>Top pasta combinations</h3>
        <Ranked items={stats.combinations} />
      </article>
      <article className="card">
        <h3>Plate leaderboard</h3>
        <Ranked items={stats.perPerson} />
      </article>
      <article className="card highlight">
        <p className="eyebrow">Personal best</p>
        <strong>{stats.largestOuting?.count ?? 0} plates</strong>
        <span>{stats.largestOuting?.label ?? 'No outings yet'}</span>
      </article>
      <article className="card">
        <h3>Outcomes</h3>
        <Ranked items={stats.outcomes} />
        <p className="muted">Average {stats.average.toFixed(1)} plates per outing</p>
      </article>
    </div>}
  </section>
}

function Ranked({ items }: { items: Array<{ label: string; count: number }> }) {
  if (!items.length) return <p className="muted">Nothing to rank yet.</p>
  return <ol className="ranking">{items.slice(0, 5).map((item) =>
    <li key={item.label}><span>{item.label}</span><strong>{item.count}</strong></li>,
  )}</ol>
}

function Trips({ data, setData }: { data: AppData; setData: (data: AppData) => void }) {
  const [selectedId, setSelectedId] = useState<string>()
  const selected = data.trips.find((trip) => trip.id === selectedId)
  const people = data.people.filter((person) => !person.archived)
  const seasons = data.seasons.filter((season) => !season.archived)

  const createTrip = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault()
    const form = new FormData(event.currentTarget)
    const participantIds = people.filter((person) => form.get(`person-${person.id}`)).map((person) => person.id)
    if (!participantIds.length) return
    const trip: Trip = {
      id: id(),
      seasonId: String(form.get('seasonId')),
      date: String(form.get('date')),
      location: String(form.get('location') || ''),
      notes: '',
      participantIds,
      plates: [],
    }
    setData({ ...data, trips: [trip, ...data.trips] })
    setSelectedId(trip.id)
    event.currentTarget.reset()
  }

  if (selected) return <TripDetail
    data={data}
    trip={selected}
    onBack={() => setSelectedId(undefined)}
    update={(trip) => setData({ ...data, trips: data.trips.map((item) => item.id === trip.id ? trip : item) })}
    remove={() => {
      if (confirm('Delete this outing and every plate in it?')) {
        setData({ ...data, trips: data.trips.filter((item) => item.id !== selected.id) })
        setSelectedId(undefined)
      }
    }}
  />

  return <section className="page">
    <header className="page-head"><div><p className="eyebrow">Mangia, mangia!</p><h2>Outings</h2></div></header>
    {!people.length || !seasons.length ? <Empty>Add at least one person and season in Setup before starting an outing.</Empty> :
      <form className="card form-grid" onSubmit={createTrip}>
        <h3>Start an outing</h3>
        <Field label="Season"><select name="seasonId" required>{seasons.map((season) => <option key={season.id} value={season.id}>{season.name}</option>)}</select></Field>
        <Field label="Date"><input type="date" name="date" defaultValue={today()} required /></Field>
        <Field label="Location (optional)"><input name="location" placeholder="Your Olive Garden" /></Field>
        <fieldset><legend>Who's eating?</legend><div className="chips">{people.map((person) =>
          <label key={person.id}><input type="checkbox" name={`person-${person.id}`} defaultChecked />{person.name}</label>,
        )}</div></fieldset>
        <button className="button primary" type="submit">Start twirling</button>
      </form>}
    <div className="list">
      {data.trips.length === 0 && <Empty>No outings yet. The pasta possibilities are endless.</Empty>}
      {data.trips.map((trip) => {
        const season = data.seasons.find((item) => item.id === trip.seasonId)
        return <button className="trip-row" key={trip.id} onClick={() => setSelectedId(trip.id)}>
          <span className="trip-emoji" aria-hidden="true">🍽️</span>
          <span><strong>{trip.location || season?.name || 'Pasta outing'}</strong><small>{trip.date} · {trip.participantIds.length} people</small></span>
          <b>{trip.plates.length}<small>plates</small></b>
        </button>
      })}
    </div>
  </section>
}

function TripDetail({ data, trip, update, remove, onBack }: {
  data: AppData
  trip: Trip
  update: (trip: Trip) => void
  remove: () => void
  onBack: () => void
}) {
  const season = data.seasons.find((item) => item.id === trip.seasonId)
  const [showPlate, setShowPlate] = useState(false)
  const [duplicate, setDuplicate] = useState<Partial<ReturnType<typeof emptyPlate>>>()
  const people = trip.participantIds.map((personId) => data.people.find((person) => person.id === personId)).filter(Boolean)
  return <section className="page">
    <header className="page-head">
      <button className="back" onClick={onBack} aria-label="Back to outings">←</button>
      <div><p className="eyebrow">{season?.name}</p><h2>{trip.location || trip.date}</h2><p className="muted">{trip.date}</p></div>
      <button className="link danger" onClick={remove}>Delete</button>
    </header>
    <button className="button primary sticky-add" onClick={() => { setDuplicate(undefined); setShowPlate(true) }}>＋ Add a plate</button>
    {showPlate && season && <PlateForm
      season={season}
      people={people as AppData['people']}
      initial={duplicate}
      onCancel={() => setShowPlate(false)}
      onSave={(plate) => { update({ ...trip, plates: [...trip.plates, plate] }); setShowPlate(false) }}
    />}
    {people.map((person) => {
      const plates = trip.plates.filter((plate) => plate.personId === person?.id)
      return <article className="person-group" key={person?.id}>
        <h3>{person?.name} <span>{plates.length} plates</span></h3>
        {plates.length === 0 && <p className="muted">Waiting for their first plate…</p>}
        {plates.map((plate, index) => <div className="plate-row" key={plate.id}>
          {plate.photo ? <img src={plate.photo} alt="" /> : <span className="plate-number">{index + 1}</span>}
          <span><strong>{plateLabel(plate)}</strong><small>{plate.outcome.replace('-', ' ')}{plate.notes ? ` · ${plate.notes}` : ''}</small></span>
          <div className="row-actions">
            <button className="icon-button" title="Order this again" onClick={() => {
              setDuplicate({
                personId: plate.personId, kind: plate.kind, size: plate.size,
                pastaId: plate.pastaId, sauceId: plate.sauceId, proteinId: plate.proteinId,
                dishId: plate.dishId, outcome: plate.outcome, photo: plate.photo, notes: plate.notes,
              })
              setShowPlate(true)
            }}>↻</button>
            <button className="icon-button danger" title="Delete plate" onClick={() => {
              if (confirm('Delete this plate?')) update({ ...trip, plates: trip.plates.filter((item) => item.id !== plate.id) })
            }}>×</button>
          </div>
        </div>)}
      </article>
    })}
  </section>
}

const emptyPlate = () => ({
  personId: '',
  kind: 'pasta' as PlateKind,
  size: 'small' as PastaSize,
  pastaId: '',
  sauceId: '',
  proteinId: '',
  dishId: '',
  outcome: 'eaten' as Outcome,
  photo: undefined as string | undefined,
  notes: '',
})

function PlateForm({ season, people, initial, onSave, onCancel }: {
  season: Season
  people: AppData['people']
  initial?: Partial<ReturnType<typeof emptyPlate>>
  onSave: (plate: Trip['plates'][number]) => void
  onCancel: () => void
}) {
  const [form, setForm] = useState({ ...emptyPlate(), personId: people[0]?.id ?? '', ...initial })
  const menu = form.kind === 'soup' ? active(season.soups) : [season.salad]
  const submit = (event: FormEvent) => {
    event.preventDefault()
    if (form.kind === 'pasta') {
      const pasta = season.pastas.find((item) => item.id === form.pastaId)
      const sauce = season.sauces.find((item) => item.id === form.sauceId)
      const protein = season.proteins.find((item) => item.id === form.proteinId)
      if (!pasta || !sauce) return
      onSave({
        id: id(), personId: form.personId, kind: 'pasta', size: form.size,
        pastaId: pasta.id, pastaName: pasta.name, sauceId: sauce.id, sauceName: sauce.name,
        proteinId: protein?.id, proteinName: protein?.name, dishName: `${pasta.name} + ${sauce.name}`,
        outcome: form.outcome, photo: form.photo, notes: form.notes, createdAt: new Date().toISOString(),
      })
    } else {
      const selected = form.kind === 'salad' ? season.salad : menu.find((item) => item.id === form.dishId)
      if (!selected) return
      onSave({
        id: id(), personId: form.personId, kind: form.kind, dishId: selected.id, dishName: selected.name,
        outcome: form.outcome, photo: form.photo, notes: form.notes, createdAt: new Date().toISOString(),
      })
    }
  }
  return <div className="modal-backdrop"><form className="modal" onSubmit={submit}>
    <header><h3>Add a plate</h3><button type="button" className="icon-button" onClick={onCancel}>×</button></header>
    <Field label="Person"><select value={form.personId} onChange={(e) => setForm({ ...form, personId: e.target.value })}>
      {people.map((person) => <option key={person.id} value={person.id}>{person.name}</option>)}
    </select></Field>
    <fieldset><legend>Type</legend><div className="segmented">
      {(['pasta', 'soup', 'salad'] as PlateKind[]).map((kind) => <label key={kind}>
        <input type="radio" name="kind" value={kind} checked={form.kind === kind} onChange={() => setForm({ ...form, kind })} />{kind}
      </label>)}
    </div></fieldset>
    {form.kind === 'pasta' ? <>
      <Field label="Pasta"><select required value={form.pastaId} onChange={(e) => setForm({ ...form, pastaId: e.target.value })}>
        <option value="">Choose pasta…</option>{active(season.pastas).map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}
      </select></Field>
      <Field label="Sauce"><select required value={form.sauceId} onChange={(e) => setForm({ ...form, sauceId: e.target.value })}>
        <option value="">Choose sauce…</option>{active(season.sauces).map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}
      </select></Field>
      <Field label="Protein"><select value={form.proteinId} onChange={(e) => setForm({ ...form, proteinId: e.target.value })}>
        <option value="">No protein</option>{active(season.proteins).map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}
      </select></Field>
      <fieldset><legend>Size</legend><div className="segmented">{(['small', 'large'] as PastaSize[]).map((size) =>
        <label key={size}><input type="radio" name="size" checked={form.size === size} onChange={() => setForm({ ...form, size })} />{size}</label>,
      )}</div></fieldset>
    </> : form.kind === 'soup' ? <Field label="Soup"><select required value={form.dishId} onChange={(e) => setForm({ ...form, dishId: e.target.value })}>
      <option value="">Choose soup…</option>{menu.map((item) => <option key={item.id} value={item.id}>{item.name}</option>)}
    </select></Field> : <div className="selected-dish"><span>🥗</span><strong>{season.salad.name}</strong></div>}
    <fieldset><legend>What happened?</legend><div className="segmented outcomes">
      {([['eaten', 'Eaten'], ['taken-home', 'Taken home'], ['left-behind', 'Left behind']] as [Outcome, string][]).map(([outcome, label]) =>
        <label key={outcome}><input type="radio" name="outcome" checked={form.outcome === outcome} onChange={() => setForm({ ...form, outcome })} />{label}</label>,
      )}
    </div></fieldset>
    <ImageInput label="plate photo" value={form.photo} onChange={(photo) => setForm({ ...form, photo })} />
    <Field label="Notes (optional)"><input value={form.notes} onChange={(e) => setForm({ ...form, notes: e.target.value })} placeholder="Extra cheesy, perfect bite…" /></Field>
    <footer><button type="button" className="button secondary" onClick={onCancel}>Cancel</button><button className="button primary">Save plate</button></footer>
  </form></div>
}

function Setup({ data, setData }: { data: AppData; setData: (data: AppData) => void }) {
  const [seasonId, setSeasonId] = useState<string>()
  const season = data.seasons.find((item) => item.id === seasonId)
  const addPerson = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault()
    const form = new FormData(event.currentTarget)
    const name = String(form.get('name')).trim()
    if (!name) return
    setData({ ...data, people: [...data.people, { id: id(), name, archived: false }] })
    event.currentTarget.reset()
  }
  const addSeason = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault()
    const form = new FormData(event.currentTarget)
    const name = String(form.get('name')).trim()
    if (!name) return
    const next: Season = {
      id: id(), name, startDate: String(form.get('startDate') || ''), endDate: String(form.get('endDate') || ''),
      notes: '', archived: false, pastas: [], sauces: [], proteins: [], soups: [],
      salad: dish('House Salad'),
    }
    setData({ ...data, seasons: [...data.seasons, next] })
    setSeasonId(next.id)
  }
  if (season) return <SeasonEditor season={season} onBack={() => setSeasonId(undefined)} onChange={(changed) =>
    setData({ ...data, seasons: data.seasons.map((item) => item.id === changed.id ? changed : item) })
  } />
  return <section className="page">
    <header className="page-head"><div><p className="eyebrow">Set the table</p><h2>People & seasons</h2></div></header>
    <div className="setup-grid">
      <article className="card">
        <h3>Tracked people</h3>
        <form className="inline-form" onSubmit={addPerson}><input name="name" aria-label="Person name" placeholder="Name" required /><button className="button primary compact">Add</button></form>
        <div className="manage-list">{data.people.map((person) => <div key={person.id}>
          <span className={person.archived ? 'archived' : ''}>{person.name}</span>
          <button className="link" onClick={() => setData({ ...data, people: data.people.map((item) => item.id === person.id ? { ...item, archived: !item.archived } : item) })}>
            {person.archived ? 'Restore' : 'Archive'}
          </button>
        </div>)}</div>
      </article>
      <article className="card">
        <h3>Promotion seasons</h3>
        <form className="form-grid" onSubmit={addSeason}>
          <Field label="Name"><input name="name" placeholder="2026 Pasta Bowl" required /></Field>
          <div className="two-col"><Field label="Starts (optional)"><input name="startDate" type="date" /></Field><Field label="Ends (optional)"><input name="endDate" type="date" /></Field></div>
          <button className="button primary">Create season</button>
        </form>
        <div className="manage-list">{data.seasons.map((item) => <button className="season-link" key={item.id} onClick={() => setSeasonId(item.id)}>
          <span><strong>{item.name}</strong><small>{item.pastas.length} pastas · {item.sauces.length} sauces</small></span><b>→</b>
        </button>)}</div>
      </article>
    </div>
  </section>
}

function SeasonEditor({ season, onChange, onBack }: { season: Season; onChange: (season: Season) => void; onBack: () => void }) {
  const updateList = (key: 'pastas' | 'sauces' | 'proteins' | 'soups', items: Dish[]) => onChange({ ...season, [key]: items })
  return <section className="page">
    <header className="page-head"><button className="back" onClick={onBack}>←</button><div><p className="eyebrow">Season menu</p><h2>{season.name}</h2></div></header>
    <div className="menu-grid">
      {(['pastas', 'sauces', 'proteins', 'soups'] as const).map((key) =>
        <DishList key={key} title={key[0].toUpperCase() + key.slice(1)} dishes={season[key]} onChange={(items) => updateList(key, items)} />,
      )}
      <article className="card">
        <h3>Salad</h3>
        <Field label="Dish name"><input value={season.salad.name} onChange={(e) => onChange({ ...season, salad: { ...season.salad, name: e.target.value } })} /></Field>
        <ImageInput label="menu photo" value={season.salad.image} onChange={(image) => onChange({ ...season, salad: { ...season.salad, image } })} />
      </article>
      <article className="card">
        <h3>Season details</h3>
        <Field label="Notes"><textarea value={season.notes} onChange={(e) => onChange({ ...season, notes: e.target.value })} /></Field>
        <button className="button secondary" onClick={() => onChange({ ...season, archived: !season.archived })}>{season.archived ? 'Restore season' : 'Archive season'}</button>
      </article>
    </div>
  </section>
}

function DishList({ title, dishes, onChange }: { title: string; dishes: Dish[]; onChange: (dishes: Dish[]) => void }) {
  const add = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault()
    const name = String(new FormData(event.currentTarget).get('name')).trim()
    if (name) onChange([...dishes, dish(name)])
    event.currentTarget.reset()
  }
  return <article className="card dish-list">
    <h3>{title}</h3>
    <form className="inline-form" onSubmit={add}><input name="name" aria-label={`New ${title}`} placeholder={`Add ${title.toLowerCase().slice(0, -1)}`} required /><button className="button primary compact">Add</button></form>
    {dishes.map((item) => <div className="dish-item" key={item.id}>
      {item.image ? <img src={item.image} alt="" /> : <span aria-hidden="true">🍝</span>}
      <strong className={item.archived ? 'archived' : ''}>{item.name}</strong>
      <ImageInput label="menu photo" value={item.image} onChange={(image) => onChange(dishes.map((entry) => entry.id === item.id ? { ...entry, image } : entry))} />
      <button className="link" onClick={() => onChange(dishes.map((entry) => entry.id === item.id ? { ...entry, archived: !entry.archived } : entry))}>{item.archived ? 'Restore' : 'Archive'}</button>
    </div>)}
  </article>
}

function Settings({ data, replaceData, updates }: { data: AppData; replaceData: (data: AppData) => void; updates: UpdateControls }) {
  const fileRef = useRef<HTMLInputElement>(null)
  const [message, setMessage] = useState('')
  const [updateState, setUpdateState] = useState<UpdateState>(updates.updateReady ? 'available' : 'idle')
  useEffect(() => { if (updates.updateReady) setUpdateState('available') }, [updates.updateReady])
  const checkUpdate = async () => {
    if (!navigator.onLine) return setUpdateState('offline')
    if (!updates.registration) return setUpdateState('error')
    setUpdateState('checking')
    try {
      await updates.registration.update()
      setUpdateState(updates.updateReady || updates.registration.waiting ? 'available' : 'current')
    } catch { setUpdateState('error') }
  }
  const updateText: Record<UpdateState, string> = {
    idle: 'Check for update', checking: 'Checking…', current: 'App is up to date', available: 'Update available',
    offline: 'Go online to check', error: 'Could not check—try again',
  }
  return <section className="page">
    <header className="page-head"><div><p className="eyebrow">Your pasta pantry</p><h2>Settings</h2></div></header>
    <div className="settings-grid">
      <article className="card">
        <h3>Backup & restore</h3><p className="muted">Photos are compressed and included in your JSON backup.</p>
        <div className="button-stack"><button className="button primary" onClick={() => exportData(data)}>Export JSON backup</button>
          <button className="button secondary" onClick={() => fileRef.current?.click()}>Import & replace data</button></div>
        <input ref={fileRef} hidden type="file" accept="application/json,.json" onChange={async (event) => {
          const file = event.target.files?.[0]
          if (!file) return
          try {
            const imported = await readImport(file)
            if (confirm(`Replace everything with ${imported.people.length} people, ${imported.seasons.length} seasons, and ${imported.trips.length} outings?`)) {
              replaceData(imported); setMessage('Backup imported successfully.')
            }
          } catch (caught) { setMessage(caught instanceof Error ? caught.message : 'Import failed.') }
          event.target.value = ''
        }} />
        {message && <p role="status" className="notice">{message}</p>}
      </article>
      <article className="card">
        <h3>Offline app</h3><p className="muted">Once loaded, the tracker and your data work without a connection.</p>
        {updateState === 'available'
          ? <button className="button primary" onClick={updates.applyUpdate}>Install update & reload</button>
          : <button className="button secondary" disabled={updateState === 'checking'} onClick={checkUpdate}>{updateText[updateState]}</button>}
      </article>
      <article className="card danger-zone">
        <h3>Clear local data</h3><p className="muted">This cannot be undone without an exported backup.</p>
        <button className="button danger-button" onClick={() => {
          if (confirm('Permanently delete all local pasta tracker data?')) {
            localStorage.removeItem(STORAGE_KEY)
            replaceData(loadData())
          }
        }}>Delete everything</button>
      </article>
      <article className="card privacy"><span aria-hidden="true">🔒</span><div><h3>Private by design</h3><p className="muted">Everything stays in this browser unless you export it. No accounts, analytics, or uploads.</p></div></article>
    </div>
  </section>
}

export default function App({ updates = { updateReady: false, applyUpdate: () => undefined } }: { updates?: UpdateControls }) {
  const [data, setDataState] = useState(loadData)
  const [tab, setTab] = useState<Tab>('dashboard')
  const [online, setOnline] = useState(navigator.onLine)
  const setData = (next: AppData) => {
    try {
      saveData(next)
      setDataState(next)
    } catch {
      alert('Your browser is out of storage. Export a backup, then remove some photos or old data.')
    }
  }
  useEffect(() => {
    const update = () => setOnline(navigator.onLine)
    addEventListener('online', update); addEventListener('offline', update)
    return () => { removeEventListener('online', update); removeEventListener('offline', update) }
  }, [])
  return <div className="app-shell">
    <header className="brand"><div className="logo" aria-hidden="true">🍝</div><div><h1>Never Ending</h1><p>Pasta Tracker</p></div><span className={online ? 'online' : 'offline'}>{online ? 'Online' : 'Offline'}</span></header>
    <main>
      {tab === 'dashboard' && <Dashboard data={data} />}
      {tab === 'trips' && <Trips data={data} setData={setData} />}
      {tab === 'setup' && <Setup data={data} setData={setData} />}
      {tab === 'settings' && <Settings data={data} replaceData={setData} updates={updates} />}
    </main>
    <nav className="bottom-nav" aria-label="Main navigation">
      {([['dashboard', '⌂', 'Stats'], ['trips', '🍽', 'Outings'], ['setup', '⚙', 'Setup'], ['settings', '☰', 'Settings']] as [Tab, string, string][]).map(([value, icon, label]) =>
        <button key={value} className={tab === value ? 'active' : ''} onClick={() => setTab(value)}><span aria-hidden="true">{icon}</span>{label}</button>,
      )}
    </nav>
  </div>
}
