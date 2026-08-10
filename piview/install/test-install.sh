#!/usr/bin/env bash
#
# Exercise the installer's logic on each supported distribution.
#
#   ./piview/install/test-install.sh                 all three distributions
#   ./piview/install/test-install.sh ubuntu:24.04    just one
#
# docker, systemctl, curl and ss are stubbed inside the container, so this
# checks what install.sh decides and where it puts things - not whether the
# images run. It needs a docker daemon on this host to provide the containers,
# and it never touches this machine's own /opt, /etc or systemd.
#
# The end-to-end test is a separate exercise: a privileged container running
# systemd as pid 1 with its own docker daemon. This one is the fast check.

set -euo pipefail

HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
DIST="$HERE/dist"

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    B=$'\033[1m'; G=$'\033[32m'; R=$'\033[31m'; N=$'\033[0m'
else
    B=""; G=""; R=""; N=""
fi

DISTROS=("$@")
[ ${#DISTROS[@]} -gt 0 ] || DISTROS=(ubuntu:24.04 debian:13 fedora:44)

command -v docker >/dev/null || { printf 'error: docker is needed to run the tests\n' >&2; exit 1; }

# Always test the package as shipped, so the build is part of what is verified.
printf '%s==>%s Building the package under test\n' "$B" "$N"
"$HERE/build-installer.sh" >/dev/null
TARBALL=$(ls -1t "$DIST"/piview-*-installer.tar.gz | head -1)
[ -n "$TARBALL" ] || { printf 'error: no package in %s\n' "$DIST" >&2; exit 1; }
printf '  %s\n\n' "$(basename "$TARBALL")"

# ---------------------------------------------------------------- the guest

# Written to a file and mounted, rather than piped to bash, so a failure
# reports a real line number.
GUEST=$(mktemp); trap 'rm -f "$GUEST"' EXIT
cat >"$GUEST" <<'GUEST_EOF'
#!/bin/bash
set -uo pipefail

PASS=0; FAIL=0
if [ -t 1 ]; then G=$'\033[32m'; R=$'\033[31m'; N=$'\033[0m'; else G=""; R=""; N=""; fi
check() {  # check <description> <test args...>
    local desc=$1; shift
    if "$@"; then printf '  %s✓%s %s\n' "$G" "$N" "$desc"; PASS=$((PASS+1))
    else          printf '  %s✗%s %s\n' "$R" "$N" "$desc"; FAIL=$((FAIL+1)); fi
}
nocheck() { # negated form, so the message still reads positively
    local desc=$1; shift
    if ! "$@"; then printf '  %s✓%s %s\n' "$G" "$N" "$desc"; PASS=$((PASS+1))
    else            printf '  %s✗%s %s\n' "$R" "$N" "$desc"; FAIL=$((FAIL+1)); fi
}
hasline() { grep -qE "$2" "$1"; }

# ---------------------------------------------------------------- stubs
#
# /usr/local/bin is where install.sh puts piviewctl, so the stubs live in a
# directory ahead of it on PATH and cannot be confused with the real thing.
STUB=/opt/stub; mkdir -p "$STUB"; export PATH="$STUB:$PATH"
CALLS=/tmp/docker-calls; : >"$CALLS"

cat >"$STUB/docker" <<'EOF'
#!/bin/bash
echo "$*" >> /tmp/docker-calls
case "$1 ${2:-}" in
  # no images until a build has been recorded, so BUILD=auto takes the
  # build path the way it would on a fresh server
  "image inspect")
      grep -q "^compose build" /tmp/docker-calls && exit 0 || exit 1 ;;
  "version --format") echo "27.0.0"; exit 0 ;;
  "compose version")  [ "${3:-}" = "--short" ] && echo "2.29.0" || echo "Docker Compose version v2.29.0"; exit 0 ;;
  "info ") exit 0 ;;
esac
exit 0
EOF

cat >"$STUB/systemctl" <<'EOF'
#!/bin/bash
echo "systemctl $*" >> /tmp/docker-calls
case "$1" in
  is-active)        echo inactive; exit 3 ;;   # never already running
  list-unit-files)  exit 0 ;;
esac
exit 0
EOF

# the readiness probe; succeeding exercises the whole start path
printf '#!/bin/bash\nexit 0\n' > "$STUB/curl"
chmod +x "$STUB/docker" "$STUB/systemctl" "$STUB/curl"

# ---------------------------------------------------------------- unpack

cd /tmp
tar xzf /pkg/*.tar.gz
cd piview-*-installer/
VERSION=$(cat VERSION)

printf '\n--- fresh install ---\n'
./install.sh >/tmp/out1 2>&1 || { printf '%sinstall.sh failed:%s\n' "$R" "$N"; cat /tmp/out1; exit 1; }

check "stack in /opt/piview"                    test -f /opt/piview/docker-compose.yml
check "interpreter sources copied"              test -d /opt/piview/src
check "server sources copied"                   test -d /opt/piview/piview/server
check "config written"                          test -f /etc/piview/piview.env
check "config records the package version"      hasline /etc/piview/piview.env "^PIVIEW_VERSION=${VERSION}$"
check "programs dir created"                    test -d /var/lib/piview/programs
check "samples seeded"                          test -f /var/lib/piview/programs/family.pl
check "unit installed"                          test -f /etc/systemd/system/piview.service
nocheck "unit fully templated"                  grep -q '__[A-Z_]*__' /etc/systemd/system/piview.service
check "unit points at the prefix"               hasline /etc/systemd/system/piview.service "^WorkingDirectory=/opt/piview$"
check "unit reads the config"                   hasline /etc/systemd/system/piview.service "^EnvironmentFile=/etc/piview/piview.env$"
check "piviewctl installed"                     test -x /usr/local/bin/piviewctl
check "piviewctl knows the prefix"              hasline /usr/local/bin/piviewctl "^PREFIX=/opt/piview$"
check ".env symlinked to the config"            test "$(readlink -f /opt/piview/.env)" = /etc/piview/piview.env
check "images were built"                       grep -q "^compose build" "$CALLS"
check "unit was enabled"                        grep -q "systemctl enable piview.service" "$CALLS"
check "service was started"                     grep -q "systemctl start piview.service" "$CALLS"

printf '\n--- upgrade keeps what the operator wrote ---\n'
# the operator edits the config and adds a program of their own
sed -i 's|^PIVIEW_PORT=.*|PIVIEW_PORT=9999|' /etc/piview/piview.env
echo '# my note' >> /etc/piview/piview.env
echo 'mine(yes).' > /var/lib/piview/programs/mine.pl
echo 'edited(true).' > /var/lib/piview/programs/family.pl

sed -i 's/^1\.0\.0$/1.1.0/' VERSION 2>/dev/null || echo 1.1.0 > VERSION
NEWVER=$(cat VERSION)
./install.sh >/tmp/out2 2>&1 || { printf '%supgrade failed:%s\n' "$R" "$N"; cat /tmp/out2; exit 1; }

check "edited port survives"                    hasline /etc/piview/piview.env "^PIVIEW_PORT=9999$"
check "operator's comment survives"             hasline /etc/piview/piview.env "^# my note$"
check "version line follows the package"        hasline /etc/piview/piview.env "^PIVIEW_VERSION=${NEWVER}$"
nocheck "no duplicate version line"             test "$(grep -c '^PIVIEW_VERSION=' /etc/piview/piview.env)" -gt 1
check "the operator's program is untouched"     test -f /var/lib/piview/programs/mine.pl
check "an edited sample is not overwritten"     hasline /var/lib/piview/programs/family.pl "^edited\(true\)\.$"
check "the running service was stopped first"   grep -q "systemctl stop piview.service" "$CALLS"

printf '\n--- overrides ---\n'
./install.sh --port 8123 --bind 0.0.0.0 --no-start >/tmp/out3 2>&1 || { cat /tmp/out3; exit 1; }
check "--port applied"                          hasline /etc/piview/piview.env "^PIVIEW_PORT=8123$"
check "--bind applied"                          hasline /etc/piview/piview.env "^PIVIEW_BIND=0.0.0.0$"
check "--bind off loopback warns"               grep -qi "no authentication" /tmp/out3
check "--no-start did not start"                test "$(grep -c 'systemctl start piview.service' "$CALLS")" -eq 2

printf '\n--- uninstall keeps the data ---\n'
/opt/piview/uninstall.sh -y >/tmp/out4 2>&1 || { cat /tmp/out4; exit 1; }
nocheck "prefix removed"                        test -e /opt/piview
nocheck "unit removed"                          test -e /etc/systemd/system/piview.service
nocheck "piviewctl removed"                     test -e /usr/local/bin/piviewctl
check "config kept"                             test -f /etc/piview/piview.env
check "programs kept"                           test -f /var/lib/piview/programs/mine.pl

printf '\n--- purge takes everything ---\n'
./install.sh --no-start >/tmp/out5 2>&1 || { cat /tmp/out5; exit 1; }
/opt/piview/uninstall.sh -y --purge >/tmp/out6 2>&1 || { cat /tmp/out6; exit 1; }
nocheck "config dir removed"                    test -e /etc/piview
nocheck "state dir removed"                     test -e /var/lib/piview

printf '\n%s: %d passed, %d failed\n' "$(. /etc/os-release; echo "$PRETTY_NAME")" "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
GUEST_EOF
chmod +x "$GUEST"

# ---------------------------------------------------------------- run

PKGDIR=$(mktemp -d); trap 'rm -f "$GUEST"; rm -rf "$PKGDIR"' EXIT
cp "$TARBALL" "$PKGDIR/"

rc=0
for image in "${DISTROS[@]}"; do
    printf '%s==>%s %s\n' "$B" "$N" "$image"
    if docker run --rm \
        -v "$PKGDIR":/pkg:ro \
        -v "$GUEST":/guest.sh:ro \
        "$image" /guest.sh
    then
        printf '%s    %s ok%s\n\n' "$G" "$image" "$N"
    else
        printf '%s    %s FAILED%s\n\n' "$R" "$image" "$N"
        rc=1
    fi
done

if [ "$rc" = "0" ]; then
    printf '%sAll distributions passed.%s\n' "$G" "$N"
else
    printf '%sSome distributions failed.%s\n' "$R" "$N"
fi
exit "$rc"
