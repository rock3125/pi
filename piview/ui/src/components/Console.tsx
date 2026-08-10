import { useEffect, useLayoutEffect, useRef, useState } from 'react'
import type { CSSProperties, KeyboardEvent as ReactKeyboardEvent } from 'react'
import type { DatabaseSnapshot, QueryResult, TranscriptEntry } from '../types'
import { goalFor } from './DatabasePanel'
import { ChevronRight, Replay } from './Icons'
import { PrologText } from './PrologText'

const SOLUTION_CEILING = 400

interface Props {
  transcript: TranscriptEntry[]
  database: DatabaseSnapshot
  busy: boolean
  onSubmit: (text: string) => void
}

export function Console({ transcript, database, busy, onSubmit }: Props) {
  const [draft, setDraft] = useState('')
  const [history, setHistory] = useState<string[]>([])
  const [historyAt, setHistoryAt] = useState<number | null>(null)

  const inputRef = useRef<HTMLTextAreaElement>(null)
  const tailRef = useRef<HTMLDivElement>(null)

  // a clause to add, or a goal to solve?  pi decides by the leading `?`, and so
  // does the badge above the prompt - the mode is never a toggle to forget
  const asserting = draft.trim().length > 0 && !isQuery(draft)

  useEffect(() => {
    inputRef.current?.focus()
  }, [])

  useLayoutEffect(() => {
    tailRef.current?.scrollIntoView({ block: 'end' })
  }, [transcript.length])

  useLayoutEffect(() => {
    const input = inputRef.current
    if (!input) return
    input.style.height = 'auto'
    input.style.height = `${input.scrollHeight}px`
  }, [draft])

  const run = (text: string) => {
    const trimmed = text.trim()
    if (!trimmed || busy) return
    setHistory((previous) => [...previous.filter((item) => item !== trimmed), trimmed].slice(-100))
    setHistoryAt(null)
    setDraft('')
    onSubmit(trimmed)
  }

  const prefill = (goal: string) => {
    setDraft(goal)
    inputRef.current?.focus()
  }

  const onKeyDown = (event: ReactKeyboardEvent<HTMLTextAreaElement>) => {
    if (event.key === 'Enter' && !event.shiftKey) {
      event.preventDefault()
      run(draft)
      return
    }

    // history only when the caret cannot go further, so a multi-line clause
    // still edits normally
    const input = event.currentTarget
    if (event.key === 'ArrowUp' && input.selectionStart === 0 && history.length) {
      event.preventDefault()
      const next = historyAt === null ? history.length - 1 : Math.max(0, historyAt - 1)
      setHistoryAt(next)
      setDraft(history[next])
      return
    }
    if (event.key === 'ArrowDown' && input.selectionStart === draft.length && historyAt !== null) {
      event.preventDefault()
      const next = historyAt + 1
      if (next >= history.length) {
        setHistoryAt(null)
        setDraft('')
      } else {
        setHistoryAt(next)
        setDraft(history[next])
      }
    }
  }

  return (
    <section className="console" aria-label="Console">
      <div className="transcript scroller">
        {transcript.length === 0 ? (
          <Opening database={database} onPick={prefill} />
        ) : (
          transcript.map((entry) => (
            <Entry key={entry.id} entry={entry} onRerun={prefill} />
          ))
        )}
        <div ref={tailRef} />
      </div>

      <form
        className="prompt"
        onSubmit={(event) => {
          event.preventDefault()
          run(draft)
        }}
      >
        <div className="prompt__mode">
          <span className={`prompt__badge prompt__badge--${asserting ? 'assert' : 'query'}`}>
            {asserting ? 'Assert' : 'Query'}
          </span>
          <span>
            {asserting
              ? 'added to the database as written'
              : 'solved against the database; ?- and . are optional'}
          </span>
          <span className="prompt__hint">
            <kbd>↵</kbd> run · <kbd>⇧↵</kbd> newline · <kbd>↑</kbd> history
          </span>
        </div>
        <div className="prompt__row">
          <span className="prompt__marker" aria-hidden="true">
            {asserting ? '+' : '?-'}
          </span>
          <textarea
            ref={inputRef}
            className="prompt__input"
            rows={1}
            spellCheck={false}
            autoComplete="off"
            value={draft}
            disabled={busy}
            onChange={(event) => setDraft(event.target.value)}
            onKeyDown={onKeyDown}
            placeholder="father(fred, X)"
            aria-label="Goal to solve, or clause to add"
          />
          <button className="prompt__run" type="submit" disabled={busy || !draft.trim()}>
            {busy ? 'Running' : asserting ? 'Add' : 'Solve'}
          </button>
        </div>
      </form>
    </section>
  )
}

// ------------------------------------------------------------------ entries

function Entry({
  entry,
  onRerun,
}: {
  entry: TranscriptEntry
  onRerun: (goal: string) => void
}) {
  if (entry.kind === 'pending') {
    return (
      <article className="entry" data-outcome="pending">
        <div className="entry__goal">
          <span className="entry__marker">?-</span>
          <span>
            <PrologText source={entry.goal} />
          </span>
        </div>
        <div className="entry__meta">
          <span className="entry__verdict">solving…</span>
        </div>
      </article>
    )
  }

  if (entry.kind === 'assert') {
    const failed = entry.results.filter((result) => !result.ok)
    return (
      <article className="entry" data-outcome={failed.length ? 'error' : 'success'}>
        <div className="entry__goal">
          <span className="entry__marker" style={{ color: 'var(--var)' }}>
            +
          </span>
          <span>
            {entry.results.map((result, index) => (
              <span key={index} style={{ display: 'block' }}>
                <PrologText source={result.command} />
              </span>
            ))}
          </span>
        </div>
        {failed.length > 0 && <pre className="fault">{failed.map((r) => r.message).join('\n')}</pre>}
        <div className="entry__meta">
          <span
            className={`entry__verdict entry__verdict--${failed.length ? 'error' : 'success'}`}
          >
            {failed.length
              ? `${failed.length} rejected`
              : `${entry.results.length} added to the database`}
          </span>
          <span>{entry.source}</span>
        </div>
      </article>
    )
  }

  if (entry.kind === 'note') {
    return (
      <article className="entry" data-outcome={entry.tone}>
        <div className="entry__goal">
          <span className="entry__marker" style={{ color: 'var(--text-3)' }}>
            ·
          </span>
          <span>{entry.title}</span>
        </div>
        {entry.body && <pre className="stream">{entry.body}</pre>}
      </article>
    )
  }

  return <QueryEntry result={entry.result} onRerun={onRerun} />
}

function QueryEntry({
  result,
  onRerun,
}: {
  result: QueryResult
  onRerun: (goal: string) => void
}) {
  const shown = result.solutions.slice(0, SOLUTION_CEILING)
  const hasBindings = result.variables.length > 0

  return (
    <article className="entry" data-outcome={result.outcome}>
      <div className="entry__goal">
        <span className="entry__marker">?-</span>
        <span>
          <PrologText source={stripMarkers(result.goal)} />
        </span>
      </div>

      {result.output && (
        <pre className="stream">
          <span className="stream__label label">written</span>
          {result.output}
        </pre>
      )}

      {result.outcome === 'error' && <pre className="fault">{result.error}</pre>}

      {result.outcome === 'failure' && (
        <p className="answer-no">
          no
          <small>nothing in the database satisfies this goal</small>
        </p>
      )}

      {result.outcome === 'success' &&
        (hasBindings ? (
          <div className="solutions__wrap">
          <table className="solutions">
            <thead>
              <tr>
                <th scope="col">
                  <span className="visually-hidden">#</span>
                </th>
                {result.variables.map((variable) => (
                  <th scope="col" key={variable}>
                    {variable}
                  </th>
                ))}
                <th className="solutions__slack" aria-hidden="true" />
              </tr>
            </thead>
            <tbody>
              {shown.map((solution, index) => (
                <tr key={index} style={{ '--i': index } as CSSProperties}>
                  <td>{index + 1}</td>
                  {result.variables.map((variable) => (
                    <td key={variable}>
                      {solution.bindings[variable] !== undefined ? (
                        <PrologText source={solution.bindings[variable]} />
                      ) : (
                        <span className="tok-op">—</span>
                      )}
                    </td>
                  ))}
                  <td className="solutions__slack" />
                </tr>
              ))}
            </tbody>
          </table>
          </div>
        ) : (
          <p className="answer-yes">yes</p>
        ))}

      {result.solutions.length > shown.length && (
        <p className="solutions__more">
          showing the first {shown.length} of {result.solutions.length}; the rest are in the raw
          answer
        </p>
      )}

      <div className="entry__meta">
        {/* the big answer already says `yes`, `no` or names the fault; the meta
            line only repeats it when it carries a count worth scanning */}
        {hasBindings && result.outcome === 'success' && (
          <span className="entry__verdict entry__verdict--success">{verdict(result)}</span>
        )}
        {result.executionSeconds !== undefined && <span>{formatDuration(result.executionSeconds)}</span>}
        <button className="entry__rerun" onClick={() => onRerun(stripMarkers(result.goal))}>
          <Replay />
          again
        </button>
      </div>

      <details className="raw">
        <summary>
          <ChevronRight />
          raw answer
        </summary>
        <pre>{result.raw || '(nothing came back)'}</pre>
      </details>
    </article>
  )
}

// ------------------------------------------------------------------ empty state

function Opening({
  database,
  onPick,
}: {
  database: DatabaseSnapshot
  onPick: (goal: string) => void
}) {
  const suggestions = database.predicates.slice(0, 6)

  return (
    <div className="empty">
      <p className="empty__title">Nothing run yet.</p>
      <p className="empty__hint">
        Type a goal and press Enter. Anything that is not a goal is added to the database
        instead — pi has no <code>assert/1</code>, so this is how a clause gets in. Remember{' '}
        <code>=</code> both evaluates and unifies here: write <code>N = M + 1</code>, never{' '}
        <code>is</code>.
      </p>
      {suggestions.length > 0 && (
        <div className="empty__goals">
          <span className="label" style={{ marginBottom: '0.15rem' }}>
            loaded now
          </span>
          {suggestions.map((predicate) => (
            <button
              key={predicate.indicator}
              className="suggest"
              onClick={() => onPick(goalFor(predicate.name, predicate.arity))}
            >
              <span className="suggest__marker">?-</span>
              <PrologText source={goalFor(predicate.name, predicate.arity)} />
            </button>
          ))}
        </div>
      )}
    </div>
  )
}

// ------------------------------------------------------------------ helpers

export function isQuery(text: string): boolean {
  return text.trimStart().startsWith('?')
}

function stripMarkers(goal: string): string {
  return goal.replace(/^\s*\?-?\s*/, '').replace(/\.\s*$/, '')
}

function verdict(result: QueryResult): string {
  if (result.outcome === 'error') return 'error'
  if (result.outcome === 'failure') return 'no'
  if (result.variables.length === 0) return 'yes'
  const n = result.solutions.length
  return `${n} solution${n === 1 ? '' : 's'}`
}

function formatDuration(seconds: number): string {
  if (seconds < 0.001) return `${(seconds * 1_000_000).toFixed(0)} µs`
  if (seconds < 1) return `${(seconds * 1000).toFixed(2)} ms`
  return `${seconds.toFixed(3)} s`
}
