# PI Prolog Interpreter — C4 model

An architecture description in the [C4 model](https://c4model.com): context,
containers, components, and then the code-level structures that matter.

Diagrams are Mermaid and render on GitHub.

---

## Level 1 — System context

Who uses the interpreter and what it touches.

```mermaid
flowchart TB
    dev["<b>Developer</b><br/><i>[Person]</i><br/>Writes and runs Prolog<br/>at the terminal"]
    client["<b>Client program or script</b><br/><i>[Software System]</i><br/>Anything that can write to<br/>a socket: nc, telnet, an app"]

    pi["<b>PI Prolog Interpreter</b><br/><i>[Software System]</i><br/>Parses and solves Prolog<br/>against an in-memory database"]

    fs["<b>File system</b><br/><i>[External System]</i><br/>Prolog program files,<br/>~/.pi_history"]

    dev -->|"types commands and queries,<br/>reads answers [terminal]"| pi
    client -->|"sends commands one per line,<br/>reads answers [TCP, plain text]"| pi
    pi -->|"reads program files,<br/>reads and writes history"| fs

    classDef person fill:#08427b,stroke:#052e56,color:#fff
    classDef system fill:#1168bd,stroke:#0b4884,color:#fff
    classDef external fill:#999,stroke:#6b6b6b,color:#fff
    class dev,client person
    class pi system
    class fs external
```

**Notes**

- The two ways in are equivalent: the same commands, the same database, the
  same answers. The TCP interface is optional and off unless `--port` is given.
- There is no authentication. `load` reads files from the machine running the
  interpreter, so the server binds to `127.0.0.1` unless told otherwise.

---

## Level 2 — Containers

The interpreter is a **single process** with no data store of its own, so this
level is deliberately thin — the structure worth reading is at level 3.

```mermaid
flowchart TB
    dev["<b>Developer</b><br/><i>[Person]</i>"]
    client["<b>Client program</b><br/><i>[Software System]</i>"]

    subgraph sys["PI Prolog Interpreter"]
        proc["<b>prolog</b><br/><i>[Container: C++17 executable]</i><br/>Prompt, optional TCP server,<br/>parser, database and engine.<br/>All state is in memory"]
    end

    fs["<b>File system</b><br/><i>[External System]</i>"]

    dev -->|"stdin / stdout<br/>[terminal, raw mode]"| proc
    client -->|"[TCP, line oriented text]"| proc
    proc -->|"[POSIX file I/O]"| fs

    classDef person fill:#08427b,stroke:#052e56,color:#fff
    classDef container fill:#438dd5,stroke:#2e6295,color:#fff
    classDef external fill:#999,stroke:#6b6b6b,color:#fff
    class dev,client person
    class proc container
    class fs external

    style sys fill:none,stroke:#9aa0a6,stroke-dasharray:4 3
```

**Notes**

- The database lives only in memory. Nothing is persisted between runs except
  the command history.
- Threads inside the process: the main thread runs the prompt, one thread
  accepts connections, and one thread serves each connected client.

---

## Level 3 — Components

Inside the `prolog` process. Each box is a source file pair in `src/`.

```mermaid
flowchart TB
    dev(["Developer<br/><i>[Person]</i>"])
    client(["Client program<br/><i>[Software System]</i>"])

    subgraph proc["prolog [Container]"]

        subgraph front["Front ends"]
            main["<b>Startup / CLI</b><br/><i>[main.cpp]</i><br/>Parses arguments, owns the<br/>prompt loop and the database"]
            edit["<b>LineEditor</b><br/><i>[lineedit.cpp]</i><br/>Raw-mode line editing and<br/>command history"]
            srv["<b>PrologServer</b><br/><i>[server.cpp]</i><br/>Accepts connections, one<br/>thread per client"]
        end

        disp["<b>Command dispatch</b><br/><i>[interpreter.cpp]</i><br/>Turns one line into an answer.<br/>Owns the parser instance and<br/>the engine lock"]

        subgraph frontend["Compilation"]
            parse["<b>PrologParser</b><br/><i>[parser.cpp]</i><br/>Recursive descent, text to<br/>a parse tree"]
            struc["<b>Structure</b><br/><i>[structure.cpp]</i><br/>Parse tree node; holds the<br/>string and variable tables"]
            node["<b>Node</b><br/><i>[node.cpp]</i><br/>Flattens a Structure tree into<br/>an indexable node list"]
        end

        subgraph runtime["Solving"]
            db["<b>DataBase</b><br/><i>[database.cpp]</i><br/>Clause store, indexed by<br/>name and arity"]
            query["<b>Query</b><br/><i>[query.cpp]</i><br/>Resolution: clause selection,<br/>and / or / not / cut"]
            engine["<b>Engine</b><br/><i>[engine.cpp]</i><br/>Node stack, unification,<br/>bindings, result sets"]
            bind["<b>Binding</b><br/><i>[binding.cpp]</i><br/>Binding, BindingList, Set"]
        end

        subgraph plat["Platform"]
            system["<b>System</b><br/><i>[system.cpp]</i><br/>Output writer, formatting,<br/>file loading"]
            timer["<b>TTime</b><br/><i>[timer.cpp]</i><br/>Monotonic clock"]
        end
    end

    dev --> edit
    client --> srv
    edit --> main
    main --> disp
    srv --> disp

    disp --> parse
    disp --> db
    disp --> query
    parse --> struc
    disp --> node
    node --> struc
    query --> engine
    query --> db
    engine --> bind
    engine --> node
    db --> node

    disp --> system
    engine --> system
    srv --> system
    system --> timer

    classDef person fill:#08427b,stroke:#052e56,color:#fff
    classDef comp fill:#85bbf0,stroke:#5d82a8,color:#000
    class dev,client person
    class main,edit,srv,disp,parse,struc,node,db,query,engine,bind,system,timer comp

    style proc fill:none,stroke:#9aa0a6,stroke-dasharray:4 3
    style front fill:none,stroke:#9aa0a6
    style frontend fill:none,stroke:#9aa0a6
    style runtime fill:none,stroke:#9aa0a6
    style plat fill:none,stroke:#9aa0a6
```

### Responsibilities

| Component | Files | Responsibility |
| --- | --- | --- |
| Startup / CLI | `src/main.cpp` | Argument parsing, owns the `DataBase`, runs the prompt loop, starts and stops the server |
| LineEditor | `src/lineedit.cpp/h` | termios raw mode, cursor and word editing, history in `~/.pi_history`. Steps aside when input is not a terminal |
| PrologServer | `src/server.cpp/h` | Listening socket, accept loop, a thread per client, captures each answer for its own client |
| Command dispatch | `src/interpreter.cpp/h` | `ExecuteCommand` — the one place a line becomes an answer. Owns the shared `PrologParser` and publishes `EngineLock` |
| PrologParser | `src/parser.cpp/h` | Recursive-descent parser for statements, queries and interpreter commands |
| Structure | `src/structure.cpp/h` | Parse-tree node; also the global string table and per-statement variable table |
| Node | `src/node.cpp/h` | Flattens a `Structure` tree into `vector<Node*>`; printing nodes back to text |
| DataBase | `src/database.cpp/h` | Owns the clauses as node lists; `name * 1000 + arity` index onto candidate clauses |
| Query | `src/query.cpp/h` | Drives resolution: picks candidate clauses, walks `,` `;` `not` `!`, proves rule bodies |
| Engine | `src/engine.cpp/h` | The node stack, forward unification, binding creation, result-set filtering and formatting |
| Binding | `src/binding.cpp/h` | `Binding` (a pair of stack indices), `BindingList`, `Set` |
| System | `src/system.cpp/h` | Output through a per-thread `IOWriter`, formatting helpers, text file loading |
| TTime | `src/timer.cpp/h` | `CLOCK_MONOTONIC` timing for the "execution time" line |

### Concurrency

```mermaid
flowchart LR
    subgraph threads["Threads"]
        m["main thread<br/>prompt"]
        a["accept thread"]
        c1["client thread 1"]
        c2["client thread N"]
    end

    lock{{"EngineLock<br/><i>std::mutex</i>"}}
    shared[("Shared engine state<br/>Engine::stack, bindingPool,<br/>outstring, tron,<br/>Structure string and var tables,<br/>the PrologParser instance,<br/>the DataBase")]

    m -->|"ExecuteCommand"| lock
    c1 -->|"ExecuteCommand"| lock
    c2 -->|"ExecuteCommand"| lock
    a -.->|"spawns"| c1
    a -.->|"spawns"| c2
    lock --> shared

    classDef t fill:#85bbf0,stroke:#5d82a8,color:#000
    classDef l fill:#ffd966,stroke:#bf9000,color:#000
    classDef s fill:#f4b6b6,stroke:#b45f5f,color:#000
    class m,a,c1,c2 t
    class lock l
    class shared s

    style threads fill:none,stroke:#9aa0a6
```

The engine keeps its state in globals, so **commands execute one at a time**.
Threads buy clients that never block each other on I/O; they do not buy
parallel solving.

Two pieces of state are deliberately *not* behind the lock, because they are
per thread instead:

- `System::writer` — `thread_local`. The main thread writes to the console; a
  client thread writes into a string it then sends back. Without this, a
  client capturing its output would steal the prompt's.
- The scratch buffers in `System::printf` / `System::lprintf` — `thread_local`
  for the same reason.

`PrologServer::running` is `std::atomic<bool>`; the accept loop is woken by a
self-pipe rather than by closing the listening socket, so no descriptor can be
reused while another thread is still selecting on it.

---

## Level 4 — Code

### The pipeline

Text becomes a tree, the tree becomes a flat list, the list is copied onto a
stack and solved there.

```mermaid
flowchart LR
    text["source text<br/><i>std::string</i>"]
    tree["parse tree<br/><i>Structure*</i>"]
    list["node list<br/><i>vector&lt;Node*&gt;</i>"]
    dbs["clause store<br/><i>DataBase</i>"]
    stack["engine stack<br/><i>Engine::stack</i>"]
    out["answer text"]

    text -->|"PrologParser::<br/>ParseStatements<br/>ParseQuery"| tree
    tree -->|"Node::<br/>StructureToNodeList"| list
    list -->|"DataBase::Add<br/>(statements)"| dbs
    list -->|"Query::Query<br/>(queries)"| stack
    dbs -->|"copied per<br/>clause attempt"| stack
    stack -->|"Engine::<br/>GatherResults"| out
```

### Key structures

**`Structure`** — the parse tree. A tagged node: `tag` says which of the union
of fields is live (`left`/`right` for operators, `structures` for a compound
term, `list` for a list, `i`/`f`/`b`/`name` for literals).

Strings and variable names are never stored as text inside the engine.
`Structure::AddString` interns them into two tables and returns an index:

- **atoms** → an index into `stringArray` (slots 0 and 1 are pre-seeded with
  `write` and `nl`, which is how those built-ins are recognised)
- **variables** → an index into `varArray`, offset by
  `VAR_PREFIX = 0x80000000`

so the top bit of a name says "this is a variable". Variable identity is
per statement: `MarkVariableFrame` moves a watermark so that `X` in one clause
is a different variable from `X` in the next.

> These names are `size_t`. Holding one in an `int` makes it negative and it
> then never compares equal to itself — a bug that once made `ForwardBind` a
> silent no-op.

**`Node`** — the flat form. The whole clause is one contiguous array, and each
node records the size of its own subtree:

| Field | Meaning |
| --- | --- |
| `type` | the `Structure::Predicate` tag this came from |
| `name`, `arity` | interned name and argument count |
| `size` | **number of nodes in this subtree, including itself** |
| `fu` | forward link: the stack index this variable stands for, or `-1` |
| `i`, `f`, `b` | literal values |

`size` is the load-bearing invariant. Walking a term's arguments is
`index += SizeOfClause(index)` rather than pointer chasing, so a wrong `size`
silently makes the engine read the same slot forever.

**`Binding`** — a pair of stack indices, `lhs` (a variable node) and `rhs`
(what it stands for). `BindingList` is one solution; `Set` is all solutions.

### Variable binding

There is no substitution and no trail. A variable is bound by pointing `fu` at
another stack slot, and resolving one means following that chain to its end:

```mermaid
flowchart LR
    b["body N<br/><i>ST_VAR</i>"] -->|fu| h["head N<br/><i>ST_VAR</i>"]
    h -->|fu| c["caller X<br/><i>ST_VAR</i>"]
    c -->|fu| v["6<br/><i>ST_INT</i>"]
```

`Engine::GetForwardValue` walks that chain. This is why an assignment resolves
its target *before* writing to it — writing to the near end would cut the
caller off from the answer.

Three things build these links:

| Function | Links |
| --- | --- |
| `Engine::ForwardBind` | occurrences of the same variable *within* one clause |
| `Engine::ForwardUnify` | a clause head to the caller's arguments |
| `Engine::CreateBindings` | results back out of a proved clause |

### Solving a query

```mermaid
sequenceDiagram
    participant C as Caller<br/>(prompt or client)
    participant D as Command dispatch
    participant P as PrologParser
    participant Q as Query
    participant DB as DataBase
    participant E as Engine

    C->>D: ExecuteCommand("?- father(fred,X).")
    D->>P: ParseQuery
    P-->>D: Structure*
    D->>D: Node::StructureToNodeList
    D->>E: ResetStack
    D->>Q: Query(nodes, database)
    Q->>E: copy query onto the stack

    D->>Q: ExecuteQuery
    loop for each goal
        Q->>DB: Get(name, arity)
        DB-->>Q: candidate clause indices
        loop for each candidate
            Q->>E: copy clause onto the stack
            Q->>E: ForwardUnify(clause head, goal)
            alt head unifies
                Q->>Q: prove the body
                Q->>E: CreateBindings
            end
        end
    end
    Q-->>D: success plus a Set of solutions
    D->>E: FilterSet, GatherResults
    E-->>D: "X=peter\nX=mark\n..."
    D-->>C: answer text
```

**A goal is solved to its complete set of solutions in one pass**, rather than
by backtracking through choice points. That single decision explains most of
what looks unusual in the engine:

- the cut truncates a solution list instead of unwinding a choice point
- a query with very many solutions holds them all in memory at once
- `FilterSet` exists at all — solutions must be reduced to the variables the
  caller actually asked about, at each clause boundary

### Memory

| Owner | Owns | Released by |
| --- | --- | --- |
| `DataBase` | the clause node lists | `Clear`, `Delete`, destructor |
| `Engine` | the node stack and every `Binding` | `ResetStack`, per query |
| `Query` / dispatch | parse trees and temporary node lists | at the end of the call |

`Binding` objects are handed out by `Engine::NewBinding` and pooled, so they
are all released together with the stack. Nothing accumulates across queries.

### Limits

| Constant | Value | Where |
| --- | --- | --- |
| `Query::MAX_PC` | 1 500 000 | stack index before "infinite recursion detected" |
| `LineEditor::MAX_HISTORY` | 500 | commands kept in `~/.pi_history` |
| `PrologServer::MAX_LINE` | 1 MiB | longest command line a client may send |
| `PrologServer::LISTEN_BACKLOG` | 16 | connections waiting to be accepted |
| `DataBase::NAME_MULTIPLIER` | 1000 | index key is `name * 1000 + arity` |
| `BUFFER_SIZE` | 10 000 | per-thread `System::printf` scratch buffer |

> `NAME_MULTIPLIER` caps arity at 999 before index keys would collide.

---

## Reading order

1. [`interpreter.cpp`](../src/interpreter.cpp) — one line in, one answer out
2. [`parser.cpp`](../src/parser.cpp) — text to `Structure`
3. [`node.cpp`](../src/node.cpp) — `Structure` to the flat node list
4. [`query.cpp`](../src/query.cpp) — the resolution loop
5. [`engine.cpp`](../src/engine.cpp) — unification and bindings underneath it
