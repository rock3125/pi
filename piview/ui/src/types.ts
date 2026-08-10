/** Mirrors the Kotlin serialisable model in server/src/main/kotlin/nz/pi/piview/Model.kt */

export type Outcome = 'success' | 'failure' | 'error'

export interface Solution {
  bindings: Record<string, string>
}

export interface QueryResult {
  goal: string
  outcome: Outcome
  variables: string[]
  solutions: Solution[]
  output: string
  executionSeconds?: number
  error?: string
  raw: string
}

export interface Clause {
  index: number
  text: string
  predicate: string
  name: string
  arity: number
  isRule: boolean
}

export interface Predicate {
  name: string
  arity: number
  indicator: string
  clauses: Clause[]
}

export interface DatabaseSnapshot {
  clauses: Clause[]
  predicates: Predicate[]
  revision: string
  error?: string
}

export interface CommandResult {
  command: string
  ok: boolean
  message: string
  raw: string
}

export interface ConnectionStatus {
  host: string
  port: number
  connected: boolean
  clauseCount: number
  detail?: string
}

export interface ActivityEntry {
  id: number
  at: number
  source: string
  kind: string
  command: string
  summary: string
  outcome: Outcome
  durationMillis: number
}

export interface SampleFile {
  name: string
  path: string
  source: string
}

export type ServerEvent =
  | { type: 'status'; status: ConnectionStatus }
  | { type: 'database'; database: DatabaseSnapshot }
  | { type: 'activity'; entry: ActivityEntry }
  | { type: 'activityHistory'; entries: ActivityEntry[] }

/** one block in the console transcript, before it is stamped and filed */
export type TranscriptBody =
  | { kind: 'query'; result: QueryResult }
  | { kind: 'pending'; goal: string }
  | { kind: 'assert'; source: string; results: CommandResult[] }
  | { kind: 'note'; tone: Outcome; title: string; body?: string }

export type TranscriptEntry = TranscriptBody & { id: number; at: number }
