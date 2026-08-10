package nz.pi.piview

import io.modelcontextprotocol.kotlin.sdk.server.StdioServerTransport
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.awaitCancellation
import kotlinx.coroutines.cancel
import kotlinx.coroutines.runBlocking
import kotlinx.io.asSink
import kotlinx.io.asSource
import kotlinx.io.buffered
import org.slf4j.LoggerFactory
import java.io.File
import java.io.FileDescriptor
import java.io.FileOutputStream
import java.io.PrintStream
import kotlin.system.exitProcess

private const val USAGE = """
piview - an MCP server and web view for a running PI prolog interpreter

  piview [options]

Options:
  --host <host>       host pi is listening on            (default 127.0.0.1)
  --port <n>          pi's --port                        (default 7071)
  --web-port <n>      port for the web view              (default 7070)
  --bind <addr>       interface for the web view         (default 127.0.0.1)
  --ui-dir <path>     serve the ui from this directory instead of the jar
  --samples <path>    directory of .pl files to offer in the ui
  --stdio             also speak MCP on stdin/stdout (for Claude Code and friends)
  --no-web            do not start the web view; MCP on stdio only
  --timeout <ms>      how long to wait for an answer      (default 60000)
  -h, --help          this

pi has to be started first, with its TCP port open:

  ./prolog --port 7071 samples/family.pl

Then:

  piview --samples samples          # web view on http://127.0.0.1:7070
  piview --stdio                    # MCP over stdio, plus the web view

The MCP endpoint is also served over http at /mcp on the web port.
"""

private class Options(args: Array<String>) {
    var host = "127.0.0.1"
    var port = 7071
    var webPort = 7070
    var bind = "127.0.0.1"
    var uiDir: File? = null
    var sampleDir: File? = null
    var stdio = false
    var web = true
    var timeoutMillis = 60_000

    init {
        var i = 0
        while (i < args.size) {
            val arg = args[i]
            fun next(): String {
                i++
                return args.getOrNull(i) ?: die("$arg needs a value")
            }
            when (arg) {
                "--host" -> host = next()
                "--port" -> port = next().toIntOrNull() ?: die("--port needs a number")
                "--web-port" -> webPort = next().toIntOrNull() ?: die("--web-port needs a number")
                "--bind" -> bind = next()
                "--ui-dir" -> uiDir = File(next())
                "--samples" -> sampleDir = File(next())
                "--timeout" -> timeoutMillis = next().toIntOrNull() ?: die("--timeout needs a number")
                "--stdio" -> stdio = true
                "--no-web" -> web = false
                "-h", "--help" -> {
                    println(USAGE.trim())
                    exitProcess(0)
                }
                else -> die("unknown option: $arg")
            }
            i++
        }
        if (!stdio && !web) die("--no-web without --stdio leaves nothing to do")
    }

    private fun die(message: String): Nothing {
        System.err.println("piview: $message")
        System.err.println(USAGE.trim())
        exitProcess(2)
    }
}

fun main(args: Array<String>): Unit = runBlocking {
    val options = Options(args)

    // In stdio mode stdout is the MCP transport and nothing else may touch it.
    // Logging is on stderr already, but libraries announce themselves on stdout
    // as they initialise, and one stray banner is a corrupt JSON-RPC stream - so
    // the real stdout is taken away and handed only to the transport.
    val mcpOut = if (options.stdio) {
        val real = PrintStream(FileOutputStream(FileDescriptor.out), true)
        System.setOut(PrintStream(FileOutputStream(FileDescriptor.err), true))
        real
    } else {
        null
    }

    val log = LoggerFactory.getLogger("piview")

    val scope = CoroutineScope(SupervisorJob() + coroutineContext)
    val client = PrologClient(
        host = options.host,
        port = options.port,
        readTimeoutMillis = options.timeoutMillis,
    )
    val session = PrologSession(client, scope)

    if (session.client.reachable()) {
        val snapshot = session.refreshDatabase()
        log.info(
            "attached to pi on {}:{} - {} clause(s)",
            options.host, options.port, snapshot.clauseCount,
        )
    } else {
        // not fatal: pi may be started after us, and the watcher will find it
        log.warn(
            "pi is not answering on {}:{} yet - start it with `./prolog --port {}` and piview will pick it up",
            options.host, options.port, options.port,
        )
    }
    session.startWatching()

    if (options.web) {
        WebServer(
            session,
            WebServer.Config(
                port = options.webPort,
                bind = options.bind,
                uiDir = options.uiDir,
                sampleDir = options.sampleDir,
                devOrigins = listOf("http://localhost:5173", "http://127.0.0.1:5173"),
            ),
        ).start()
    }

    if (options.stdio) {
        val transport = StdioServerTransport(
            input = System.`in`.asSource().buffered(),
            output = mcpOut!!.asSink().buffered(),
        )
        val server = McpTools.build(session)
        val mcpSession = server.createSession(transport)

        log.info("mcp server on stdio")
        val done = CompletableDeferred<Unit>()
        mcpSession.onClose { done.complete(Unit) }
        done.await()

        log.info("stdio closed, shutting down")
        // the database watcher and, if it is up, the web server are still
        // running in this scope; the client has gone, so stop rather than idle
        scope.cancel()
        exitProcess(0)
    } else {
        awaitCancellation()
    }
}
