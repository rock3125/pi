(() => {
  "use strict";

  const familyProgram = `father(fred,peter).
father(fred,mark).
father(fred,micheal).
father(fred,jj).
mother(anne,peter).
mother(anne,mark).
mother(frieda,micheal).
mother(frieda,jj).
different(X,Y) :- X \\= Y.
half(X,Y) :- father(Z,X), father(Z,Y), mother(K,X), mother(L,Y), different(K,L).`;

  const output = document.querySelector("#output");
  const terminal = document.querySelector("#terminal");
  const form = document.querySelector("#repl");
  const command = document.querySelector("#command");
  const runButton = form.querySelector("button");
  const status = document.querySelector("#status");
  const fileInput = document.querySelector("#file");
  const controls = ["#example", "#upload", "#clear"].map((id) => document.querySelector(id));
  const history = [];
  let historyIndex = 0;
  let prolog;

  function append(text, className = "result") {
    const block = document.createElement("pre");
    block.className = className;
    block.textContent = text;
    output.append(block);
    terminal.scrollTop = terminal.scrollHeight;
  }

  function resizeInput() {
    command.style.height = "auto";
    command.style.height = `${Math.min(command.scrollHeight, 180)}px`;
  }

  function run(source) {
    const text = source.trim();
    if (!text || !prolog) return;
    append(`> ${text}`, "input");
    const result = prolog.execute(text);
    if (result) append(result.replace(/\n$/, ""));
    history.push(text);
    historyIndex = history.length;
    command.value = "";
    resizeInput();
  }

  form.addEventListener("submit", (event) => {
    event.preventDefault();
    run(command.value);
  });

  command.addEventListener("input", resizeInput);
  command.addEventListener("keydown", (event) => {
    if (event.key === "Enter" && !event.shiftKey) {
      event.preventDefault();
      form.requestSubmit();
    } else if (event.key === "ArrowUp" && command.value.indexOf("\n") === -1) {
      if (historyIndex > 0) historyIndex -= 1;
      if (history[historyIndex] !== undefined) command.value = history[historyIndex];
      resizeInput();
      event.preventDefault();
    } else if (event.key === "ArrowDown" && command.value.indexOf("\n") === -1) {
      if (historyIndex < history.length) historyIndex += 1;
      command.value = history[historyIndex] || "";
      resizeInput();
      event.preventDefault();
    }
  });

  document.querySelector("#example").addEventListener("click", () => {
    const result = prolog.loadProgram(familyProgram);
    append("> [loaded family example]", "input");
    if (result) append(result.replace(/\n$/, ""));
    command.value = "?- father(fred,X).";
    resizeInput();
    command.focus();
  });

  document.querySelector("#upload").addEventListener("click", () => fileInput.click());
  fileInput.addEventListener("change", async () => {
    const file = fileInput.files[0];
    if (!file) return;
    const result = prolog.loadProgram(await file.text());
    append(`> [loaded ${file.name}]`, "input");
    if (result) append(result.replace(/\n$/, ""));
    fileInput.value = "";
    command.focus();
  });

  document.querySelector("#clear").addEventListener("click", () => append(prolog.reset().trim()));
  document.querySelector("#clear-screen").addEventListener("click", () => {
    output.replaceChildren();
    command.focus();
  });

  async function start() {
    try {
      const module = await createPiModule({
        locateFile: (path) => path,
        printErr: (text) => append(text, "error")
      });
      prolog = new module.PiProlog();
      command.disabled = false;
      runButton.disabled = false;
      controls.forEach((control) => { control.disabled = false; });
      status.textContent = "Ready · runs locally";
      status.classList.add("ready");
      append("PI Prolog Interpreter (WASM)\ntype 'help' for a list of possible commands");
      command.focus();
    } catch (error) {
      status.textContent = "Engine failed to load";
      status.classList.add("failed");
      append(`Could not start WebAssembly: ${error.message}\nBuild with 'make' in the wasm directory and serve over HTTP.`, "error");
    }
  }

  window.addEventListener("beforeunload", () => {
    if (prolog) prolog.delete();
  });

  start();
})();
