# Design — piview

Recorded from the built interface, not from intention. The surface is
`piview/ui`; the direction contract it was built against is the HTML comment at
the top of `piview/ui/index.html`.

## Mode and scene

**Operate.** Someone with pi already running in another pane, usually in a dark
room, who needs to see what the database holds, what a goal returned, and what an
agent just did to it. They know prolog. Expression never gets in front of the
task.

Dark is not a category default here: the surface sits beside a terminal, on the
same screen, at the same time.

## The world

A phosphor instrument clamped onto a running interpreter. Near-black ground under
a faint scanline field and a corner falloff, both far enough down to read as
material rather than as an effect. Regions are separated by hairline rules and a
gutter mark, never by cards — the one arrangement this surface refuses is the
card-grid admin panel.

### Colour

Restrained: a near-black ground, four signals, nothing else.

| Token | Value | Carries |
| --- | --- | --- |
| `--ink-1` | `#070a09` | the ground |
| `--ink-2` `--ink-3` `--ink-4` | `#0b100e` `#101613` `#16201b` | raised surfaces, in that order |
| `--rule-faint` `--rule` `--rule-strong` | `#1a2521` `#24322c` `#35473f` | dividers, borders, marks |
| `--text` `--text-2` `--text-3` | `#dbe6df` `#93a89c` `#64796e` | body, secondary, faint |
| `--phosphor` | `#64f0a8` | success, bindings, life |
| `--amber` | `#f2b53c` | `no` — a goal that ran and failed |
| `--alarm` | `#ff6f5f` | a goal that never ran: parse errors, faults |
| `--agent` | `#b291ff` | anything an agent did, and the `:-` neck |
| `--var` | `#63cdf2` | variables, and the web as a source |

The three outcomes are a colour each and never share one. Green means the
interpreter answered, amber means it answered `no`, red means it did not get
that far — the distinction prolog itself makes, which a single "error" colour
would flatten.

Each signal appears at three strengths: full for text, `color-mix` at ~9% for a
fill, ~22% for a border. The `-dim` tokens carry the gutter marks.

### Type

- **`--mono`, JetBrains Mono Variable** — every piece of prolog: clauses,
  goals, bindings, raw answers, the prompt. Ligatures are switched off wherever
  prolog is shown: the font would draw `!=` as `≠`, and this view reports what is
  in the database.
- **`--chrome`, Archivo Variable** — labels, verdicts, tabs, buttons, in tracked
  uppercase at 0.69rem/620. Chrome is never mono, prolog is never anything else,
  and the two never swap.

Scale: `0.6875 · 0.8125 · 0.9375 · 1.125 · 1.5rem`. The display end is barely
used — the largest thing on the surface is the word `yes`.

### Space

Rails are fixed (`19rem` left, `22.5rem` right, `3.25rem` top); the console
takes the rest. Inside a panel: `0.75rem` horizontal throughout, rows at
`0.25–0.5rem`, entries at `0.5rem` above and `0.85rem` below. Tight groups,
hairline separation.

## Structure

Three columns under one status rail.

- **Top rail** — gauges, not a header: interpreter address with a pulsing life
  dot, state, clause and predicate counts, the MCP endpoint. The one action here
  is destructive and sits at the far right, alone.
- **Left rail, the database** — predicates grouped and collapsible, each with a
  fact tally and a rule tally in different colours; clauses numbered as pi
  numbers them, clicking one prefills a query, hovering reveals its delete.
  Samples at the bottom.
- **Centre, the console** — the spine. Transcript above, a live `?-` prompt
  below. The prompt decides between solving and asserting the way pi does, from
  the leading `?`, and says which above the input rather than offering a toggle.
- **Right rail** — relations or activity. The relation graph draws ground facts
  only, each connected island settled on its own and packed into a grid, because
  one simulation over the lot puts the interesting cluster in a corner. The
  activity feed's `source` column is the point of the whole surface: `mcp` is an
  agent working the same database you are looking at.

Under 1000px the three columns become one, with a panel switcher; under 620px
the gauges and button labels drop and the icons stay.

## Answers

The shape of an answer is the design problem. Four renderings, never mixed:

- **Bindings** — a table, variables as columns, one row per solution, rows
  printing in on a 22ms stagger. Real columns size to their content and an empty
  column absorbs the slack, so two variables sit next to each other; the table
  scrolls in its own box when a term is long.
- **`yes`** — the word, at `1.125rem`, in phosphor.
- **`no`** — the word, in amber, with one line saying what it means.
- **A fault** — the parser's own message, in a red-bordered block.

`write/1` output is always a separate block labelled `written`: it is not an
answer. Every entry carries a collapsed `raw answer` — the parse is a
convenience, and the wire is the truth.

The meta line never repeats what the answer already says. It carries the
solution count when there is one worth scanning, and the execution time in µs,
ms or s.

## Motion

One authored moment: solutions printing in row by row, which is what the
interpreter is doing. Around it, only the life dot's pulse and short state
transitions on hover, disclosure and tabs. Everything respects
`prefers-reduced-motion`.

## Language

pi's own. `clauses`, `predicates`, `solutions`, `assert`, `answering`. Errors
name the problem and the recovery: *"pi is not answering. Start it with its port
open — `./prolog --port 7071 samples/family.pl` — and this view reconnects on
its own."* The empty console teaches the two things that trip people up in this
dialect: there is no `assert/1`, and `=` is not `is`.

## Icons

Drawn, in one 16-unit grid at 1.4 stroke, in `components/Icons.tsx`. No emoji,
no glyph fonts. The only typographic mark is the `π` in the brand, set in a
serif italic — the mark the 2004 original used.
