package nz.pi.piview

import io.modelcontextprotocol.kotlin.sdk.server.Server
import io.modelcontextprotocol.kotlin.sdk.server.ServerOptions
import io.modelcontextprotocol.kotlin.sdk.types.CallToolRequest
import io.modelcontextprotocol.kotlin.sdk.types.CallToolResult
import io.modelcontextprotocol.kotlin.sdk.types.Implementation
import io.modelcontextprotocol.kotlin.sdk.types.ReadResourceResult
import io.modelcontextprotocol.kotlin.sdk.types.ServerCapabilities
import io.modelcontextprotocol.kotlin.sdk.types.TextContent
import io.modelcontextprotocol.kotlin.sdk.types.TextResourceContents
import io.modelcontextprotocol.kotlin.sdk.types.Tool
import io.modelcontextprotocol.kotlin.sdk.types.ToolAnnotations
import io.modelcontextprotocol.kotlin.sdk.types.ToolSchema
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.encodeToJsonElement
import kotlinx.serialization.json.put

/**
 * The MCP face of a running pi.
 *
 * Every tool returns two things: a text block laid out the way pi lays it out,
 * for a model to read, and `structuredContent` carrying the same answer parsed,
 * for anything that would rather have fields than prose.
 */
object McpTools {

    const val NAME = "piview"
    const val VERSION = "1.0.0"

    private val INSTRUCTIONS = """
        This server is attached to a running PI prolog interpreter over its TCP
        port. The database is shared: clauses you assert are visible at pi's own
        prompt and in the piview web view, and clauses typed there are visible to
        you.

        PI is not standard prolog. The differences that matter:
          - `=` both evaluates and unifies. There is no `is`: write `N = M + 1`.
          - There is no assert/1 or retract/1. Use the prolog_assert tool to add a
            clause and prolog_delete to remove one by its listing number.
          - Comparison is `==`, `!=`, `<`, `<=`, `>`, `>=`.
          - Built-ins are write/1 and nl/0 only.
          - Control: `,` `;` `:-` `!` `not(...)` `fail`.
          - Lists are `[a,b,c]`, `[]` and `[H|T]`.
          - A goal is solved to its complete set of solutions in one pass, so a
            query with very many solutions holds them all at once.

        Start with prolog_status or prolog_list to see what is loaded.
    """.trimIndent()

    fun build(session: PrologSession): Server {
        val server = Server(
            serverInfo = Implementation(name = NAME, version = VERSION, title = "PI Prolog"),
            options = ServerOptions(
                capabilities = ServerCapabilities(
                    tools = ServerCapabilities.Tools(listChanged = false),
                    resources = ServerCapabilities.Resources(listChanged = false, subscribe = false),
                ),
            ),
            instructions = INSTRUCTIONS,
        )

        registerTools(server, session)
        registerResources(server, session)
        return server
    }

    // ------------------------------------------------------------ tools

    private fun registerTools(server: Server, session: PrologSession) {

        server.addTool(
            name = "prolog_query",
            title = "Run a prolog query",
            description = """
                Run a goal against the running pi database and get back every
                solution. The `?-` prefix and the trailing `.` are added if you
                leave them off, so `father(fred,X)` and `?- father(fred,X).` are
                the same request. Remember pi uses `=` where standard prolog uses
                `is`. Output written by write/1 during the goal is returned
                separately from the bindings.
            """.trimIndent(),
            inputSchema = schema(
                properties = mapOf(
                    "goal" to stringProp("The goal to solve, e.g. `father(fred,X)` or `X = 3 - 1`"),
                ),
                required = listOf("goal"),
            ),
            toolAnnotations = ToolAnnotations(
                readOnlyHint = true,
                destructiveHint = false,
                idempotentHint = true,
                openWorldHint = false,
            ),
        ) { request ->
            val goal = request.stringArg("goal") ?: return@addTool fail("goal is required")
            val result = session.query(goal, source = "mcp")
            CallToolResult(
                content = listOf(TextContent(renderQuery(result))),
                structuredContent = Json.encodeToJsonElement(result) as JsonObject,
                isError = result.outcome == Outcome.ERROR,
            )
        }

        server.addTool(
            name = "prolog_assert",
            title = "Add clauses to the database",
            description = """
                Add one or more facts or rules to the running database. Several
                clauses can be sent at once - a whole program can be pasted in,
                comments and line breaks included, and each statement is added in
                order. This is the only way to change the database from a program:
                pi has no assert/1.
            """.trimIndent(),
            inputSchema = schema(
                properties = mapOf(
                    "clauses" to stringProp(
                        "Prolog source: `likes(pete,prolog).` or a whole program with rules and comments",
                    ),
                ),
                required = listOf("clauses"),
            ),
            toolAnnotations = ToolAnnotations(
                readOnlyHint = false,
                destructiveHint = false,
                idempotentHint = false,
                openWorldHint = false,
            ),
        ) { request ->
            val source = request.stringArg("clauses") ?: return@addTool fail("clauses is required")
            val results = session.consult(source, origin = "mcp")
            val failures = results.filter { !it.ok }

            val text = buildString {
                appendLine("added ${results.size - failures.size} of ${results.size} statement(s)")
                results.forEach { appendLine("  ${if (it.ok) "ok  " else "fail"}  ${it.command}") }
                failures.forEach { appendLine(it.message) }
                append("database now holds ${session.database.clauseCount} clause(s)")
            }
            CallToolResult(
                content = listOf(TextContent(text)),
                structuredContent = buildJsonObject {
                    put("added", results.size - failures.size)
                    put("failed", failures.size)
                    put("clauseCount", session.database.clauseCount)
                    put("results", Json.encodeToJsonElement(results))
                },
                isError = failures.isNotEmpty(),
            )
        }

        server.addTool(
            name = "prolog_list",
            title = "List the database",
            description = """
                List the clauses currently in the database, with the numbers that
                prolog_delete takes, grouped by predicate. Optionally narrow it to
                a range of those numbers, or to a single predicate name.
            """.trimIndent(),
            inputSchema = schema(
                properties = mapOf(
                    "start" to intProp("First clause number to show"),
                    "end" to intProp("Last clause number to show"),
                    "predicate" to stringProp("Show only clauses whose head has this name, e.g. `father`"),
                ),
            ),
            toolAnnotations = ToolAnnotations(readOnlyHint = true, destructiveHint = false, idempotentHint = true),
        ) { request ->
            val snapshot = session.listing()
            val start = request.intArg("start")
            val end = request.intArg("end")
            val predicate = request.stringArg("predicate")

            var clauses = snapshot.clauses
            if (start != null) clauses = clauses.filter { it.index >= start }
            if (end != null) clauses = clauses.filter { it.index <= end }
            if (predicate != null) clauses = clauses.filter { it.name == predicate }

            val text = if (clauses.isEmpty()) {
                if (snapshot.clauses.isEmpty()) "database empty" else "no clauses matched"
            } else {
                clauses.groupBy { it.predicate }.entries.joinToString("\n\n") { (indicator, group) ->
                    buildString {
                        appendLine("$indicator  (${group.size} clause${if (group.size == 1) "" else "s"})")
                        group.forEach { append("  %05d  %s.%n".format(it.index, it.text)) }
                    }.trimEnd()
                }
            }

            CallToolResult(
                content = listOf(TextContent(text)),
                structuredContent = Json.encodeToJsonElement(snapshot.copy(clauses = clauses)) as JsonObject,
            )
        }

        server.addTool(
            name = "prolog_delete",
            title = "Delete clauses by number",
            description = """
                Remove clauses from the database by the numbers `prolog_list`
                shows. Numbers shift down once a clause is gone, so list again
                before deleting a second time.
            """.trimIndent(),
            inputSchema = schema(
                properties = mapOf(
                    "start" to intProp("First clause number to remove"),
                    "end" to intProp("Last clause number to remove; omit to remove just one"),
                ),
                required = listOf("start"),
            ),
            toolAnnotations = ToolAnnotations(
                readOnlyHint = false,
                destructiveHint = true,
                idempotentHint = false,
                openWorldHint = false,
            ),
        ) { request ->
            val start = request.intArg("start") ?: return@addTool fail("start is required")
            val result = session.delete(start, request.intArg("end"), origin = "mcp")
            CallToolResult(
                content = listOf(
                    TextContent(
                        "${result.message.ifBlank { "deleted" }}\n" +
                            "database now holds ${session.database.clauseCount} clause(s)",
                    ),
                ),
                structuredContent = Json.encodeToJsonElement(result) as JsonObject,
                isError = !result.ok,
            )
        }

        server.addTool(
            name = "prolog_load",
            title = "Load a prolog file",
            description = """
                Have pi read a `.pl` file from its own filesystem and add it to the
                current database. The path is resolved by the pi process, not by
                this server. Nothing is cleared first - use prolog_clear for that.
            """.trimIndent(),
            inputSchema = schema(
                properties = mapOf("path" to stringProp("Path to a prolog file, e.g. `samples/family.pl`")),
                required = listOf("path"),
            ),
            toolAnnotations = ToolAnnotations(readOnlyHint = false, destructiveHint = false, openWorldHint = true),
        ) { request ->
            val path = request.stringArg("path") ?: return@addTool fail("path is required")
            val result = session.load(path, origin = "mcp")
            CallToolResult(
                content = listOf(
                    TextContent(
                        "${result.message}\ndatabase now holds ${session.database.clauseCount} clause(s)",
                    ),
                ),
                structuredContent = Json.encodeToJsonElement(result) as JsonObject,
                isError = !result.ok,
            )
        }

        server.addTool(
            name = "prolog_clear",
            title = "Clear the database",
            description = """
                Throw away every clause in the database. This affects pi's own
                prompt and every other client too, and cannot be undone - ask
                before using it on a database you did not load yourself.
            """.trimIndent(),
            inputSchema = schema(
                properties = mapOf(
                    "confirm" to boolProp("Must be true. Guards against clearing a database by accident."),
                ),
                required = listOf("confirm"),
            ),
            toolAnnotations = ToolAnnotations(
                readOnlyHint = false,
                destructiveHint = true,
                idempotentHint = true,
                openWorldHint = false,
            ),
        ) { request ->
            if (!request.boolArg("confirm")) {
                return@addTool fail("refusing to clear the database without confirm=true")
            }
            val result = session.clear(origin = "mcp")
            CallToolResult(
                content = listOf(TextContent(result.message)),
                structuredContent = Json.encodeToJsonElement(result) as JsonObject,
                isError = !result.ok,
            )
        }

        server.addTool(
            name = "prolog_status",
            title = "Interpreter status",
            description = """
                Is pi answering, on which host and port, and what does it hold?
                Returns the connection state, the clause count and a summary of
                every predicate in the database. A good first call.
            """.trimIndent(),
            inputSchema = schema(),
            toolAnnotations = ToolAnnotations(readOnlyHint = true, destructiveHint = false, idempotentHint = true),
        ) { _ ->
            val snapshot = session.listing()
            val status = session.status()
            val text = buildString {
                appendLine("pi at ${status.host}:${status.port} - ${if (status.connected) "connected" else "not answering"}")
                status.detail?.let { appendLine(it) }
                appendLine("${snapshot.clauseCount} clause(s) in ${snapshot.predicates.size} predicate(s)")
                snapshot.predicates.forEach {
                    appendLine("  ${it.indicator}  ${it.factCount} fact(s), ${it.ruleCount} rule(s)")
                }
            }.trimEnd()
            CallToolResult(
                content = listOf(TextContent(text)),
                structuredContent = buildJsonObject {
                    put("status", Json.encodeToJsonElement(status))
                    put("predicates", Json.encodeToJsonElement(snapshot.predicates))
                },
                isError = !status.connected,
            )
        }

        server.addTool(
            name = "prolog_command",
            title = "Send a raw interpreter command",
            description = """
                Send one line exactly as it would be typed at pi's `>` prompt, for
                the few commands the other tools do not cover (`tron`, `troff`,
                `help`). Queries and clauses are better sent through prolog_query
                and prolog_assert, which parse the answer.
            """.trimIndent(),
            inputSchema = schema(
                properties = mapOf("command" to stringProp("A single interpreter command line")),
                required = listOf("command"),
            ),
            toolAnnotations = ToolAnnotations(readOnlyHint = false, destructiveHint = true, openWorldHint = true),
        ) { request ->
            val command = request.stringArg("command") ?: return@addTool fail("command is required")
            if (command.trim().lowercase() in setOf("exit", "quit", "bye")) {
                return@addTool fail("that would close this server's connection to pi; it is not sent")
            }
            val result = session.raw(command, origin = "mcp")
            CallToolResult(
                content = listOf(TextContent(result.message)),
                structuredContent = Json.encodeToJsonElement(result) as JsonObject,
                isError = !result.ok,
            )
        }
    }

    // ------------------------------------------------------------ resources

    private fun registerResources(server: Server, session: PrologSession) {
        server.addResource(
            uri = "prolog://database",
            name = "The pi database",
            description = "Every clause currently loaded, as pi's `list` prints it",
            mimeType = "text/x-prolog",
        ) { request ->
            val snapshot = session.listing()
            val text = if (snapshot.clauses.isEmpty()) {
                "% database empty"
            } else {
                snapshot.clauses.joinToString("\n") { "${it.text}." }
            }
            ReadResourceResult(
                contents = listOf(TextResourceContents(text, request.params.uri, "text/x-prolog")),
            )
        }

        server.addResource(
            uri = "prolog://activity",
            name = "Recent interpreter activity",
            description = "The commands piview has sent to pi, newest last",
            mimeType = "text/plain",
        ) { request ->
            val text = session.activity().joinToString("\n") {
                "${it.source.padEnd(7)} ${it.kind.padEnd(7)} ${it.command}  -> ${it.summary}"
            }
            ReadResourceResult(
                contents = listOf(
                    TextResourceContents(text.ifEmpty { "nothing yet" }, request.params.uri, "text/plain"),
                ),
            )
        }
    }

    // ------------------------------------------------------------ rendering

    /** the answer as pi would have printed it, with a little more shape */
    fun renderQuery(result: QueryResult): String = buildString {
        appendLine(result.goal)
        if (result.output.isNotEmpty()) {
            appendLine(result.output)
        }
        when (result.outcome) {
            Outcome.ERROR -> appendLine(result.error ?: "error")
            Outcome.FAILURE -> appendLine("no")
            Outcome.SUCCESS -> {
                if (result.variables.isEmpty()) {
                    appendLine("yes")
                } else {
                    result.solutions.forEach { solution ->
                        appendLine(result.variables.joinToString("  ") { v -> "$v=${solution.bindings[v] ?: "_"}" })
                    }
                    appendLine("${result.solutionCount} solution${if (result.solutionCount == 1) "" else "s"}")
                }
            }
        }
        result.executionSeconds?.let { append("(%.8f seconds)".format(it)) }
    }.trimEnd()

    // ------------------------------------------------------------ schema helpers

    private fun schema(
        properties: Map<String, JsonObject> = emptyMap(),
        required: List<String> = emptyList(),
    ) = ToolSchema(
        properties = JsonObject(properties),
        required = required.ifEmpty { null },
    )

    private fun stringProp(description: String) = buildJsonObject {
        put("type", "string")
        put("description", description)
    }

    private fun intProp(description: String) = buildJsonObject {
        put("type", "integer")
        put("description", description)
    }

    private fun boolProp(description: String) = buildJsonObject {
        put("type", "boolean")
        put("description", description)
    }

    private fun fail(message: String) = CallToolResult(
        content = listOf(TextContent(message)),
        isError = true,
    )
}

// ---------------------------------------------------------------- argument access

private fun CallToolRequest.stringArg(name: String): String? =
    (arguments?.get(name) as? JsonPrimitive)?.content?.takeIf { it.isNotBlank() }

private fun CallToolRequest.intArg(name: String): Int? =
    (arguments?.get(name) as? JsonPrimitive)?.content?.toIntOrNull()

private fun CallToolRequest.boolArg(name: String): Boolean =
    (arguments?.get(name) as? JsonPrimitive)?.content?.equals("true", ignoreCase = true) == true
