import type {
  CommandResult,
  ConnectionStatus,
  DatabaseSnapshot,
  QueryResult,
  SampleFile,
} from './types'

async function send<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, {
    ...init,
    headers: { 'Content-Type': 'application/json', ...(init?.headers ?? {}) },
  })
  if (!response.ok) {
    throw new Error(`${init?.method ?? 'GET'} ${path} answered ${response.status}`)
  }
  return (await response.json()) as T
}

export const api = {
  status: () => send<ConnectionStatus>('/api/status'),
  database: () => send<DatabaseSnapshot>('/api/database'),
  samples: () => send<SampleFile[]>('/api/samples'),

  query: (goal: string) =>
    send<QueryResult>('/api/query', { method: 'POST', body: JSON.stringify({ goal }) }),

  assert: (clause: string) =>
    send<CommandResult[]>('/api/assert', { method: 'POST', body: JSON.stringify({ clause }) }),

  remove: (start: number, end?: number) =>
    send<CommandResult>('/api/delete', { method: 'POST', body: JSON.stringify({ start, end }) }),

  load: (path: string) =>
    send<CommandResult>('/api/load', { method: 'POST', body: JSON.stringify({ path }) }),

  clear: () => send<CommandResult>('/api/clear', { method: 'POST' }),
}

/** the websocket url for this page, dev-server proxy included */
export function eventsUrl(): string {
  const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:'
  return `${protocol}//${location.host}/api/events`
}
