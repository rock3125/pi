# PI — product context

## What it is

PI is a small Prolog interpreter written in C++ (~7k lines, no third-party
dependencies), built to be read as much as used. It runs at a `>` prompt and,
with `--port`, over TCP.

`piview` is the second surface: a Kotlin MCP server that attaches to a running
pi over that port, giving an AI agent typed tools for querying and editing the
database, and giving a person a live web view of the same interpreter.

## The unique mechanism

pi keeps **one database shared by every client and by its own prompt**. A clause
typed at the terminal, asserted by an agent through MCP, and shown in the browser
are all the same database. piview's reason to exist is making that shared state
visible: you watch an agent reason over prolog while you hold the prompt.

## Audience and scene

The author and readers of pi: systems programmers and language implementors,
sitting at a terminal with pi running in one pane and an agent in another,
usually in a dark room. They already know prolog; they do not need it explained.
They do need to see what the database currently holds, what a goal actually
returned, and what the agent just did to it.

## Product truth that constrains design

- pi is **not** standard prolog: `=` both evaluates and unifies (no `is`), there
  is no `assert/1` or `retract/1`, and the built-ins are `write/1` and `nl/0`.
- The database is a flat, **numbered** list. Clauses are addressed by their
  listing number, and those numbers shift when one is deleted.
- A goal is solved to its **complete solution set in one pass**, so answers
  arrive all at once rather than on backtracking. Solution counts and timings
  are meaningful; incremental "next solution" is not a thing here.
- pi's wire protocol returns prose, not structure. Everything piview shows is
  recovered by parsing, so the raw answer must always stay reachable.
- The interpreter is single-threaded behind a global lock: piview must not
  hammer it.

## What success looks like

A person opens piview and, within seconds, knows: whether pi is answering, what
is loaded, and what the last thing to touch the database was. Running a goal and
reading its solutions is faster than typing it at the prompt, because the
solutions arrive as a table instead of a stream of `X=...` lines.

## Constraints

- Local-only by default (pi's own protocol has no authentication).
- Ships inside the server jar; no external network at runtime, so fonts and
  assets are self-hosted.
