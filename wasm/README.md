# PI Prolog in WebAssembly

This directory builds the C++ interpreter core into WebAssembly and supplies a
browser REPL. Queries and database changes happen entirely in the page; no
server is involved.

## Build and run

Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html),
then from this directory run:

```sh
make
make serve
```

Open <http://localhost:8080>. A web server is required because browsers do not
reliably load `.wasm` from a `file://` page.

`make` generates `pi.js` and `pi.wasm`. The checked-in browser files are
`index.html`, `app.js`, and `styles.css`; the adapter in `pi_wasm.cpp` exposes
the existing C++ engine through Embind.

The **Open .pl file** button reads a local Prolog source file through the
browser and sends its text to the engine. It does not upload the file. The
native `load <filename>` command cannot address the user's filesystem from a
browser, so use that button instead.

## JavaScript API

After `createPiModule()` resolves, create `new module.PiProlog()` and call:

- `execute(command)` — run a query, command, or statements and return output.
- `loadProgram(source)` — add a complete Prolog source string.
- `reset()` — clear the engine and database.

Call `delete()` on the object when it is no longer needed.
