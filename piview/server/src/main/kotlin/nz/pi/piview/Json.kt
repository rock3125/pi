package nz.pi.piview

import kotlinx.serialization.json.Json as KotlinxJson

/**
 * piview's own JSON settings.
 *
 * Deliberately separate from the MCP SDK's `McpJson`, which turns the class
 * discriminator off - that would strip the `type` field the browser switches on
 * when it reads a [ServerEvent] off the websocket.
 */
val Json: KotlinxJson = KotlinxJson {
    encodeDefaults = true
    ignoreUnknownKeys = true
    explicitNulls = false
    classDiscriminator = "type"
    prettyPrint = false
}
