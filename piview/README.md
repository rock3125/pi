# piview

An MCP server, written in Kotlin, that attaches to a running PI prolog
interpreter over its TCP port — and a web view of the same interpreter, in
React and TypeScript.

pi keeps **one database shared by every client and by its own prompt**. That is
what piview is for: a clause typed at the `>` prompt, one asserted by an agent
through MCP, and the listing in the browser are all the same database. You watch
an agent reason over prolog while you still hold the prompt.

```
        ┌──────────┐   MCP over stdio        ┌──────────┐   TCP     ┌────────┐
        │  agent   │◄───────or /mcp─────────►│          │  one      │        │
        └──────────┘                         │  piview  │◄─command─►│   pi   │
        ┌──────────┐   REST + WebSocket      │          │  per      │        │
        │ browser  │◄───────────────────────►│          │  connect  │        │
        └──────────┘                         └──────────┘           └────────┘
                                                                        ▲
                                                                   the `>` prompt
```

## Quick start

### With Docker

From the repository root:

```sh
docker compose up --build
```

Then open <http://127.0.0.1:7070>. That brings up two containers — pi built from
source and serving on 7071, and piview serving the web view on 7070 with the MCP
endpoint at `/mcp`. Both ports are published to loopback only.

### On a server

Build an installer, copy it over, run it. It installs the stack under systemd,
so it comes back after a reboot.

```sh
./piview/install/build-installer.sh                # 150 KB; the server builds
./piview/install/build-installer.sh --with-images   # 160 MB; the server builds nothing
```

```sh
scp piview/install/dist/piview-1.0.0-installer.tar.gz user@server:
ssh user@server
tar xzf piview-1.0.0-installer.tar.gz && cd piview-1.0.0-installer
sudo ./install.sh
```

Tested on Ubuntu 24.04, Debian 13 and Fedora 44, on a host that already runs
docker with the compose plugin. `piviewctl` manages it afterwards. The details,
including how to put it behind an authenticating proxy, are in
[install/payload/README.md](install/payload/README.md) — the same file the
installer leaves in `/opt/piview`.

### Without Docker

Start pi with its port open, then piview:

```sh
make                                    # builds ./prolog
./prolog --port 7071 samples/family.pl

cd piview/server
./gradlew installDist                   # builds the ui too; needs node
./build/install/piview/bin/piview --samples ../../samples
```

`./gradlew installDist -PskipUi=true` builds the server alone when node is not
around; the web view then serves a fallback page and the MCP tools still work.

For UI development, run the vite dev server against a running piview:

```sh
cd piview/ui && npm run dev             # http://localhost:5173, proxying /api
```

## Wiring it to an agent

Over stdio — add to `.mcp.json`, or `claude mcp add`:

```json
{
  "mcpServers": {
    "piview": {
      "command": "/path/to/pi/piview/server/build/install/piview/bin/piview",
      "args": ["--stdio", "--port", "7071", "--samples", "/path/to/pi/samples"]
    }
  }
}
```

That also brings up the web view on 7070; add `--no-web` for MCP only.

Over HTTP — piview always mounts a streamable-HTTP MCP endpoint on the web port,
so a client that speaks it can use `http://127.0.0.1:7070/mcp` with nothing else
running.

### The tools

| Tool | What it does |
| --- | --- |
| `prolog_query` | Solve a goal and get every solution back, parsed into variables and bindings. `?-` and the trailing `.` are optional. |
| `prolog_assert` | Add clauses. Takes a whole program — comments and line breaks included — and adds each statement in order. |
| `prolog_list` | The database, with the numbers `prolog_delete` takes, filterable by range or predicate name. |
| `prolog_delete` | Remove clauses by number. |
| `prolog_load` | Have pi read a `.pl` file from its own filesystem. |
| `prolog_clear` | Empty the database. Refuses without `confirm: true`. |
| `prolog_status` | Is pi answering, and what does it hold. A good first call. |
| `prolog_command` | One raw line as it would be typed at pi's prompt, for `tron` and friends. Refuses `exit`. |

Two resources come with them: `prolog://database`, the whole listing as prolog
source, and `prolog://activity`, what piview has sent to pi.

Every tool returns both a text block laid out the way pi lays it out and a
`structuredContent` object carrying the same answer as fields.

## Options

```
--host <host>       host pi is listening on            (default 127.0.0.1)
--port <n>          pi's --port                        (default 7071)
--web-port <n>      port for the web view              (default 7070)
--bind <addr>       interface for the web view         (default 127.0.0.1)
--ui-dir <path>     serve the ui from a directory instead of the jar
--samples <path>    directory of .pl files to offer in the ui
--stdio             also speak MCP on stdin/stdout
--no-web            do not start the web view; MCP on stdio only
--timeout <ms>      how long to wait for an answer      (default 60000)
```

pi does not have to be up first. piview says so, keeps polling, and attaches
when it appears.

## How it talks to pi

pi's protocol is one command per line, and the answer is free text with no
terminator — there is no way to tell where one answer ends and the next begins
on a shared connection. pi does close its side when the client half-closes, so
every command gets its own short-lived connection: send, shut down the write
side, read to end of file. That is what pi's own `tests/test_server.py` does.
Nothing is lost by not holding a connection open, because there is no
per-connection state to lose.

Answers come back as prose, so structure has to be recovered
([`Parser.kt`](server/src/main/kotlin/nz/pi/piview/Parser.kt)). Two facts about
the engine make that reliable:

- a query's output is built in one order — anything `write/1` printed, then one
  `Var=value` line per binding per solution, then `yes` if there were no
  bindings at all (`Query::ExecuteQuery`);
- solutions are emitted back to back with no separator, but a solution binds
  each variable at most once (`Engine::GatherResults`), so a repeated variable
  name is where the next solution starts.

The raw answer is carried through to both the tools and the browser, so nothing
depends on the parse being right.

The database is polled every 1.5 s and pushed over a websocket when it changes.
Polling is the only way to see a clause typed at pi's own prompt, and pi runs
commands one at a time behind a global lock, so the interval is deliberately
unhurried.

### Two things to know

- **A bare `no` is read as failure.** A goal that succeeds having printed
  exactly `no` and bound nothing is indistinguishable from a failed goal on the
  wire. The raw answer is identical in both cases; this is not recoverable in
  piview.
- **Clause numbers shift.** `delete` renumbers everything after it, so list
  again before deleting a second time. The UI refreshes the listing after every
  change for this reason.

## Layout

```
piview/
  server/     kotlin: the pi client, the parser, the MCP tools, the web api
  ui/         react + vite + typescript: the web view
  docker/     one image for pi, one for piview
```

## Licence

Apache 2.0, the same as pi.
