#!/usr/bin/env python3
#
#     PI Prolog Interpreter
#     Copyright (C) 2004  Rock de Vocht
#
#     Licensed under the Apache License, Version 2.0 (the "License");
#     you may not use this file except in compliance with the License.
#     You may obtain a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#     Unless required by applicable law or agreed to in writing, software
#     distributed under the License is distributed on an "AS IS" BASIS,
#     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#     See the License for the specific language governing permissions and
#     limitations under the License.
#
"""Build docs/c4-model.html from docs/c4-model.md, and the pdf from that.

    make docs      the html
    make pdf       docs/c4-model.pdf, printed from the html

The mermaid blocks are pre-rendered to inline SVG rather than pulled from a
CDN at view time, so the finished page makes no network requests and reads
fine offline.  Each diagram is embedded twice, in mermaid's light and dark
themes, and swapped by prefers-color-scheme.

The pdf is the same page printed by headless chrome, so the print stylesheet
in CSS below is the whole of the page-break policy: what may not be split,
what starts a page, and which diagrams are turned onto a landscape page
because they are wider than they are tall.  Ghostscript then stamps the
document information, which chrome does not carry over from the html.

This is the only part of the project that needs anything beyond a C++
compiler, which is why it is a separate target and not part of "make":

    node and npx   to run marked and mermaid-cli
    a chrome build for mermaid-cli to render with, and to print the pdf
    ghostscript    to set the pdf's title and author

None is needed to build or run the interpreter.

Note that the output is not byte for byte reproducible: mermaid lays the
diagrams out through a headless browser, and the SVG path geometry shifts
very slightly between runs.  Regenerating an unchanged document therefore
still produces a small diff.  It is cosmetic.
"""

import base64
import json
import os
import re
import select
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "c4-model.md")
TARGET = os.path.join(HERE, "c4-model.html")
PDF_TARGET = os.path.join(HERE, "c4-model.pdf")

#! what goes in the html and in the pdf's document information
TITLE = "PI Prolog Interpreter - C4 model"
AUTHOR = "Rock de Vocht"
SUBJECT = ("C4 architecture model of the PI Prolog interpreter and piview: "
           "context, containers, components and code.")
#! the attribution on the page and in the footer of every printed page.  The
#! year is written out rather than taken from the clock, so rebuilding an
#! unchanged document does not quietly restamp it
COPYRIGHT = "Rock de Vocht, 2026"

#! a diagram this much wider than it is tall, and this many units across, is
#! printed on a landscape page of its own rather than squeezed to portrait width
WIDE_RATIO = 1.6
WIDE_UNITS = 1200

#! pinned so the output does not change under us
MERMAID = "@mermaid-js/mermaid-cli@11"
MARKED = "marked@18"

#! chrome builds mermaid-cli can drive, in the order we try them
BROWSERS = [
    "google-chrome-stable",
    "google-chrome",
    "chromium",
    "chromium-browser",
]


def fail(message):
    sys.stderr.write("build-html: %s\n" % message)
    sys.exit(1)


def find_npx():
    npx = shutil.which("npx")
    if npx is None:
        fail("npx not found - install node.js to build the html docs")
    return npx


def find_browser():
    """mermaid-cli renders through puppeteer, which needs a browser."""
    explicit = os.environ.get("PUPPETEER_EXECUTABLE_PATH")
    if explicit:
        if not os.path.exists(explicit):
            fail("PUPPETEER_EXECUTABLE_PATH is set to '%s', which does not exist"
                 % explicit)
        return explicit

    for name in BROWSERS:
        found = shutil.which(name)
        if found:
            return found

    fail("no chrome or chromium found - install one, or point\n"
         "            PUPPETEER_EXECUTABLE_PATH at it")


def run(command, what):
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        sys.stderr.write(result.stdout[-2000:])
        sys.stderr.write(result.stderr[-2000:])
        fail("%s failed" % what)


def split_mermaid(markdown):
    """Pull the mermaid blocks out, leaving a placeholder behind for each."""
    blocks = re.findall(r"```mermaid\n(.*?)```", markdown, re.S)

    body = markdown
    for index in range(len(blocks), 0, -1):
        pattern = re.compile(r"```mermaid\n" + re.escape(blocks[index - 1]) + r"```")
        body = pattern.sub("@@MERMAID%d@@" % index, body)

    return blocks, body


#! Mermaid's own furniture - edges, labels, clusters, sequence actors and
#! notes - in the page's palette.  The node fills come from the classDefs in
#! the markdown; everything mermaid draws for itself comes from here.  Built
#! on the "base" theme, which is the one that takes these.
def _mermaid_theme(ground, surface, surface_2, line, ink, ink_2, ink_3,
                   note_bkg, note_border):
    return {
        "theme": "base",
        "themeVariables": {
            "fontFamily": '"Helvetica Neue", Helvetica, Arial, sans-serif',
            "fontSize": "16px",
            "background": ground,
            "primaryColor": surface_2,
            "primaryTextColor": ink,
            "primaryBorderColor": ink_3,
            "secondaryColor": surface,
            "tertiaryColor": ground,
            "mainBkg": surface_2,
            "nodeBorder": ink_3,
            "lineColor": ink_3,
            "textColor": ink,
            "titleColor": ink,
            "clusterBkg": ground,
            "clusterBorder": line,
            "edgeLabelBackground": ground,
            "labelBoxBkgColor": surface_2,
            "labelBoxBorderColor": line,
            "labelTextColor": ink,
            "actorBkg": surface_2,
            "actorBorder": ink_3,
            "actorTextColor": ink,
            "actorLineColor": ink_3,
            "signalColor": ink_2,
            "signalTextColor": ink,
            "loopTextColor": ink,
            "activationBkgColor": surface_2,
            "activationBorderColor": ink_3,
            "sequenceNumberColor": ground,
            "noteBkgColor": note_bkg,
            "noteBorderColor": note_border,
            "noteTextColor": ink,
        },
    }


MERMAID_THEMES = {
    "light": _mermaid_theme(
        ground="#F1F2ED", surface="#E7E8E3", surface_2="#DCDED7",
        line="#C5C8C0", ink="#161D20", ink_2="#56605D", ink_3="#7C8683",
        # a note is an aside in the margin: parchment, ruled in orpiment
        note_bkg="#F2E7D3", note_border="#8A6F2E",
    ),
    "dark": _mermaid_theme(
        ground="#141C1F", surface="#0D1315", surface_2="#1C2528",
        line="#2B383C", ink="#E6E4DC", ink_2="#9AA5A3", ink_3="#6E7A79",
        note_bkg="#2A2418", note_border="#B99B58",
    ),
}


def render_diagrams(blocks, work, npx, browser):
    """Render every block to a light and a dark SVG."""
    config = os.path.join(work, "puppeteer.json")
    with open(config, "w") as handle:
        handle.write('{"executablePath": %s, "args": ["--no-sandbox",'
                     ' "--disable-dev-shm-usage"]}' % _json_string(browser))

    themes = {}
    for variant, theme in MERMAID_THEMES.items():
        themes[variant] = os.path.join(work, "mermaid-%s.json" % variant)
        with open(themes[variant], "w") as handle:
            json.dump(theme, handle)

    rendered = []
    for index, block in enumerate(blocks, 1):
        source = os.path.join(work, "d%d.mmd" % index)
        with open(source, "w") as handle:
            handle.write(block)

        variants = {}
        for variant in ("light", "dark"):
            output = os.path.join(work, "d%d-%s.svg" % (index, variant))
            run([npx, "-y", MERMAID, "-p", config, "-b", "transparent",
                 "-c", themes[variant], "-i", source, "-o", output],
                "rendering diagram %d (%s)" % (index, variant))
            variants[variant] = _prepare_svg(output, index, variant)

        rendered.append(variants)
        sys.stdout.write("  diagram %d/%d\n" % (index, len(blocks)))
        sys.stdout.flush()

    return rendered


def _json_string(text):
    return '"%s"' % text.replace("\\", "\\\\").replace('"', '\\"')


def _prepare_svg(path, index, variant):
    """Make one SVG safe to inline alongside the others."""
    with open(path) as handle:
        svg = handle.read()

    # mermaid calls every diagram "my-svg": the root id, the scoped <style>
    # selectors and the url(#my-svg_...) marker references all use it.  Left
    # alone, the diagrams would overwrite each other's styling once inlined.
    svg = svg.replace("my-svg", "diagram-%d-%s" % (index, variant))

    svg = re.sub(r"^\s*<\?xml[^>]*\?>\s*", "", svg)
    svg = re.sub(r"^\s*<!DOCTYPE[^>]*>\s*", "", svg, flags=re.I)

    # the stylesheet owns the sizing
    svg = svg.replace('width="100%"', "", 1)
    return svg.strip()


def markdown_to_html(body, work, npx):
    source = os.path.join(work, "body.md")
    output = os.path.join(work, "body.html")

    with open(source, "w") as handle:
        handle.write(body)

    run([npx, "-y", MARKED, "--gfm", "-i", source, "-o", output],
        "converting markdown")

    with open(output) as handle:
        return handle.read()


CSS = """
/* ---------------------------------------------------------------- tokens

   Seshat's hand, applied to a generated document: the palette, the type and
   the pigments of the index at https://home.peter.nz/ - carbon body, red
   ochre for the opening of a thing, Egyptian blue for what is live, orpiment
   for hairlines.  Light is the bare :root so the un-stamped "system" state
   most viewers see always resolves to a complete palette. */

:root {
  --ground:    #E7E8E3;
  --surface:   #F1F2ED;
  --surface-2: #DCDED7;
  --line:      #C5C8C0;
  --line-soft: #D6D9D1;
  --ink:       #161D20;
  --ink-2:     #56605D;
  --ink-3:     #7C8683;

  --rubric:    #A93A26;
  --faience:   #256F79;
  --gold:      #8A6F2E;
  --focus:     #256F79;

  --f-display: "Palatino Linotype", "Book Antiqua", Palatino, "Iowan Old Style", Georgia, serif;
  --f-ui: "Helvetica Neue", Helvetica, "Segoe UI", Arial, sans-serif;
  --f-data: ui-monospace, "SF Mono", SFMono-Regular, Menlo, "DejaVu Sans Mono", Consolas, monospace;

  --t-micro: 0.6875rem;
  --t-label: 0.75rem;
  --t-small: 0.875rem;
  --t-body:  1.0625rem;
  --t-h3:    1.3125rem;
  --t-h2:    clamp(1.4rem, 1.05rem + 1.4vw, 1.85rem);
  --t-h1:    clamp(1.9rem, 1.2rem + 2.8vw, 3rem);

  --measure: 74ch;
  --pad: clamp(1.25rem, 4vw, 3.5rem);
}

@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --ground:    #0D1315;
    --surface:   #141C1F;
    --surface-2: #1C2528;
    --line:      #2B383C;
    --line-soft: #212C30;
    --ink:       #E6E4DC;
    --ink-2:     #9AA5A3;
    --ink-3:     #6E7A79;

    --rubric:    #D2543C;
    --faience:   #57A9B2;
    --gold:      #B99B58;
    --focus:     #57A9B2;
  }
}

:root[data-theme="dark"] {
  --ground:    #0D1315;
  --surface:   #141C1F;
  --surface-2: #1C2528;
  --line:      #2B383C;
  --line-soft: #212C30;
  --ink:       #E6E4DC;
  --ink-2:     #9AA5A3;
  --ink-3:     #6E7A79;

  --rubric:    #D2543C;
  --faience:   #57A9B2;
  --gold:      #B99B58;
  --focus:     #57A9B2;
}

/* ---------------------------------------------------------------- base */

* { box-sizing: border-box; }

body {
  margin: 0;
  padding: 0 var(--pad) clamp(2.5rem, 6vw, 5rem);
  background: var(--ground);
  color: var(--ink);
  font-family: var(--f-display);
  font-size: var(--t-body);
  line-height: 1.7;
  -webkit-text-size-adjust: 100%;
  -webkit-font-smoothing: antialiased;
}

.sheet { max-width: 62rem; margin-inline: auto; }

::selection { background: var(--rubric); color: var(--ground); }

a { color: var(--faience); text-underline-offset: 0.2em; }

:focus-visible { outline: 2px solid var(--focus); outline-offset: 3px; border-radius: 2px; }

/* ---------------------------------------------------------------- masthead */

.mast {
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
  padding-block: clamp(2rem, 5vw, 3.5rem);
}

.eyebrow {
  font-family: var(--f-data);
  font-size: var(--t-micro);
  letter-spacing: 0.18em;
  text-transform: uppercase;
  color: var(--ink-3);
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem 1.25rem;
}

.eyebrow b { color: var(--rubric); font-weight: 400; }

/* the mark in the margin, the record beside it - the works index downstairs
   is set the same way */
.lockup { display: flex; align-items: center; gap: clamp(1rem, 3vw, 2rem); }

.lockup .mark { width: clamp(64px, 9vw, 96px); height: auto; flex: none; color: var(--ink); }

.lockup h1 {
  margin: 0;
  font-family: var(--f-display);
  font-weight: 400;
  font-size: var(--t-h1);
  line-height: 1.05;
  letter-spacing: 0.02em;
  text-wrap: balance;
}

.lockup .kind {
  font-family: var(--f-data);
  font-size: var(--t-micro);
  letter-spacing: 0.12em;
  text-transform: uppercase;
  color: var(--ink-3);
  margin-top: 0.55rem;
}

/* ---------------------------------------------------------------- body */

main { max-width: none; }

/* the first paragraph of the document carries it, so it is set as one */
main > p:first-of-type {
  font-size: var(--t-h3);
  line-height: 1.45;
  color: var(--ink-2);
  max-width: 54ch;
}

h1, h2, h3, h4 { font-weight: 400; text-wrap: balance; }

h2 {
  margin: 2.75rem 0 1.25rem;
  padding-top: 1.5rem;
  border-top: 1px solid var(--line);
  font-family: var(--f-display);
  font-size: var(--t-h2);
  line-height: 1.15;
}

h3 {
  margin: 2rem 0 0.75rem;
  font-family: var(--f-ui);
  font-size: var(--t-label);
  font-weight: 600;
  letter-spacing: 0.14em;
  text-transform: uppercase;
  color: var(--ink-2);
}

h4 {
  margin: 1.5rem 0 0.5rem;
  font-family: var(--f-data);
  font-size: var(--t-micro);
  letter-spacing: 0.12em;
  text-transform: uppercase;
  color: var(--ink-3);
}

p, ul, ol { margin: 0 0 1rem; max-width: var(--measure); }
li { margin: 0.3rem 0; }
li > ul, li > ol { margin: 0.3rem 0; }

strong { font-weight: 600; }

/* the markdown rules the sections apart; here the rule above a heading does
   that, so the horizontal rule itself is redundant */
hr { display: none; }

code {
  font-family: var(--f-data);
  font-size: 0.82em;
  background: var(--surface);
  border: 1px solid var(--line-soft);
  padding: 0.1em 0.35em;
  border-radius: 2px;
}

pre {
  background: var(--surface);
  border: 1px solid var(--line-soft);
  padding: 1rem 1.125rem;
  overflow-x: auto;
  max-width: var(--measure);
}

pre code { background: none; border: 0; padding: 0; font-size: 0.8rem; line-height: 1.6; }

blockquote {
  margin: 1.25rem 0;
  padding: 0.1rem 0 0.1rem 1.125rem;
  border-left: 2px solid var(--rubric);
  color: var(--ink-2);
  max-width: var(--measure);
}

blockquote p { margin: 0.5rem 0; }

/* ---------------------------------------------------------------- tables */

.table-scroll { overflow-x: auto; margin: 0 0 1.5rem; }

table {
  border-collapse: collapse;
  width: 100%;
  font-family: var(--f-ui);
  font-size: var(--t-small);
  line-height: 1.55;
}

th, td {
  border-bottom: 1px solid var(--line-soft);
  padding: 0.55rem 0.9rem 0.55rem 0;
  text-align: left;
  vertical-align: top;
  color: var(--ink-2);
}

th {
  font-family: var(--f-data);
  font-size: var(--t-micro);
  font-weight: 400;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  color: var(--ink-3);
  border-bottom: 1px solid var(--line);
  white-space: nowrap;
}

/* the first column of every table in this document names the thing the row
   is about, so it is set in the body hand rather than the note hand */
td:first-child { color: var(--ink); font-family: var(--f-data); font-size: 0.78rem; white-space: nowrap; }

table code { background: none; border: 0; padding: 0; }

/* ---------------------------------------------------------------- diagrams */

figure.diagram {
  margin: 1.5rem 0 2rem;
  padding: 1.25rem;
  background: var(--surface);
  border: 1px solid var(--line-soft);
  overflow-x: auto;
}

figure.diagram svg { display: block; margin: 0 auto; height: auto; max-width: 100%; }

/* one variant at a time, following the page theme */
.diagram-dark { display: none; }
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) .diagram-light { display: none; }
  :root:not([data-theme="light"]) .diagram-dark { display: block; }
}
:root[data-theme="light"] .diagram-light { display: block; }
:root[data-theme="light"] .diagram-dark { display: none; }
:root[data-theme="dark"] .diagram-light { display: none; }
:root[data-theme="dark"] .diagram-dark { display: block; }

/* ---------------------------------------------------------------- colophon */

.colophon {
  display: flex;
  align-items: center;
  gap: 1rem;
  margin-top: 3rem;
  padding-top: 1.25rem;
  border-top: 1px solid var(--line);
  font-family: var(--f-data);
  font-size: var(--t-micro);
  letter-spacing: 0.1em;
  text-transform: uppercase;
  color: var(--ink-3);
}

.colophon .mark { width: 2.25rem; height: 2.25rem; flex: none; color: var(--ink-3); }

.colophon b { color: var(--ink-2); font-weight: 400; }

@media (max-width: 40rem) {
  body { padding-inline: 1rem; }
  .lockup { gap: 1rem; }
}

/* ---------------------------------------------------------------- print */

@page { size: A4 portrait; margin: 16mm 12mm; }

/* a diagram wider than it is tall gets a landscape page to itself.  Chrome
   breaks the page wherever the page name changes, so no explicit break is
   needed - and a browser without named pages ignores the whole thing and
   prints it portrait, which is the old behaviour rather than a broken one. */
@page wide { size: A4 landscape; margin: 12mm; }

@media print {
  /* paper is white whatever the screen was set to, and the pigments stay */
  :root {
    --ground:    #ffffff;
    --surface:   #F5F6F1;
    --surface-2: #E7E8E3;
    --line:      #B8BCB4;
    --line-soft: #D6D9D1;
    --ink:       #161D20;
    --ink-2:     #4A544F;
    --ink-3:     #6E7874;
    --rubric:    #A93A26;
    --faience:   #1F5C64;
    --gold:      #8A6F2E;
  }

  /* !important because the dark rules above are more specific: an explicit
     data-theme, or a dark browser, would otherwise print the dark diagrams
     on white paper */
  .diagram-light { display: block !important; }
  .diagram-dark { display: none !important; }

  body { padding: 0; font-size: 10pt; line-height: 1.55; }
  .sheet { max-width: none; }
  .mast { padding-block: 0 1.5rem; }

  /* a heading stays with what it introduces, and each level starts a page.
     The rule above the heading is what the page break replaces. */
  h1, h2, h3, h4 { break-after: avoid; break-inside: avoid; }
  h2 { break-before: page; border-top: 0; padding-top: 0; margin-top: 0; }
  p, li { orphans: 3; widows: 3; }

  /* nothing that has to be read whole may be split across a break */
  figure.diagram, pre, blockquote, tr { break-inside: avoid; }
  thead { display: table-header-group; }
  .table-scroll { overflow: visible; }
  .colophon { break-before: avoid; }

  /* the panel around a diagram is screen furniture; on paper the page is the
     frame, and the space is better spent on the drawing */
  figure.diagram { margin: .6rem 0 1rem; padding: 0; border: 0; background: none; }

  /* fitted to the printable box in both directions, so a tall diagram scales
     down instead of being cut in half by a break */
  figure.diagram svg { max-height: 235mm; }

  /* Chrome turns the page for a named page but still lays the document out at
     the width of the first one, so a landscape figure has to be told how wide
     it may be.  Behind a feature query: a browser without named pages would
     otherwise cut a 271mm figure off at the edge of a portrait page. */
  @supports (page: wide) {
    figure.diagram--wide { page: wide; width: 271mm; max-width: none; }
    figure.diagram--wide svg { max-width: 271mm; max-height: 172mm; }
  }

  p, ul, ol, pre, blockquote { max-width: none; }
  a { color: inherit; text-decoration: none; }
}
"""

# ---------------------------------------------------------------- the marks

#! The two marks from the index page, in the same hand: 5.5 strokes, round
#! caps, exactly one rubric stroke each.  Copied rather than linked because
#! this page has to work from a file:// url with nothing beside it.

PI_MARK = """<svg class="mark" viewBox="0 0 128 128" role="img" \
aria-label="Mark for PI: a pi glyph over a red rule.">
  <g fill="none" stroke="currentColor" stroke-width="5.5" stroke-linecap="round">
    <path d="M34 46 H94"/><path d="M52 46 L46 96"/><path d="M76 46 L82 96"/>
  </g>
  <path d="M30 110 H98" fill="none" stroke="var(--rubric)" stroke-width="5.5" stroke-linecap="round"/>
</svg>"""

SESHAT_MARK = """<svg class="mark" viewBox="0 0 128 128" role="img" \
aria-label="The Seshat mark: seven rays beneath a single red arc, on a cord.">
  <g fill="none" stroke="currentColor" stroke-width="6" stroke-linecap="round">
    <path d="M64 58V36"/><path d="M70.25 61.01L87.46 47.3"/><path d="M71.8 67.78L93.25 72.67"/>
    <path d="M67.47 73.21L77.02 93.03"/><path d="M60.53 73.21L50.98 93.03"/>
    <path d="M56.2 67.78L34.75 72.67"/><path d="M57.74 61.01L40.54 47.3"/>
    <path d="M64 98v14"/><path d="M42 116h44"/>
  </g>
  <path d="M24 34C24-4 104-4 104 34" fill="none" stroke="var(--rubric)" stroke-width="6" stroke-linecap="round"/>
</svg>"""

#! the favicon is the Seshat mark again, as its own document: an svg favicon
#! inherits no theme, so it carries its own
FAVICON = (
    "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 128 128'%3E"
    "%3Cstyle%3E.ink%7Bstroke:%23161D20%7D@media(prefers-color-scheme:dark)%7B.ink%7Bstroke:%23E6E4DC%7D%7D%3C/style%3E"
    "%3Cg class='ink' fill='none' stroke-width='7' stroke-linecap='round'%3E"
    "%3Cpath d='M64 58V36'/%3E%3Cpath d='M70.25 61.01L87.46 47.3'/%3E%3Cpath d='M71.8 67.78L93.25 72.67'/%3E"
    "%3Cpath d='M67.47 73.21L77.02 93.03'/%3E%3Cpath d='M60.53 73.21L50.98 93.03'/%3E"
    "%3Cpath d='M56.2 67.78L34.75 72.67'/%3E%3Cpath d='M57.74 61.01L40.54 47.3'/%3E"
    "%3Cpath d='M64 98v14'/%3E%3Cpath d='M42 116h44'/%3E%3C/g%3E"
    "%3Cpath d='M24 34C24-4 104-4 104 34' fill='none' stroke='%23A93A26' stroke-width='7' stroke-linecap='round'/%3E"
    "%3C/svg%3E"
)

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="light dark">
<title>%(title)s</title>
<meta name="author" content="%(author)s">
<meta name="description" content="%(subject)s">
<link rel="icon" href="%(favicon)s" type="image/svg+xml">
<!--
  Generated from docs/c4-model.md by docs/build-html.py ("make docs").
  Edit the markdown, not this file.

  Set in the hand of the index page at https://home.peter.nz/ - the same
  tokens, the same pigments, the same marks.  Diagrams are the mermaid blocks
  pre-rendered to inline SVG, so the page is self contained and needs no
  network access.  The print stylesheet is what "make pdf" prints through: it
  decides the page breaks.
-->
<style>
%(css)s
</style>
</head>
<body>
<div class="sheet">

<header class="mast">
  <div class="eyebrow">
    <span>Home lab</span>
    <span><b>Seshat</b></span>
    <span>%(author)s</span>
    <span>Architecture record</span>
  </div>
  <div class="lockup">
    %(pi_mark)s
    <div>
      %(heading)s
      <div class="kind">C4 model &middot; context, containers, components, code</div>
    </div>
  </div>
</header>

<main>
%(body)s
</main>

<footer class="colophon">
  %(seshat_mark)s
  <div>
    <b>%(copyright)s</b> &middot; generated from <code>docs/c4-model.md</code>.
    Diagrams are pre-rendered to inline SVG, so this page works offline.
  </div>
</footer>

</div>
</body>
</html>
"""


def _is_wide(svg):
    """Is this diagram better off on a landscape page than a portrait one?"""
    box = re.search(r'viewBox="[-\d.]+ [-\d.]+ ([\d.]+) ([\d.]+)"', svg)
    if box is None:
        return False
    width, height = float(box.group(1)), float(box.group(2))
    return height > 0 and width >= WIDE_UNITS and width / height >= WIDE_RATIO


def assemble(body, diagrams):
    def substitute(match):
        index = int(match.group(1))
        variants = diagrams[index - 1]
        wide = " diagram--wide" if _is_wide(variants["light"]) else ""
        return ('<figure class="diagram%s">\n'
                '<div class="diagram-light">%s</div>\n'
                '<div class="diagram-dark">%s</div>\n'
                "</figure>" % (wide, variants["light"], variants["dark"]))

    body = re.sub(r"<p>@@MERMAID(\d+)@@</p>", substitute, body)
    if "@@MERMAID" in body:
        fail("a diagram placeholder was left behind - the markdown and the "
             "conversion disagree")

    # let wide tables scroll instead of stretching the page
    body = body.replace("<table>", '<div class="table-scroll"><table>')
    body = body.replace("</table>", "</table></div>")

    # the document's own title moves up into the lockup, beside the mark, the
    # way an entry in the works index is set
    heading = re.search(r"<h1[^>]*>.*?</h1>", body, re.S)
    if heading is None:
        fail("the markdown has no level-one heading to set beside the mark")
    body = body.replace(heading.group(0), "", 1)

    return PAGE % {
        "title": TITLE,
        "author": AUTHOR,
        "subject": SUBJECT,
        "copyright": COPYRIGHT,
        "favicon": FAVICON,
        "pi_mark": PI_MARK,
        "seshat_mark": SESHAT_MARK,
        "heading": heading.group(0),
        "css": CSS.strip(),
        "body": body.strip(),
    }


# ---------------------------------------------------------------- the pdf

#! Printed in the bottom margin of every page.  Chrome renders these templates
#! in their own document, so the styling has to be inline and the size stated
#! outright - it does not inherit a thing from the page being printed.
FOOTER = ("""
<div style="width:100%%; margin: 0 12mm; font-size: 7pt; color: #6E7874;
            font-family: ui-monospace, 'DejaVu Sans Mono', Menlo, Consolas, monospace;
            letter-spacing: 0.1em; text-transform: uppercase;
            display: flex; align-items: center; justify-content: space-between;">
  <span style="display: flex; align-items: center; gap: 6px;">
    <svg viewBox="0 0 128 128" style="width: 11px; height: 11px;">
      <g fill="none" stroke="#6E7874" stroke-width="9" stroke-linecap="round">
        <path d="M64 58V36"/><path d="M70.25 61.01L87.46 47.3"/><path d="M71.8 67.78L93.25 72.67"/>
        <path d="M67.47 73.21L77.02 93.03"/><path d="M60.53 73.21L50.98 93.03"/>
        <path d="M56.2 67.78L34.75 72.67"/><path d="M57.74 61.01L40.54 47.3"/>
        <path d="M64 98v14"/><path d="M42 116h44"/>
      </g>
      <path d="M24 34C24-4 104-4 104 34" fill="none" stroke="#A93A26" stroke-width="9" stroke-linecap="round"/>
    </svg>
    <span>%s</span>
  </span>
  <span class="title"></span>
  <span class="pageNumber"></span>
</div>
""" % COPYRIGHT).strip()


def _ps_string(text):
    """A postscript literal string, for pdfmark."""
    for char in ("\\", "(", ")"):
        text = text.replace(char, "\\" + char)
    return "(%s)" % text


class _Chrome(object):
    """Chrome driven over the devtools protocol, on a pipe.

    `--print-to-pdf` is the obvious way to do this, and on the chrome-for-
    testing builds this project already uses for mermaid it never returns.  So
    the same request is made directly: `--remote-debugging-pipe` puts the
    protocol on file descriptors 3 and 4, each message is json followed by a
    nul byte, and `Page.printToPDF` is what the switch would have called.

    No third-party module is involved, which keeps the pdf on exactly the
    dependencies the html already needs.
    """

    def __init__(self, browser, profile):
        self._to_read, to_write = os.pipe()
        read_from, self._to_write = os.pipe()

        def hand_over_the_pipe():
            """In the child: chrome reads the protocol on 3 and writes it on 4."""
            write_end = to_write
            # the read end is about to land on 3; move the write end first if
            # that is where it happens to be sitting
            if write_end == 3:
                write_end = os.dup(write_end)
            if read_from != 3:
                os.dup2(read_from, 3)
            if write_end != 4:
                os.dup2(write_end, 4)
            # a pipe from os.pipe() is close-on-exec, and dup2 onto a
            # descriptor that is already the right number does nothing at all -
            # including nothing about that flag.  Chrome would then start with
            # "remote debugging pipe file descriptors are not open".
            os.set_inheritable(3, True)
            os.set_inheritable(4, True)

        self._process = subprocess.Popen(
            [browser, "--headless=new", "--disable-gpu", "--no-sandbox",
             "--no-first-run", "--no-default-browser-check",
             "--user-data-dir=" + profile, "--remote-debugging-pipe"],
            preexec_fn=hand_over_the_pipe,
            # the descriptors above are handed over by number, which the
            # close-fds pass would undo
            close_fds=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        os.close(read_from)
        os.close(to_write)

        self._next_id = 0
        self._buffer = b""

    def send(self, method, params=None, session=None, timeout=120):
        self._next_id += 1
        message = {"id": self._next_id, "method": method, "params": params or {}}
        if session is not None:
            message["sessionId"] = session
        os.write(self._to_write, json.dumps(message).encode("utf-8") + b"\0")

        while True:
            answer = self._read(timeout)
            if answer.get("id") != self._next_id:
                continue
            if "error" in answer:
                fail("%s failed: %s" % (method, answer["error"].get("message")))
            return answer.get("result", {})

    def wait_for(self, method, timeout=120):
        while True:
            message = self._read(timeout)
            if message.get("method") == method:
                return message

    def _read(self, timeout):
        deadline = time.time() + timeout
        while b"\0" not in self._buffer:
            remaining = deadline - time.time()
            if remaining <= 0 or not select.select([self._to_read], [], [], remaining)[0]:
                fail("chrome stopped answering while printing")
            chunk = os.read(self._to_read, 65536)
            if not chunk:
                fail("chrome closed the protocol pipe while printing")
            self._buffer += chunk

        message, _, self._buffer = self._buffer.partition(b"\0")
        return json.loads(message.decode("utf-8"))

    def close(self):
        try:
            self.send("Browser.close", timeout=10)
        except (Exception, SystemExit):
            # shutting the browser down is never the reason a build fails: if
            # it will not go quietly it is killed, and whatever went wrong
            # before this point is what gets reported
            self._process.kill()
        finally:
            os.close(self._to_write)
            os.close(self._to_read)
            try:
                self._process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self._process.kill()


def build_pdf(browser):
    """Print the finished html, then stamp the document information on it.

    Chrome carries neither <title> nor <meta name=author> into the pdf, so the
    printed file is passed through ghostscript to set them.  Without gs the pdf
    is still written - it just arrives with chrome's own empty metadata.
    """
    if not os.path.exists(TARGET):
        fail("%s not found - run 'make docs' first" % os.path.basename(TARGET))

    sys.stdout.write("printing %s from %s\n"
                     % (os.path.basename(PDF_TARGET), os.path.basename(TARGET)))
    sys.stdout.write("  printing with %s\n" % os.path.basename(browser))

    work = tempfile.mkdtemp(prefix="pi-pdf-")
    try:
        printed = os.path.join(work, "printed.pdf")
        chrome = _Chrome(browser, os.path.join(work, "profile"))
        try:
            target = chrome.send("Target.createTarget", {"url": "about:blank"})
            session = chrome.send(
                "Target.attachToTarget",
                {"targetId": target["targetId"], "flatten": True},
            )["sessionId"]

            chrome.send("Page.enable", session=session)
            chrome.send("Page.navigate", {"url": "file://" + TARGET}, session=session)
            chrome.wait_for("Page.loadEventFired")

            # preferCSSPageSize is what lets @page decide the paper, including
            # the landscape pages the wide diagrams ask for.  The footer has to
            # be a print-job template rather than css: chrome does not
            # implement the @page margin boxes that would carry it.
            result = chrome.send(
                "Page.printToPDF",
                {
                    "printBackground": True,
                    "preferCSSPageSize": True,
                    "displayHeaderFooter": True,
                    # chrome wants both templates; an empty one keeps the top
                    # of the page clear
                    "headerTemplate": "<div></div>",
                    "footerTemplate": FOOTER,
                },
                session=session,
            )
        finally:
            chrome.close()

        with open(printed, "wb") as handle:
            handle.write(base64.b64decode(result["data"]))

        gs = shutil.which("gs")
        if gs is None:
            shutil.copyfile(printed, PDF_TARGET)
            sys.stderr.write("build-html: ghostscript not found - the pdf has "
                             "no title or author\n")
        else:
            marks = os.path.join(work, "docinfo.ps")
            with open(marks, "w") as handle:
                handle.write("[ /Title %s\n  /Author %s\n  /Subject %s\n"
                             "  /Creator %s\n  /DOCINFO pdfmark\n"
                             % (_ps_string(TITLE), _ps_string(AUTHOR),
                                _ps_string(SUBJECT),
                                _ps_string("docs/build-html.py")))
            run([gs, "-q", "-dNOPAUSE", "-dBATCH", "-dPreserveAnnots=true",
                 "-dCompatibilityLevel=1.7", "-sDEVICE=pdfwrite",
                 "-o", PDF_TARGET, printed, marks],
                "stamping the pdf")
    finally:
        shutil.rmtree(work, ignore_errors=True)

    sys.stdout.write("  wrote %s (%d bytes)\n"
                     % (os.path.basename(PDF_TARGET),
                        os.path.getsize(PDF_TARGET)))


def main():
    if "--pdf" in sys.argv[1:]:
        build_pdf(find_browser())
        return

    if not os.path.exists(SOURCE):
        fail("%s not found" % SOURCE)

    npx = find_npx()
    browser = find_browser()

    with open(SOURCE) as handle:
        markdown = handle.read()

    blocks, body = split_mermaid(markdown)
    sys.stdout.write("building %s from %s\n"
                     % (os.path.basename(TARGET), os.path.basename(SOURCE)))
    sys.stdout.write("  %d diagrams, rendering with %s\n"
                     % (len(blocks), os.path.basename(browser)))

    work = tempfile.mkdtemp(prefix="pi-docs-")
    try:
        diagrams = render_diagrams(blocks, work, npx, browser)
        html = assemble(markdown_to_html(body, work, npx), diagrams)
    finally:
        shutil.rmtree(work, ignore_errors=True)

    previous = None
    if os.path.exists(TARGET):
        with open(TARGET) as handle:
            previous = handle.read()

    with open(TARGET, "w") as handle:
        handle.write(html)

    if previous == html:
        sys.stdout.write("  %s is unchanged\n" % os.path.basename(TARGET))
    else:
        sys.stdout.write("  wrote %s (%d bytes)\n"
                         % (os.path.basename(TARGET), len(html)))


if __name__ == "__main__":
    main()
