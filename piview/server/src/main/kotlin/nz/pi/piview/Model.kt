package nz.pi.piview

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

/**
 * The shapes handed to both the MCP client and the browser.  They are the same
 * shapes on purpose: whatever the agent sees, the web view shows.
 */

@Serializable
enum class Outcome {
    /** the goal succeeded */
    @SerialName("success")
    SUCCESS,

    /** the goal ran and failed - prolog's "no" */
    @SerialName("failure")
    FAILURE,

    /** the goal never ran - a parse error, or the interpreter complained */
    @SerialName("error")
    ERROR,
}

/** one solution: the bindings of the query's variables */
@Serializable
data class Solution(val bindings: Map<String, String>)

/** the answer to a `?- ...` query */
@Serializable
data class QueryResult(
    val goal: String,
    val outcome: Outcome,
    /** variable names in the order pi reported them, the columns of a result table */
    val variables: List<String> = emptyList(),
    val solutions: List<Solution> = emptyList(),
    /** anything write/1 and nl/0 printed while the goal ran */
    val output: String = "",
    val executionSeconds: Double? = null,
    val error: String? = null,
    /** exactly what came back down the socket, for when the parse is not enough */
    val raw: String = "",
) {
    val solutionCount: Int get() = solutions.size
}

/** one clause in the database, as `list` numbers it */
@Serializable
data class Clause(
    val index: Int,
    val text: String,
    /** the head's predicate indicator, e.g. `father/2` */
    val predicate: String,
    val name: String,
    val arity: Int,
    /** true when the clause has a body - `head :- body` rather than a bare fact */
    val isRule: Boolean,
)

/** the database grouped the way you read it: by predicate */
@Serializable
data class Predicate(
    val name: String,
    val arity: Int,
    val indicator: String,
    val clauses: List<Clause>,
) {
    val factCount: Int get() = clauses.count { !it.isRule }
    val ruleCount: Int get() = clauses.count { it.isRule }
}

@Serializable
data class DatabaseSnapshot(
    val clauses: List<Clause> = emptyList(),
    val predicates: List<Predicate> = emptyList(),
    /** changes whenever the listing does - the watcher's cheap dirty check */
    val revision: String = "",
    val error: String? = null,
) {
    val clauseCount: Int get() = clauses.size
}

/** the result of anything that is not a query: assert, delete, load, new */
@Serializable
data class CommandResult(
    val command: String,
    val ok: Boolean,
    val message: String,
    val raw: String = "",
)

@Serializable
data class ConnectionStatus(
    val host: String,
    val port: Int,
    val connected: Boolean,
    val clauseCount: Int = 0,
    val detail: String? = null,
)

/** one line of the activity feed - who asked pi to do what, and how it went */
@Serializable
data class ActivityEntry(
    val id: Long,
    val at: Long,
    /** "mcp", "web" or "watcher" */
    val source: String,
    val kind: String,
    val command: String,
    val summary: String,
    val outcome: Outcome,
    val durationMillis: Long = 0,
)

// ---------------------------------------------------------------- websocket

@Serializable
sealed interface ServerEvent

@Serializable
@SerialName("status")
data class StatusEvent(val status: ConnectionStatus) : ServerEvent

@Serializable
@SerialName("database")
data class DatabaseEvent(val database: DatabaseSnapshot) : ServerEvent

@Serializable
@SerialName("activity")
data class ActivityEvent(val entry: ActivityEntry) : ServerEvent

@Serializable
@SerialName("activityHistory")
data class ActivityHistoryEvent(val entries: List<ActivityEntry>) : ServerEvent

// ---------------------------------------------------------------- rest bodies

@Serializable
data class QueryRequest(val goal: String)

@Serializable
data class AssertRequest(val clause: String)

@Serializable
data class DeleteRequest(val start: Int, val end: Int? = null)

@Serializable
data class LoadRequest(val path: String)

@Serializable
data class SampleFile(val name: String, val path: String, val source: String)
