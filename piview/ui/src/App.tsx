import { useCallback, useEffect, useRef, useState } from 'react'
import { api } from './api'
import { ActivityFeed } from './components/ActivityFeed'
import { Console, isQuery } from './components/Console'
import { DatabasePanel } from './components/DatabasePanel'
import { Alert, Wave } from './components/Icons'
import { RelationGraph } from './components/RelationGraph'
import { TopRail } from './components/TopRail'
import { applyTheme, storedTheme, type Theme } from './theme'
import type { Clause, SampleFile, TranscriptBody, TranscriptEntry } from './types'
import { useSession } from './useSession'

type Inspector = 'relations' | 'activity'
type MobilePanel = 'database' | 'console' | 'inspect'

export function App() {
  const { status, database, activity, socketOpen } = useSession()

  const [transcript, setTranscript] = useState<TranscriptEntry[]>([])
  const [samples, setSamples] = useState<SampleFile[]>([])
  const [inspector, setInspector] = useState<Inspector>('relations')
  const [mobilePanel, setMobilePanel] = useState<MobilePanel>('console')
  const [busy, setBusy] = useState(false)
  const [theme, setTheme] = useState<Theme>(storedTheme)

  const nextId = useRef(1)
  const seenActivity = useRef(0)

  const append = useCallback((entry: TranscriptBody) => {
    const id = nextId.current++
    setTranscript((previous) => [...previous, { ...entry, id, at: Date.now() }])
    return id
  }, [])

  const replace = useCallback((id: number, entry: TranscriptBody) => {
    setTranscript((previous) =>
      previous.map((item): TranscriptEntry =>
        item.id === id ? { ...entry, id, at: item.at } : item,
      ),
    )
  }, [])

  useEffect(() => {
    api.samples().then(setSamples).catch(() => setSamples([]))
  }, [])

  // the inline script in index.html has already applied a stored choice; this
  // keeps <html> and the store in step with every toggle after that
  useEffect(() => applyTheme(theme), [theme])

  // an agent's queries belong in the transcript too - the console is a view of
  // the interpreter, not a log of this browser tab
  useEffect(() => {
    if (seenActivity.current === 0) {
      seenActivity.current = activity.length ? activity[activity.length - 1].id : 0
      return
    }
    const fresh = activity.filter(
      (entry) => entry.id > seenActivity.current && entry.source === 'mcp',
    )
    if (!fresh.length) return
    seenActivity.current = activity[activity.length - 1].id
    for (const entry of fresh) {
      append({
        kind: 'note',
        tone: entry.outcome,
        title: `agent · ${entry.command}`,
        body: entry.summary,
      })
    }
  }, [activity, append])

  const runQuery = useCallback(
    async (goal: string) => {
      const pendingId = append({ kind: 'pending', goal })
      setBusy(true)
      try {
        const result = await api.query(goal)
        replace(pendingId, { kind: 'query', result })
      } catch (error) {
        replace(pendingId, {
          kind: 'note',
          tone: 'error',
          title: 'piview could not reach the interpreter',
          body: String(error),
        })
      } finally {
        setBusy(false)
      }
    },
    [append, replace],
  )

  const addClauses = useCallback(
    async (source: string, origin = 'typed here') => {
      setBusy(true)
      try {
        const results = await api.assert(source)
        append({ kind: 'assert', source: origin, results })
      } catch (error) {
        append({
          kind: 'note',
          tone: 'error',
          title: 'piview could not reach the interpreter',
          body: String(error),
        })
      } finally {
        setBusy(false)
      }
    },
    [append],
  )

  const submit = useCallback(
    (text: string) => (isQuery(text) || !looksLikeClause(text) ? runQuery(text) : addClauses(text)),
    [runQuery, addClauses],
  )

  const removeClause = useCallback(
    async (clause: Clause) => {
      setBusy(true)
      try {
        const result = await api.remove(clause.index)
        append({
          kind: 'note',
          tone: result.ok ? 'success' : 'error',
          title: result.ok
            ? `deleted ${clause.index}  ${clause.text}.`
            : `could not delete ${clause.index}`,
          body: result.ok ? undefined : result.message,
        })
      } finally {
        setBusy(false)
      }
    },
    [append],
  )

  const loadSample = useCallback(
    async (sample: SampleFile) => {
      setBusy(true)
      try {
        const result = await api.load(sample.path)
        append({
          kind: 'note',
          tone: result.ok ? 'success' : 'error',
          title: result.ok ? `loaded ${sample.name}` : `could not load ${sample.name}`,
          body: result.message,
        })
      } finally {
        setBusy(false)
      }
    },
    [append],
  )

  const clearDatabase = useCallback(async () => {
    const count = database.clauses.length
    if (!window.confirm(`Remove all ${count} clauses? pi's own prompt shares this database.`)) return
    setBusy(true)
    try {
      const result = await api.clear()
      append({ kind: 'note', tone: 'success', title: result.message })
    } finally {
      setBusy(false)
    }
  }, [append, database.clauses.length])

  const prefill = useCallback((goal: string) => {
    setMobilePanel('console')
    runQuery(goal)
  }, [runQuery])

  return (
    <div className="shell" data-mobile-panel={mobilePanel}>
      <TopRail
        status={status}
        database={database}
        busy={busy}
        theme={theme}
        onToggleTheme={() => setTheme((current) => (current === 'dark' ? 'light' : 'dark'))}
        onClear={clearDatabase}
      />

      <nav className="mobile-switch" aria-label="Panels">
        {(['database', 'console', 'inspect'] as MobilePanel[]).map((panel) => (
          <button
            key={panel}
            className="tab"
            role="tab"
            aria-selected={mobilePanel === panel}
            onClick={() => setMobilePanel(panel)}
          >
            {panel}
          </button>
        ))}
      </nav>

      <DatabasePanel
        database={database}
        samples={samples}
        busy={busy}
        onQueryPredicate={prefill}
        onRemoveClause={removeClause}
        onLoadSample={loadSample}
      />

      <main style={{ display: 'flex', flexDirection: 'column', minWidth: 0, minHeight: 0 }}>
        {!status.connected && <Disconnected socketOpen={socketOpen} detail={status.detail} />}
        <Console transcript={transcript} database={database} busy={busy} onSubmit={submit} />
      </main>

      <section className="panel panel--right" aria-label="Inspector">
        <div className="tabs" role="tablist">
          <button
            className="tab"
            role="tab"
            aria-selected={inspector === 'relations'}
            onClick={() => setInspector('relations')}
          >
            Relations
          </button>
          <button
            className="tab"
            role="tab"
            aria-selected={inspector === 'activity'}
            onClick={() => setInspector('activity')}
          >
            Activity
            {activity.length > 0 && <span className="tab__badge">{activity.length}</span>}
          </button>
        </div>

        {inspector === 'relations' ? (
          <div className="panel__body">
            <RelationGraph database={database} onPickNode={(atom) => prefill(guessGoal(atom, database))} />
          </div>
        ) : (
          <ActivityFeed entries={activity} />
        )}
      </section>
    </div>
  )
}

// ------------------------------------------------------------------ notices

function Disconnected({ socketOpen, detail }: { socketOpen: boolean; detail?: string }) {
  return (
    <div className="notice" role="status">
      {socketOpen ? <Alert /> : <Wave />}
      <div>
        {socketOpen ? (
          <>
            <strong>pi is not answering.</strong> Start it with its port open —{' '}
            <code>./prolog --port 7071 samples/family.pl</code> — and this view reconnects on its
            own. {detail && <span style={{ color: 'var(--text-3)' }}>{detail}</span>}
          </>
        ) : (
          <>
            <strong>Reconnecting to piview.</strong> The web server went away; this page keeps
            trying.
          </>
        )}
      </div>
    </div>
  )
}

// ------------------------------------------------------------------ helpers

/**
 * A clause ends in something that could be asserted; a goal usually does not.
 * The `?-` prefix is the reliable signal and is checked first - this only
 * catches the case where someone pastes `likes(pete,prolog).` with no marker.
 */
function looksLikeClause(text: string): boolean {
  const trimmed = text.trim()
  return trimmed.endsWith('.') || /:-/.test(trimmed)
}

/** clicking `peter` in the graph should ask something useful about peter */
function guessGoal(atom: string, database: { predicates: { name: string; arity: number }[] }): string {
  const binary = database.predicates.find((predicate) => predicate.arity === 2)
  if (!binary) return atom
  return `${binary.name}(X, ${atom})`
}
