#!/usr/bin/env python3
"""
Interactive line-editor tests for the PI prolog interpreter.

The editor only turns itself on when stdin is a terminal, so these run the
interpreter under a pty, send real key sequences, and check that the line
which finally got executed was the one the editing keys should have produced.

Run from the directory holding the ./prolog binary:

    python3 tests/test_interactive.py
"""

import os
import pty
import re
import select
import shutil
import sys
import tempfile
import time

PROLOG = "./prolog"
PROGRAM = "samples/family.pl"

# key sequences, as a terminal actually sends them
UP         = b"\x1b[A"
DOWN       = b"\x1b[B"
LEFT       = b"\x1b[D"
RIGHT      = b"\x1b[C"
HOME       = b"\x1b[H"
HOME_TILDE = b"\x1b[1~"
END        = b"\x1b[F"
END_TILDE  = b"\x1b[4~"
DELETE     = b"\x1b[3~"
CTRL_LEFT  = b"\x1b[1;5D"
CTRL_RIGHT = b"\x1b[1;5C"
ALT_B      = b"\x1bb"
ALT_F      = b"\x1bf"
CTRL_A     = b"\x01"
CTRL_B     = b"\x02"
CTRL_C     = b"\x03"
CTRL_D     = b"\x04"
CTRL_E     = b"\x05"
CTRL_F     = b"\x06"
CTRL_K     = b"\x0b"
CTRL_T     = b"\x14"
CTRL_U     = b"\x15"
CTRL_W     = b"\x17"
BACKSPACE  = b"\x7f"
ALT_BS     = b"\x1b\x7f"
CR         = b"\r"

ANSI = re.compile(rb"\x1b\[[0-9;]*[A-Za-z]|\x1b[HO@-Z\\-_]")


def session(keys, args=(), home=None, timeout=6.0):
    """Run the interpreter under a pty, send keys, return decoded output."""
    pid, fd = pty.fork()
    if pid == 0:
        env = dict(os.environ)
        env["TERM"] = "xterm"
        if home is not None:
            env["HOME"] = home
        try:
            os.execve(PROLOG, [PROLOG] + list(args), env)
        except Exception:
            os._exit(127)

    out = bytearray()

    def drain(wait):
        end = time.time() + wait
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.05)
            if not r:
                continue
            try:
                data = os.read(fd, 65536)
            except OSError:
                return False
            if not data:
                return False
            out.extend(data)
        return True

    drain(0.35)
    for chunk in keys:
        try:
            os.write(fd, chunk)
        except OSError:
            break
        drain(0.18)

    # let it finish
    deadline = time.time() + timeout
    while time.time() < deadline:
        r, _, _ = select.select([fd], [], [], 0.2)
        if not r:
            break
        try:
            data = os.read(fd, 65536)
        except OSError:
            break
        if not data:
            break
        out.extend(data)

    try:
        os.close(fd)
    except OSError:
        pass
    try:
        os.waitpid(pid, 0)
    except ChildProcessError:
        pass

    clean = ANSI.sub(b"", bytes(out))
    return clean.decode("utf-8", "replace")


PASS = 0
FAIL = 0


def check(name, keys, expect, absent=None, args=(), home=None):
    global PASS, FAIL
    text = session(keys, args=args, home=home)
    ok = expect in text
    if ok and absent is not None:
        ok = absent not in text
    if ok:
        PASS += 1
    else:
        FAIL += 1
        print("FAIL: %s" % name)
        print("      expected to find: %r" % expect)
        if absent:
            print("      and NOT to find:  %r" % absent)
        print("      got: %r" % text[-400:])


EXIT = [b"exit", CR]
FAMILY = (PROGRAM,)

# ---------------------------------------------------------------- basics

check("plain typing still works",
      [b"?- father(fred,peter).", CR] + EXIT,
      "yes", args=FAMILY)

# ---------------------------------------------------------------- history

check("up arrow recalls the previous command",
      [b"?- half(peter,X).", CR, UP, CR] + EXIT,
      "X=micheal", args=FAMILY)

check("up arrow twice reaches the older command",
      [b"?- half(peter,X).", CR,
       b"?- father(fred,peter).", CR,
       UP, UP, CR] + EXIT,
      "X=micheal", args=FAMILY)

check("down arrow comes back to the newer command",
      [b"?- half(peter,X).", CR,
       b"?- father(fred,zoe).", CR,
       UP, UP, DOWN, CR] + EXIT,
      "no", args=FAMILY)

check("ctrl-p / ctrl-n walk the history too",
      [b"?- half(peter,X).", CR, b"\x10", CR] + EXIT,
      "X=micheal", args=FAMILY)

# ---------------------------------------------------------- word deletion

# "?- father(fred,WRONG." -> ctrl-w drops back to the space after "?-"
check("ctrl-w deletes the word before the cursor",
      [b"?- father(fred,WRONG.", CTRL_W, b"father(fred,peter).", CR] + EXIT,
      "yes", absent="error", args=FAMILY)

# alt-backspace stops at punctuation, so it eats "xyz" and leaves the "."
check("alt-backspace deletes a word",
      [b"?- father(fred,peter).xyz", ALT_BS, CR] + EXIT,
      "yes", absent="error", args=FAMILY)

# ------------------------------------------------------------ line motion

check("home lets you type at the start of the line",
      [b"father(fred,peter).", HOME, b"?- ", CR] + EXIT,
      "yes", absent="error", args=FAMILY)

check("home via ESC[1~ works",
      [b"father(fred,peter).", HOME_TILDE, b"?- ", CR] + EXIT,
      "yes", absent="error", args=FAMILY)

check("ctrl-a behaves like home",
      [b"father(fred,peter).", CTRL_A, b"?- ", CR] + EXIT,
      "yes", absent="error", args=FAMILY)

check("end returns to the end of the line",
      [b"father(fred,peter)", HOME, b"?- ", END, b".", CR] + EXIT,
      "yes", absent="error", args=FAMILY)

check("end via ESC[4~ works",
      [b"father(fred,peter)", HOME, b"?- ", END_TILDE, b".", CR] + EXIT,
      "yes", absent="error", args=FAMILY)

check("ctrl-e behaves like end",
      [b"father(fred,peter)", CTRL_A, b"?- ", CTRL_E, b".", CR] + EXIT,
      "yes", absent="error", args=FAMILY)

# --------------------------------------------------------- cursor editing

# "?- father(fred,pete)." with the cursor moved left twice sits before ")"
check("left arrow moves the cursor for insertion",
      [b"?- father(fred,pete).", LEFT, LEFT, b"r", CR] + EXIT,
      "yes", absent="error", args=FAMILY)

check("right arrow moves back again",
      [b"?- father(fred,peter).", CTRL_A, RIGHT, RIGHT, RIGHT, RIGHT, CR] + EXIT,
      "yes", absent="error", args=FAMILY)

check("delete key removes the character under the cursor",
      [b"X?- father(fred,peter).", HOME, DELETE, CR] + EXIT,
      "yes", absent="error", args=FAMILY)

check("backspace removes the character before the cursor",
      [b"?- father(fred,peter).X", BACKSPACE, CR] + EXIT,
      "yes", absent="error", args=FAMILY)

check("ctrl-t swaps the last two characters",
      [b"?- father(fred,peter).", LEFT, LEFT, LEFT, CTRL_T, RIGHT, RIGHT, CR] + EXIT,
      "no", args=FAMILY)

# ------------------------------------------------------------ line killing

check("ctrl-u clears back to the start",
      [b"total rubbish", CTRL_U, b"?- father(fred,peter).", CR] + EXIT,
      "yes", absent="error", args=FAMILY)

check("ctrl-k clears to the end",
      [b"?- father(fred,peter).RUBBISH", CTRL_A,
       RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT,
       RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT, RIGHT,
       RIGHT, RIGHT, CTRL_K, CR] + EXIT,
      "yes", absent="error", args=FAMILY)

# -------------------------------------------------------------- word jumps

# two words back from the end is the start of "fred", so the X lands there
# and turns it into the variable Xfred - which then binds to fred
check("ctrl-left jumps back a word",
      [b"?- father(fred,peter).", CTRL_LEFT, CTRL_LEFT, b"X", CR] + EXIT,
      "Xfred=fred", args=FAMILY)

# two words in from the start is just past "fred", giving father(fredX,peter)
check("ctrl-right jumps forward a word",
      [b"?- father(fred,peter).", CTRL_A, CTRL_RIGHT, CTRL_RIGHT, b"X", CR] + EXIT,
      "no", absent="error", args=FAMILY)

check("alt-b and alt-f jump words",
      [b"?- father(fred,peter).", ALT_B, ALT_F, b"", CR] + EXIT,
      "yes", absent="error", args=FAMILY)

# ------------------------------------------------------------------ ctrl-c

check("ctrl-c abandons the line without running it",
      [b"?- father(fred,zoe).", CTRL_C, b"?- father(fred,peter).", CR] + EXIT,
      "yes", absent="no", args=FAMILY)

# ------------------------------------------------------------------ ctrl-d

check("ctrl-d on an empty line exits",
      [b"?- father(fred,peter).", CR, CTRL_D],
      "yes", args=FAMILY)

check("ctrl-d mid line deletes forward",
      [b"X?- father(fred,peter).", CTRL_A, CTRL_D, CR] + EXIT,
      "yes", absent="error", args=FAMILY)

# ------------------------------------------------------- history is durable

home = tempfile.mkdtemp(prefix="pi-hist-")
try:
    session([b"?- half(peter,X).", CR] + EXIT, args=FAMILY, home=home)

    hist = os.path.join(home, ".pi_history")
    if os.path.exists(hist):
        PASS += 1
    else:
        FAIL += 1
        print("FAIL: history file written to $HOME/.pi_history")

    # "exit" goes into the history too, the way bash records it, so the
    # query is one step further back
    check("history survives a restart",
          [UP, UP, CR] + EXIT,
          "X=micheal", args=FAMILY, home=home)
finally:
    shutil.rmtree(home, ignore_errors=True)

# --------------------------------------------------------------------------

print()
print("interactive: passed: %d   failed: %d" % (PASS, FAIL))
sys.exit(1 if FAIL else 0)
