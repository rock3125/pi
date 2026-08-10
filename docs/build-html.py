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
"""Build docs/c4-model.html from docs/c4-model.md.

    make docs

The mermaid blocks are pre-rendered to inline SVG rather than pulled from a
CDN at view time, so the finished page makes no network requests and reads
fine offline.  Each diagram is embedded twice, in mermaid's light and dark
themes, and swapped by prefers-color-scheme.

This is the only part of the project that needs anything beyond a C++
compiler, which is why it is a separate target and not part of "make":

    node and npx   to run marked and mermaid-cli
    a chrome build for mermaid-cli to render with

Neither is needed to build or run the interpreter.

Note that the output is not byte for byte reproducible: mermaid lays the
diagrams out through a headless browser, and the SVG path geometry shifts
very slightly between runs.  Regenerating an unchanged document therefore
still produces a small diff.  It is cosmetic.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "c4-model.md")
TARGET = os.path.join(HERE, "c4-model.html")

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


def render_diagrams(blocks, work, npx, browser):
    """Render every block to a light and a dark SVG."""
    config = os.path.join(work, "puppeteer.json")
    with open(config, "w") as handle:
        handle.write('{"executablePath": %s, "args": ["--no-sandbox",'
                     ' "--disable-dev-shm-usage"]}' % _json_string(browser))

    rendered = []
    for index, block in enumerate(blocks, 1):
        source = os.path.join(work, "d%d.mmd" % index)
        with open(source, "w") as handle:
            handle.write(block)

        variants = {}
        for variant, theme in (("light", "default"), ("dark", "dark")):
            output = os.path.join(work, "d%d-%s.svg" % (index, variant))
            run([npx, "-y", MERMAID, "-p", config, "-b", "transparent",
                 "-t", theme, "-i", source, "-o", output],
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
:root {
  --bg: #ffffff;
  --fg: #1f2328;
  --muted: #59636e;
  --rule: #d1d9e0;
  --accent: #0969da;
  --code-bg: #f6f8fa;
  --card: #f6f8fa;
  --quote: #8250df;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #0d1117;
    --fg: #e6edf3;
    --muted: #9198a1;
    --rule: #3d444d;
    --accent: #4493f8;
    --code-bg: #151b23;
    --card: #151b23;
    --quote: #ab7df8;
  }
}
:root[data-theme="light"] {
  --bg: #ffffff; --fg: #1f2328; --muted: #59636e; --rule: #d1d9e0;
  --accent: #0969da; --code-bg: #f6f8fa; --card: #f6f8fa; --quote: #8250df;
}
:root[data-theme="dark"] {
  --bg: #0d1117; --fg: #e6edf3; --muted: #9198a1; --rule: #3d444d;
  --accent: #4493f8; --code-bg: #151b23; --card: #151b23; --quote: #ab7df8;
}

* { box-sizing: border-box; }

body {
  margin: 0;
  padding: 3rem 1.25rem 6rem;
  background: var(--bg);
  color: var(--fg);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans",
               Helvetica, Arial, sans-serif;
  font-size: 16px;
  line-height: 1.6;
  -webkit-text-size-adjust: 100%;
}

main { max-width: 60rem; margin: 0 auto; }

h1, h2, h3, h4 { line-height: 1.25; font-weight: 600; margin: 2.2rem 0 1rem; }
h1 { font-size: 2rem; margin-top: 0; padding-bottom: .4rem; border-bottom: 1px solid var(--rule); }
h2 { font-size: 1.45rem; padding-bottom: .3rem; border-bottom: 1px solid var(--rule); }
h3 { font-size: 1.15rem; }
h4 { font-size: 1rem; color: var(--muted); }

p, ul, ol { margin: 0 0 1rem; }
li { margin: .25rem 0; }

a { color: var(--accent); text-decoration: none; }
a:hover { text-decoration: underline; }

hr { border: 0; border-top: 1px solid var(--rule); margin: 2.5rem 0; }

code {
  font-family: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas,
               "Liberation Mono", monospace;
  font-size: .875em;
  background: var(--code-bg);
  padding: .15em .4em;
  border-radius: 6px;
}
pre {
  background: var(--code-bg);
  border: 1px solid var(--rule);
  border-radius: 8px;
  padding: 1rem;
  overflow-x: auto;
}
pre code { background: none; padding: 0; font-size: .85em; }

blockquote {
  margin: 1rem 0;
  padding: .1rem 1rem;
  border-left: 4px solid var(--quote);
  color: var(--muted);
  background: var(--card);
  border-radius: 0 6px 6px 0;
}
blockquote p { margin: .8rem 0; }

/* tables scroll rather than push the page sideways */
.table-scroll { overflow-x: auto; margin: 0 0 1.25rem; }
table { border-collapse: collapse; width: 100%; font-size: .92rem; }
th, td { border: 1px solid var(--rule); padding: .5rem .75rem; text-align: left; vertical-align: top; }
th { background: var(--card); font-weight: 600; }
tbody tr:nth-child(even) { background: color-mix(in srgb, var(--card) 55%, transparent); }

figure.diagram {
  margin: 1.5rem 0 2rem;
  padding: 1.25rem;
  background: var(--card);
  border: 1px solid var(--rule);
  border-radius: 10px;
  overflow-x: auto;
}
figure.diagram svg { display: block; margin: 0 auto; height: auto; max-width: 100%; }

/* one variant at a time, following the page theme */
.diagram-dark { display: none; }
@media (prefers-color-scheme: dark) {
  .diagram-light { display: none; }
  .diagram-dark { display: block; }
}
:root[data-theme="light"] .diagram-light { display: block; }
:root[data-theme="light"] .diagram-dark { display: none; }
:root[data-theme="dark"] .diagram-light { display: none; }
:root[data-theme="dark"] .diagram-dark { display: block; }

.source-note {
  max-width: 60rem;
  margin: 0 auto 2rem;
  color: var(--muted);
  font-size: .85rem;
}

@media (max-width: 40rem) {
  body { padding: 1.5rem .9rem 4rem; }
  h1 { font-size: 1.6rem; }
  h2 { font-size: 1.25rem; }
}
"""

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PI Prolog Interpreter - C4 model</title>
<meta name="description" content="C4 architecture model of the PI Prolog interpreter: context, containers, components and code.">
<!--
  Generated from docs/c4-model.md by docs/build-html.py ("make docs").
  Edit the markdown, not this file.

  Diagrams are the mermaid blocks pre-rendered to inline SVG, so the page is
  self contained and needs no network access.
-->
<style>
%s
</style>
</head>
<body>
<main>
%s
</main>
<p class="source-note">Generated from <code>docs/c4-model.md</code>. Diagrams are pre-rendered to inline SVG, so this page works offline.</p>
</body>
</html>
"""


def assemble(body, diagrams):
    def substitute(match):
        index = int(match.group(1))
        variants = diagrams[index - 1]
        return ('<figure class="diagram">\n'
                '<div class="diagram-light">%s</div>\n'
                '<div class="diagram-dark">%s</div>\n'
                "</figure>" % (variants["light"], variants["dark"]))

    body = re.sub(r"<p>@@MERMAID(\d+)@@</p>", substitute, body)
    if "@@MERMAID" in body:
        fail("a diagram placeholder was left behind - the markdown and the "
             "conversion disagree")

    # let wide tables scroll instead of stretching the page
    body = body.replace("<table>", '<div class="table-scroll"><table>')
    body = body.replace("</table>", "</table></div>")

    return PAGE % (CSS.strip(), body.strip())


def main():
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
