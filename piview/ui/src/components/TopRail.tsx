import type { ConnectionStatus, DatabaseSnapshot } from '../types'
import { Broom } from './Icons'

interface Props {
  status: ConnectionStatus
  database: DatabaseSnapshot
  busy: boolean
  onClear: () => void
}

export function TopRail({ status, database, busy, onClear }: Props) {
  const live = status.connected

  return (
    <header className="top-rail">
      <div className="brand">
        <span className="brand__pi" aria-hidden="true">
          π
        </span>
        <span className="brand__name">piview</span>
      </div>

      <div className="gauge">
        <span className="gauge__label">interpreter</span>
        <span className="gauge__value link-target">
          <span className={`pulse${live ? '' : ' pulse--down'}`} aria-hidden="true" />
          {status.host}:{status.port}
        </span>
      </div>

      <div className="gauge">
        <span className="gauge__label">state</span>
        <span className="gauge__value" style={{ color: live ? 'var(--phosphor)' : 'var(--alarm)' }}>
          {live ? 'answering' : 'no answer'}
        </span>
      </div>

      <div className="gauge gauge--optional">
        <span className="gauge__label">clauses</span>
        <span className="gauge__value">{database.clauses.length}</span>
      </div>

      <div className="gauge gauge--optional">
        <span className="gauge__label">predicates</span>
        <span className="gauge__value">{database.predicates.length}</span>
      </div>

      <div className="gauge gauge--optional">
        <span className="gauge__label">mcp endpoint</span>
        <span className="gauge__value gauge__value--dim">/mcp</span>
      </div>

      <span className="top-rail__spacer" />

      <button
        className="rail-button rail-button--alarm"
        onClick={onClear}
        disabled={busy || database.clauses.length === 0}
        title="Remove every clause from the shared database"
        /* the label is hidden on narrow screens, which would take the button's
           accessible name with it */
        aria-label="Clear database"
      >
        <Broom />
        <span>Clear database</span>
      </button>
    </header>
  )
}
