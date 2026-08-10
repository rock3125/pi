import { useLayoutEffect, useRef } from 'react'
import type { ActivityEntry } from '../types'
import { PrologText } from './PrologText'

/**
 * Who touched the interpreter, and what came back.
 *
 * The point of this panel is the `source` column: `mcp` is an agent driving the
 * same database you are looking at, `web` is this page. Watching an agent solve
 * something is the one thing a terminal beside pi cannot show you.
 */
export function ActivityFeed({ entries }: { entries: ActivityEntry[] }) {
  const tailRef = useRef<HTMLDivElement>(null)

  useLayoutEffect(() => {
    tailRef.current?.scrollIntoView({ block: 'end' })
  }, [entries.length])

  if (entries.length === 0) {
    return (
      <div className="empty">
        <p className="empty__title">Quiet so far.</p>
        <p className="empty__hint">
          Every command piview sends to pi lands here, whether it came from this page or from an
          agent over MCP. Run a goal and watch it arrive.
        </p>
      </div>
    )
  }

  return (
    <div className="panel__body scroller">
      {entries.map((entry) => (
        <article className="feed__item" key={entry.id}>
          <span className="feed__source" data-source={entry.source} title={sourceTitle(entry.source)}>
            {entry.source}
          </span>
          <span className="feed__command">
            <PrologText source={entry.command} />
          </span>
          <span className="feed__summary" data-outcome={entry.outcome}>
            {entry.summary}
          </span>
          <span className="feed__time">
            {clock(entry.at)} · {entry.durationMillis} ms
          </span>
        </article>
      ))}
      <div ref={tailRef} />
    </div>
  )
}

function sourceTitle(source: string): string {
  if (source === 'mcp') return 'an agent, over the MCP connection'
  if (source === 'web') return 'this page'
  return source
}

function clock(at: number): string {
  return new Date(at).toLocaleTimeString(undefined, {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false,
  })
}
