package nz.pi.piview

import io.ktor.http.ContentType
import io.ktor.http.HttpHeaders
import io.ktor.http.HttpMethod
import io.ktor.http.HttpStatusCode
import io.ktor.serialization.kotlinx.KotlinxWebsocketSerializationConverter
import io.ktor.server.application.Application
import io.ktor.server.application.install
import io.ktor.server.cio.CIO
import io.ktor.server.engine.embeddedServer
import io.ktor.server.http.content.staticFiles
import io.ktor.server.http.content.staticResources
import io.ktor.server.plugins.cors.routing.CORS
import io.ktor.server.request.receiveText
import io.ktor.server.request.uri
import io.ktor.server.response.respondText
import io.ktor.server.routing.RoutingCall
import io.ktor.server.routing.get
import io.ktor.server.routing.post
import io.ktor.server.routing.route
import io.ktor.server.routing.routing
import io.ktor.server.websocket.WebSockets
import io.ktor.server.websocket.pingPeriod
import io.ktor.server.websocket.webSocket
import io.ktor.websocket.Frame
import io.modelcontextprotocol.kotlin.sdk.server.mcpStreamableHttp
import kotlinx.coroutines.channels.ClosedSendChannelException
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import kotlinx.serialization.encodeToString
import org.slf4j.LoggerFactory
import java.io.File
import kotlin.time.Duration.Companion.seconds

/**
 * The web half: a small JSON API over the same [PrologSession] the MCP tools
 * use, a websocket that pushes the database and the activity feed as they
 * change, the built React bundle, and the MCP streamable-HTTP endpoint for
 * clients that would rather connect over http than over a pipe.
 */
class WebServer(
    private val session: PrologSession,
    private val config: Config,
) {
    data class Config(
        val port: Int,
        val bind: String,
        val uiDir: File?,
        val sampleDir: File?,
        val allowedOrigins: List<String>,
    )

    private val log = LoggerFactory.getLogger(WebServer::class.java)

    fun start() {
        embeddedServer(CIO, port = config.port, host = config.bind) { module() }
            .start(wait = false)
        log.info("piview web view on http://{}:{}", config.bind, config.port)
        log.info("mcp streamable http on http://{}:{}/mcp", config.bind, config.port)
    }

    private fun Application.module() {
        // NOTE: no ContentNegotiation here on purpose.  The MCP SDK installs its
        // own, configured with settings this API does not want (the class
        // discriminator is off, which would flatten ServerEvent).  Responses are
        // serialised explicitly with piview's Json instead.
        // The transport refuses a Host header it does not recognise, which is
        // its DNS-rebinding defence. A reverse proxy passes the browser's Host
        // through verbatim, so every origin we accept has to be a host we
        // accept as well — "https://pi.example.com" arrives as Host
        // "pi.example.com". Deriving one list from the other keeps a caller
        // from having to name the same place twice.
        val originHosts = config.allowedOrigins.map { it.substringAfter("://") }

        mcpStreamableHttp(
            path = "/mcp",
            // the browser and pi are both local; the UI is served from this same
            // origin, and the vite dev server needs to be let in during development
            allowedHosts = listOf("localhost", "127.0.0.1", "[::1]", "${config.bind}:${config.port}") +
                originHosts,
            allowedOrigins = config.allowedOrigins.ifEmpty { null },
        ) {
            McpTools.build(session)
        }

        install(WebSockets) {
            pingPeriod = 20.seconds
            contentConverter = KotlinxWebsocketSerializationConverter(Json)
        }

        install(CORS) {
            config.allowedOrigins.forEach { origin ->
                val withoutScheme = origin.substringAfter("://")
                allowHost(withoutScheme, schemes = listOf(origin.substringBefore("://")))
            }
            allowMethod(HttpMethod.Get)
            allowMethod(HttpMethod.Post)
            allowMethod(HttpMethod.Delete)
            allowHeader(HttpHeaders.ContentType)
        }

        routing {
            route("/api") {
                get("/status") {
                    session.refreshDatabase()
                    call.json(session.status())
                }

                get("/database") {
                    call.json(session.listing())
                }

                get("/activity") {
                    call.json(session.activity())
                }

                get("/samples") {
                    call.json(samples())
                }

                post("/query") {
                    val request = call.body<QueryRequest>() ?: return@post
                    call.json(session.query(request.goal, source = "web"))
                }

                post("/assert") {
                    val request = call.body<AssertRequest>() ?: return@post
                    call.json(session.consult(request.clause, origin = "web"))
                }

                post("/delete") {
                    val request = call.body<DeleteRequest>() ?: return@post
                    call.json(session.delete(request.start, request.end, origin = "web"))
                }

                post("/load") {
                    val request = call.body<LoadRequest>() ?: return@post
                    call.json(session.load(request.path, origin = "web"))
                }

                post("/clear") {
                    call.json(session.clear(origin = "web"))
                }

                get("/help") {
                    call.json(session.help())
                }

                webSocket("/events") {
                    // catch a new viewer up before streaming changes at it
                    sendEvent(StatusEvent(session.status()))
                    sendEvent(DatabaseEvent(session.listing(refresh = false)))
                    sendEvent(ActivityHistoryEvent(session.activity()))

                    val pump = launch {
                        session.eventFlow.collect { event -> sendEvent(event) }
                    }
                    try {
                        for (frame in incoming) {
                            // the browser only listens; anything it sends is a keepalive
                        }
                    } finally {
                        pump.cancel()
                    }
                }
            }

            // the UI: from a directory while developing, from the jar otherwise
            val uiDir = config.uiDir
            if (uiDir != null && uiDir.isDirectory) {
                log.info("serving the ui from {}", uiDir.absolutePath)
                staticFiles("/", uiDir) { default("index.html") }
            } else {
                staticResources("/", "web") { default("index.html") }
            }
        }
    }

    // ------------------------------------------------------------ helpers

    private suspend fun io.ktor.server.websocket.DefaultWebSocketServerSession.sendEvent(event: ServerEvent) {
        try {
            send(Frame.Text(Json.encodeToString<ServerEvent>(event)))
        } catch (_: ClosedSendChannelException) {
            // the viewer went away mid-send; the collector is cancelled by the
            // finally block above
        }
    }

    private suspend inline fun <reified T> RoutingCall.body(): T? = try {
        Json.decodeFromString<T>(receiveText())
    } catch (e: Exception) {
        respondText(
            Json.encodeToString(mapOf("error" to "could not read ${request.uri}: ${e.message}")),
            ContentType.Application.Json,
            HttpStatusCode.BadRequest,
        )
        null
    }

    private suspend inline fun <reified T> RoutingCall.json(value: T) {
        respondText(Json.encodeToString(value), ContentType.Application.Json, HttpStatusCode.OK)
    }

    private fun samples(): List<SampleFile> {
        val dir = config.sampleDir ?: return emptyList()
        if (!dir.isDirectory) return emptyList()
        return dir.listFiles { file -> file.isFile && file.name.endsWith(".pl") }
            ?.sortedBy { it.name }
            ?.map { SampleFile(it.nameWithoutExtension, it.absolutePath, it.readText()) }
            ?: emptyList()
    }
}
