#!/usr/bin/env python3
"""
TCP server tests for the PI prolog interpreter.

Starts ./prolog with --port, then drives it over sockets: single commands,
several commands down one connection, many clients at once, and a check
that the prompt and the network share one database.

Run from the directory holding the ./prolog binary:

    python3 tests/test_server.py
"""

import os
import pty
import socket
import subprocess
import sys
import threading
import time

PROLOG = "./prolog"
PROGRAM = "samples/family.pl"
PORT = 8137

PASS = 0
FAIL = 0


def report(name, ok, detail=""):
    global PASS, FAIL
    if ok:
        PASS += 1
    else:
        FAIL += 1
        print("FAIL: %s" % name)
        if detail:
            print("      %s" % detail)


class Server:
    """Runs the interpreter with a pty on stdin so the prompt stays alive."""

    def __init__(self, *args):
        self.master, slave = pty.openpty()
        self.proc = subprocess.Popen(
            [PROLOG] + list(args),
            stdin=slave, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        os.close(slave)
        self._wait_for_port()

    def _wait_for_port(self, timeout=10.0):
        end = time.time() + timeout
        while time.time() < end:
            try:
                with socket.create_connection(("127.0.0.1", PORT), 0.3):
                    return
            except OSError:
                time.sleep(0.05)
        raise RuntimeError("server never started listening on %d" % PORT)

    def prompt(self, line):
        """Type a command at the interactive prompt."""
        os.write(self.master, (line + "\n").encode())
        time.sleep(0.4)

    def stop(self):
        try:
            os.write(self.master, b"exit\n")
        except OSError:
            pass
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(timeout=5)
            return False
        finally:
            try:
                os.close(self.master)
            except OSError:
                pass
        return True


def ask(commands, timeout=10.0):
    """Send commands down one connection, return everything sent back."""
    s = socket.create_connection(("127.0.0.1", PORT), timeout)
    s.settimeout(timeout)
    payload = "".join(c + "\n" for c in commands)
    s.sendall(payload.encode())
    s.shutdown(socket.SHUT_WR)

    out = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            out += chunk
    except socket.timeout:
        pass
    finally:
        s.close()
    return out.decode("utf-8", "replace")


# ---------------------------------------------------------------- run

server = Server("--port", str(PORT), PROGRAM)
try:
    # a single query
    t = ask(["?- father(fred,X)."])
    report("single query over TCP", "X=peter" in t and "X=jj" in t, repr(t))

    # a failing query
    t = ask(["?- father(fred,zoe)."])
    report("failing query answers no", "no" in t, repr(t))

    # several commands down one connection, in order
    t = ask(["?- half(peter,X).", "?- father(fred,peter)."])
    report("several commands, one connection",
           "X=micheal" in t and "yes" in t, repr(t))

    # a client can add to the database and query it back
    t = ask(["likes(pete,prolog).", "?- likes(pete,X)."])
    report("client can assert and query", "X=prolog" in t, repr(t))

    # interpreter commands work too
    t = ask(["list"])
    report("list works over TCP", "father(fred,peter)" in t, repr(t))

    t = ask(["help"])
    report("help works over TCP", "allowed commands" in t, repr(t))

    # a syntax error comes back as text rather than killing the connection
    t = ask(["?- father(fred,X)", "?- father(fred,peter)."])
    report("parse error is reported and the connection survives",
           "error parsing query" in t and "yes" in t, repr(t))

    # "exit" closes that client only - the next connection still works
    t = ask(["exit"])
    t2 = ask(["?- father(fred,peter)."])
    report("client exit does not stop the server", "yes" in t2, repr(t2))

    # the prompt and the network share one database
    server.prompt("server_side_fact(hello).")
    t = ask(["?- server_side_fact(X)."])
    report("prompt and clients share the database", "X=hello" in t, repr(t))

    # ...and the other way round
    t = ask(["client_side_fact(world)."])
    t = ask(["?- client_side_fact(X)."])
    report("a client's facts persist for later clients", "X=world" in t, repr(t))

    # many clients at once - the engine is serialised behind a lock, so this
    # is really a check that concurrency does not corrupt or deadlock it
    results = []
    errors = []
    lock = threading.Lock()

    def worker(n):
        try:
            got = ask(["?- half(peter,X).", "?- father(fred,X)."], timeout=30)
            with lock:
                results.append(got)
        except Exception as exc:                      # noqa: BLE001
            with lock:
                errors.append("%s: %r" % (n, exc))

    threads = [threading.Thread(target=worker, args=(i,)) for i in range(20)]
    for t_ in threads:
        t_.start()
    for t_ in threads:
        t_.join(60)

    ok = (not errors) and len(results) == 20
    if ok:
        ok = all(("X=micheal" in r and "X=jj" in r and "X=peter" in r)
                 for r in results)
    report("20 concurrent clients all get correct answers", ok,
           "errors=%s got=%d" % (errors[:3], len(results)))

    # the interpreter is still healthy afterwards
    t = ask(["?- father(fred,X)."])
    report("engine still correct after concurrent load",
           "X=peter" in t and "X=jj" in t, repr(t))

finally:
    clean = server.stop()
    report("server shuts down cleanly on exit", clean,
           "process had to be killed")

print()
print("server: passed: %d   failed: %d" % (PASS, FAIL))
sys.exit(1 if FAIL else 0)
