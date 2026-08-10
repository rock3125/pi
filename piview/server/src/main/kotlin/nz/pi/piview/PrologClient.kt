package nz.pi.piview

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import java.io.IOException
import java.net.InetSocketAddress
import java.net.Socket
import java.net.SocketTimeoutException

/**
 * Talks to a running `./prolog --port <n>`.
 *
 * The wire protocol is one command per line and the answer is free text with no
 * terminator, so there is no way to tell where one answer stops and the next
 * begins on a shared connection.  pi closes its side when the client half-closes,
 * which does give a frame - so every command gets its own short-lived connection:
 * send, shutdown the write side, read to EOF.  That is exactly what pi's own
 * test suite does, and it costs a socket per command against a loopback server.
 *
 * Nothing is lost by not holding a connection open: pi keeps one database shared
 * by every client and by its own prompt, so there is no per-connection session.
 */
class PrologClient(
    val host: String,
    val port: Int,
    private val connectTimeoutMillis: Int = 3_000,
    private val readTimeoutMillis: Int = 60_000,
) {
    /**
     * pi runs commands one at a time behind a global lock anyway.  Serialising
     * here as well keeps the activity feed in the order things really happened.
     */
    private val gate = Mutex()

    class PrologUnreachable(host: String, port: Int, cause: Throwable) :
        IOException("pi is not answering on $host:$port (${cause.message})", cause)

    /** send one command line, return everything pi wrote back */
    suspend fun send(command: String): String = gate.withLock { sendUnlocked(command) }

    /** send several commands, each on its own connection, and return each answer */
    suspend fun sendAll(commands: List<String>): List<String> = gate.withLock {
        commands.map { sendUnlocked(it) }
    }

    private suspend fun sendUnlocked(command: String): String = withContext(Dispatchers.IO) {
        val line = command.trim()
        require(!line.contains('\n')) { "a command is a single line" }

        try {
            Socket().use { socket ->
                socket.tcpNoDelay = true
                socket.connect(InetSocketAddress(host, port), connectTimeoutMillis)
                socket.soTimeout = readTimeoutMillis

                socket.getOutputStream().apply {
                    write((line + "\n").toByteArray(Charsets.UTF_8))
                    flush()
                }
                socket.shutdownOutput()

                val answer = StringBuilder()
                val buffer = ByteArray(16 * 1024)
                val input = socket.getInputStream()
                while (true) {
                    val n = try {
                        input.read(buffer)
                    } catch (_: SocketTimeoutException) {
                        // a query that never comes back still tells us what it
                        // managed to print before we gave up on it
                        answer.append("\n(piview: no answer within ${readTimeoutMillis}ms)\n")
                        break
                    }
                    if (n <= 0) break
                    answer.append(String(buffer, 0, n, Charsets.UTF_8))
                }
                answer.toString()
            }
        } catch (e: SocketTimeoutException) {
            throw PrologUnreachable(host, port, e)
        } catch (e: IOException) {
            throw PrologUnreachable(host, port, e)
        }
    }

    /** cheap liveness check - open a connection and drop it */
    suspend fun reachable(): Boolean = withContext(Dispatchers.IO) {
        try {
            Socket().use { it.connect(InetSocketAddress(host, port), connectTimeoutMillis) }
            true
        } catch (_: IOException) {
            false
        }
    }
}
