import { useCallback, useEffect, useRef, useState } from 'react'
import { eventsUrl } from './api'
import type {
  ActivityEntry,
  ConnectionStatus,
  DatabaseSnapshot,
  ServerEvent,
} from './types'

const EMPTY_DATABASE: DatabaseSnapshot = { clauses: [], predicates: [], revision: '' }

const OFFLINE: ConnectionStatus = {
  host: '—',
  port: 0,
  connected: false,
  clauseCount: 0,
  detail: 'piview is not answering',
}

/**
 * One websocket to the piview server, holding the three things every panel
 * reads: whether pi is up, what it holds, and what has been done to it.
 *
 * The socket is the only source of truth here - the REST calls return the
 * answer to one command, but the database and the feed always arrive as pushes,
 * so a clause typed at pi's own prompt updates the view the same way one typed
 * here does.
 */
export function useSession() {
  const [status, setStatus] = useState<ConnectionStatus>(OFFLINE)
  const [database, setDatabase] = useState<DatabaseSnapshot>(EMPTY_DATABASE)
  const [activity, setActivity] = useState<ActivityEntry[]>([])
  const [socketOpen, setSocketOpen] = useState(false)

  const socketRef = useRef<WebSocket | null>(null)
  const retryRef = useRef<number | null>(null)
  const attemptsRef = useRef(0)

  const connect = useCallback(() => {
    if (socketRef.current) return

    let socket: WebSocket
    try {
      socket = new WebSocket(eventsUrl())
    } catch {
      scheduleRetry()
      return
    }
    socketRef.current = socket

    socket.onopen = () => {
      attemptsRef.current = 0
      setSocketOpen(true)
    }

    socket.onmessage = (message) => {
      let event: ServerEvent
      try {
        event = JSON.parse(message.data as string) as ServerEvent
      } catch {
        return
      }
      switch (event.type) {
        case 'status':
          setStatus(event.status)
          break
        case 'database':
          setDatabase(event.database)
          break
        case 'activity':
          setActivity((previous) => [...previous, event.entry].slice(-500))
          break
        case 'activityHistory':
          setActivity(event.entries)
          break
      }
    }

    socket.onclose = () => {
      socketRef.current = null
      setSocketOpen(false)
      setStatus((previous) => ({ ...previous, connected: false, detail: 'piview is not answering' }))
      scheduleRetry()
    }

    socket.onerror = () => socket.close()

    function scheduleRetry() {
      if (retryRef.current !== null) return
      // back off, but stay responsive enough that restarting the server feels
      // like a reconnect rather than a reload
      const delay = Math.min(500 * 2 ** attemptsRef.current, 8000)
      attemptsRef.current += 1
      retryRef.current = window.setTimeout(() => {
        retryRef.current = null
        connect()
      }, delay)
    }
  }, [])

  useEffect(() => {
    connect()
    return () => {
      if (retryRef.current !== null) window.clearTimeout(retryRef.current)
      const socket = socketRef.current
      socketRef.current = null
      socket?.close()
    }
  }, [connect])

  return { status, database, activity, socketOpen }
}
