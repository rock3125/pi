package nz.pi.piview

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.slf4j.LoggerFactory
import java.util.concurrent.atomic.AtomicLong

/**
 * Everything the MCP tools and the web API both need, in one place.
 *
 * It owns the connection to pi, keeps the last database listing, and publishes
 * an event whenever either changes.  The point of the watcher is that pi's
 * database is shared - a clause typed at pi's own prompt, or asserted by a
 * different client, has to show up in the browser too, and the only way to
 * notice that is to keep asking.
 */
class PrologSession(
    val client: PrologClient,
    private val scope: CoroutineScope,
    private val watchIntervalMillis: Long = 1_500,
    private val activityLimit: Int = 500,
) {
    private val log = LoggerFactory.getLogger(PrologSession::class.java)

    private val events = MutableSharedFlow<ServerEvent>(replay = 0, extraBufferCapacity = 256)
    val eventFlow: SharedFlow<ServerEvent> = events.asSharedFlow()

    private val activityIds = AtomicLong(0)
    private val activityLock = Mutex()
    private val activityLog = ArrayDeque<ActivityEntry>()

    @Volatile
    var database: DatabaseSnapshot = DatabaseSnapshot()
        private set

    @Volatile
    var connected: Boolean = false
        private set

    @Volatile
    var lastError: String? = null
        private set

    // ------------------------------------------------------------ commands

    suspend fun query(goal: String, source: String = "web"): QueryResult {
        val command = Parser.normaliseGoal(goal)
        val started = System.currentTimeMillis()
        val result = try {
            Parser.parseQuery(command, exchange(command))
        } catch (e: PrologClient.PrologUnreachable) {
            QueryResult(command, Outcome.ERROR, error = e.message, raw = "")
        }

        record(
            source = source,
            kind = "query",
            command = command,
            outcome = result.outcome,
            durationMillis = System.currentTimeMillis() - started,
            summary = when (result.outcome) {
                Outcome.ERROR -> result.error?.lineSequence()?.firstOrNull() ?: "error"
                Outcome.FAILURE -> "no"
                Outcome.SUCCESS ->
                    if (result.variables.isEmpty()) "yes"
                    else "${result.solutionCount} solution${if (result.solutionCount == 1) "" else "s"}"
            },
        )
        return result
    }

    /** add one or more clauses; a whole pasted program is fine */
    suspend fun consult(source: String, origin: String = "web"): List<CommandResult> {
        val statements = Parser.splitProgram(source)
        if (statements.isEmpty()) {
            return listOf(CommandResult(source, false, "nothing to add"))
        }

        val results = statements.map { statement ->
            val started = System.currentTimeMillis()
            val result = try {
                Parser.parseCommand(statement, exchange(statement))
            } catch (e: PrologClient.PrologUnreachable) {
                CommandResult(statement, false, e.message ?: "pi is not answering")
            }
            record(
                source = origin,
                kind = if (Parser.isQuery(statement)) "query" else "assert",
                command = statement,
                outcome = if (result.ok) Outcome.SUCCESS else Outcome.ERROR,
                durationMillis = System.currentTimeMillis() - started,
                summary = if (result.ok) "added" else result.message.lineSequence().first(),
            )
            result
        }

        refreshDatabase()
        return results
    }

    suspend fun listing(refresh: Boolean = true): DatabaseSnapshot {
        if (refresh) refreshDatabase()
        return database
    }

    suspend fun listingRange(start: Int, end: Int?): CommandResult =
        simpleCommand("list " + if (end != null) "$start-$end" else "$start", "list", "web")

    suspend fun delete(start: Int, end: Int?, origin: String = "web"): CommandResult {
        val command = "delete " + if (end != null && end != start) "$start-$end" else "$start"
        val result = simpleCommand(command, "delete", origin)
        refreshDatabase()
        return result
    }

    suspend fun load(path: String, origin: String = "web"): CommandResult {
        val result = simpleCommand("load ${path.trim()}", "load", origin)
        refreshDatabase()
        return result
    }

    suspend fun clear(origin: String = "web"): CommandResult {
        val result = simpleCommand("new", "clear", origin)
        refreshDatabase()
        return result
    }

    suspend fun help(): CommandResult = simpleCommand("help", "help", "web")

    /** run a raw command line exactly as it would be typed at pi's prompt */
    suspend fun raw(command: String, origin: String = "web"): CommandResult {
        val result = simpleCommand(command, "raw", origin)
        refreshDatabase()
        return result
    }

    private suspend fun simpleCommand(command: String, kind: String, origin: String): CommandResult {
        val started = System.currentTimeMillis()
        val result = try {
            Parser.parseCommand(command, exchange(command))
        } catch (e: PrologClient.PrologUnreachable) {
            CommandResult(command, false, e.message ?: "pi is not answering")
        }
        record(
            source = origin,
            kind = kind,
            command = command,
            outcome = if (result.ok) Outcome.SUCCESS else Outcome.ERROR,
            durationMillis = System.currentTimeMillis() - started,
            summary = result.message.lineSequence().firstOrNull()?.take(120) ?: "ok",
        )
        return result
    }

    // ------------------------------------------------------------ plumbing

    private suspend fun exchange(command: String): String = try {
        val answer = client.send(command)
        markConnected(true, null)
        answer
    } catch (e: PrologClient.PrologUnreachable) {
        markConnected(false, e.message)
        throw e
    }

    private fun markConnected(now: Boolean, detail: String?) {
        val changed = connected != now || lastError != detail
        connected = now
        lastError = detail
        if (changed) emit(StatusEvent(status()))
    }

    fun status(): ConnectionStatus = ConnectionStatus(
        host = client.host,
        port = client.port,
        connected = connected,
        clauseCount = database.clauseCount,
        detail = lastError,
    )

    suspend fun refreshDatabase(): DatabaseSnapshot {
        val snapshot = try {
            val parsed = Parser.parseListing(client.send("list"))
            markConnected(true, null)
            parsed
        } catch (e: PrologClient.PrologUnreachable) {
            markConnected(false, e.message)
            DatabaseSnapshot(error = e.message, revision = "unreachable")
        }

        if (snapshot.revision != database.revision) {
            database = snapshot
            emit(DatabaseEvent(snapshot))
            emit(StatusEvent(status()))
        }
        return snapshot
    }

    /** keep asking, so changes made outside piview still reach the browser */
    fun startWatching() {
        scope.launch {
            while (isActive) {
                try {
                    refreshDatabase()
                } catch (e: Exception) {
                    log.debug("watcher poll failed", e)
                }
                delay(watchIntervalMillis)
            }
        }
    }

    // ------------------------------------------------------------ activity

    private fun record(
        source: String,
        kind: String,
        command: String,
        outcome: Outcome,
        durationMillis: Long,
        summary: String,
    ) {
        val entry = ActivityEntry(
            id = activityIds.incrementAndGet(),
            at = System.currentTimeMillis(),
            source = source,
            kind = kind,
            command = command,
            summary = summary,
            outcome = outcome,
            durationMillis = durationMillis,
        )
        scope.launch {
            activityLock.withLock {
                activityLog.addLast(entry)
                while (activityLog.size > activityLimit) activityLog.removeFirst()
            }
        }
        emit(ActivityEvent(entry))
    }

    suspend fun activity(): List<ActivityEntry> = activityLock.withLock { activityLog.toList() }

    private fun emit(event: ServerEvent) {
        if (!events.tryEmit(event)) {
            scope.launch { events.emit(event) }
        }
    }
}
