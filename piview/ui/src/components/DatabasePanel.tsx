import { useMemo, useState } from 'react'
import type { Clause, DatabaseSnapshot, SampleFile } from '../types'
import { ChevronRight, Search, Trash } from './Icons'
import { PrologText } from './PrologText'

interface Props {
  database: DatabaseSnapshot
  samples: SampleFile[]
  busy: boolean
  onQueryPredicate: (goal: string) => void
  onRemoveClause: (clause: Clause) => void
  onLoadSample: (sample: SampleFile) => void
}

export function DatabasePanel({
  database,
  samples,
  busy,
  onQueryPredicate,
  onRemoveClause,
  onLoadSample,
}: Props) {
  const [filter, setFilter] = useState('')
  const [closed, setClosed] = useState<Set<string>>(() => new Set())

  const predicates = useMemo(() => {
    const needle = filter.trim().toLowerCase()
    if (!needle) return database.predicates
    return database.predicates
      .map((predicate) => {
        if (predicate.indicator.toLowerCase().includes(needle)) return predicate
        const clauses = predicate.clauses.filter((clause) =>
          clause.text.toLowerCase().includes(needle),
        )
        return clauses.length ? { ...predicate, clauses } : null
      })
      .filter((predicate) => predicate !== null)
  }, [database.predicates, filter])

  const toggle = (indicator: string) =>
    setClosed((previous) => {
      const next = new Set(previous)
      if (next.has(indicator)) next.delete(indicator)
      else next.add(indicator)
      return next
    })

  const searching = filter.trim().length > 0

  return (
    <section className="panel panel--left" aria-label="Database">
      <header className="panel__head">
        <h2 className="label">Database</h2>
        <span className="panel__count">
          {database.clauses.length} clause{database.clauses.length === 1 ? '' : 's'}
        </span>
      </header>

      <div className="search">
        <span className="search__glyph">
          <Search />
        </span>
        <input
          type="search"
          value={filter}
          onChange={(event) => setFilter(event.target.value)}
          placeholder="filter predicates and clauses"
          aria-label="Filter the database"
        />
      </div>

      <div className="panel__body scroller">
        {predicates.length === 0 ? (
          <p className="empty">
            {database.clauses.length === 0
              ? 'Nothing loaded. Assert a clause below, or load a sample.'
              : `No predicate or clause matches “${filter.trim()}”.`}
          </p>
        ) : (
          predicates.map((predicate) => {
            const open = searching || !closed.has(predicate.indicator)
            const facts = predicate.clauses.filter((clause) => !clause.isRule).length
            const rules = predicate.clauses.length - facts
            return (
              <div className="predicate" key={predicate.indicator}>
                <button
                  className="predicate__head"
                  onClick={() => toggle(predicate.indicator)}
                  aria-expanded={open}
                  title={`${predicate.indicator} — click to ${open ? 'collapse' : 'expand'}`}
                >
                  <span className="predicate__chevron" data-open={open}>
                    <ChevronRight />
                  </span>
                  <span className="predicate__name">{predicate.name}</span>
                  <span className="predicate__arity">/{predicate.arity}</span>
                  <span className="predicate__tally">
                    {facts > 0 && (
                      <span className="tally tally--fact" title={`${facts} fact(s)`}>
                        {facts}
                      </span>
                    )}
                    {rules > 0 && (
                      <span className="tally tally--rule" title={`${rules} rule(s)`}>
                        {rules}
                      </span>
                    )}
                  </span>
                </button>

                {open &&
                  predicate.clauses.map((clause) => (
                    <div className="clause" key={clause.index}>
                      <span className="clause__index">
                        {String(clause.index).padStart(3, '0')}
                      </span>
                      <button
                        className="clause__text"
                        style={{ textAlign: 'left' }}
                        onClick={() => onQueryPredicate(goalFor(predicate.name, predicate.arity))}
                        title={`Query ${predicate.indicator}`}
                      >
                        <PrologText source={clause.text} />
                      </button>
                      <button
                        className="clause__remove"
                        onClick={() => onRemoveClause(clause)}
                        disabled={busy}
                        aria-label={`Delete clause ${clause.index}: ${clause.text}`}
                        title={`Delete clause ${clause.index}`}
                      >
                        <Trash />
                      </button>
                    </div>
                  ))}
              </div>
            )
          })
        )}
      </div>

      <div className="samples">
        {samples.length > 0 && (
          <>
            <header className="panel__head" style={{ borderBottom: 'none' }}>
              <h2 className="label">Samples</h2>
            </header>
            <div className="samples__list">
              {samples.map((sample) => (
                <button
                  key={sample.path}
                  className="chip"
                  onClick={() => onLoadSample(sample)}
                  disabled={busy}
                  title={`Load ${sample.path} into the running database`}
                >
                  {sample.name}
                </button>
              ))}
            </div>
          </>
        )}
        <header className="panel__head" style={{ borderBottom: 'none' }}>
          <h2 className="label">Reference</h2>
        </header>
        <div className="samples__list">
          <a
            className="chip"
            /* relative, like every other asset the bundle emits: nginx serves
               this view from /pi/ with the prefix stripped, and an absolute
               path would leave that location and 404 on the site root */
            href="iso-prolog.pdf"
            download
            title="How ISO Prolog works, with examples - and what pi implements of it"
          >
            iso prolog &middot; pdf
          </a>
        </div>
      </div>
    </section>
  )
}

/** `father/2` becomes `?- father(X, Y).` - a query worth running, prefilled */
export function goalFor(name: string, arity: number): string {
  if (arity === 0) return `${name}`
  const names = ['X', 'Y', 'Z', 'A', 'B', 'C', 'D', 'E']
  const args = Array.from({ length: arity }, (_, i) => names[i] ?? `V${i}`)
  return `${name}(${args.join(', ')})`
}
