#!/usr/bin/env bash
#
# Install piview on a server that already runs docker compose.
#
#   sudo ./install.sh                 install or upgrade
#   sudo ./install.sh --help          the options
#
# Supported: Ubuntu 24.04, Debian 13, Fedora 44.  Anything else with a working
# `docker compose` will very likely work too - pass --force to go ahead.

set -euo pipefail

PREFIX=/opt/piview
CONFIG_DIR=/etc/piview
CONFIG=/etc/piview/piview.env
STATE_DIR=/var/lib/piview
UNIT=/etc/systemd/system/piview.service
BINDIR=/usr/local/bin

FORCE=0
START=1
BUILD=auto        # auto | always | never
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
PKG_VERSION=$(cat "$HERE/VERSION" 2>/dev/null || echo unknown)

# ---------------------------------------------------------------- output

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    B=$'\033[1m'; G=$'\033[32m'; Y=$'\033[33m'; R=$'\033[31m'; D=$'\033[2m'; N=$'\033[0m'
else
    B=""; G=""; Y=""; R=""; D=""; N=""
fi

say()  { printf '%s\n' "$*"; }
step() { printf '%s==>%s %s\n' "$B" "$N" "$*"; }
ok()   { printf '  %s✓%s %s\n' "$G" "$N" "$*"; }
warn() { printf '  %s!%s %s\n' "$Y" "$N" "$*" >&2; }
die()  { printf '%serror:%s %s\n' "$R" "$N" "$*" >&2; exit 1; }

usage() {
    cat <<EOF
piview installer ${PKG_VERSION}

  sudo ./install.sh [options]

Options:
  --prefix <dir>     where the stack lives           (default ${PREFIX})
  --config <file>    configuration file              (default ${CONFIG})
  --state <dir>      programs and data               (default ${STATE_DIR})
  --port <n>         port for the web view           (default 7070)
  --bind <addr>      address for the web view        (default 127.0.0.1)
  --build            build the images even if the package carries them
  --no-build         never build; fail if no images are present
  --no-start         install but do not start the service
  --force            install on an untested distribution
  -h, --help         this

Re-running installs over an existing install: the configuration file, the
programs directory and the running database are left alone.
EOF
}

# ---------------------------------------------------------------- options

BIND_OVERRIDE=""
PORT_OVERRIDE=""

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)   PREFIX=${2:?--prefix needs a directory}; shift 2 ;;
        --config)   CONFIG=${2:?--config needs a file}; CONFIG_DIR=$(dirname "$CONFIG"); shift 2 ;;
        --state)    STATE_DIR=${2:?--state needs a directory}; shift 2 ;;
        --port)     PORT_OVERRIDE=${2:?--port needs a number}; shift 2 ;;
        --bind)     BIND_OVERRIDE=${2:?--bind needs an address}; shift 2 ;;
        --build)    BUILD=always; shift ;;
        --no-build) BUILD=never; shift ;;
        --no-start) START=0; shift ;;
        --force)    FORCE=1; shift ;;
        -h|--help)  usage; exit 0 ;;
        *)          die "unknown option: $1  (--help for the list)" ;;
    esac
done

# ---------------------------------------------------------------- checks

step "Checking this host"

[ "$(id -u)" = "0" ] || die "run this with sudo - it writes to $PREFIX, $CONFIG_DIR and systemd"

DISTRO=unknown
DISTRO_VERSION=""
DISTRO_PRETTY=""
if [ -r /etc/os-release ]; then
    # in a subshell: os-release sets NAME, VERSION and friends, and VERSION in
    # particular would quietly overwrite this script's own
    eval "$(
        # shellcheck disable=SC1091
        . /etc/os-release
        printf 'DISTRO=%q DISTRO_VERSION=%q DISTRO_PRETTY=%q' \
            "${ID:-unknown}" "${VERSION_ID:-}" "${PRETTY_NAME:-}"
    )"
fi
DISTRO_LABEL=${DISTRO_PRETTY:-$DISTRO $DISTRO_VERSION}

case "${DISTRO}-${DISTRO_VERSION}" in
    ubuntu-24.04|debian-13|fedora-44)
        ok "$DISTRO_LABEL"
        ;;
    *)
        if [ "$FORCE" = "1" ]; then
            warn "$DISTRO_LABEL is untested; continuing because of --force"
        else
            die "$(printf '%s is not one of the tested distributions.\n%s' \
                "$DISTRO_LABEL" \
                "       Tested: Ubuntu 24.04, Debian 13, Fedora 44. Pass --force to install anyway.")"
        fi
        ;;
esac

command -v systemctl >/dev/null 2>&1 || die "systemd is needed to run piview as a service"

DOCKER=$(command -v docker || true)
[ -n "$DOCKER" ] || die "$(printf 'docker is not installed.\n%s' \
    "       See https://docs.docker.com/engine/install/ for ${DISTRO}.")"

docker compose version >/dev/null 2>&1 || die "$(printf 'the docker compose plugin is missing.\n%s' \
    "       Install docker-compose-plugin (deb) or docker-compose-plugin (dnf).")"

docker info >/dev/null 2>&1 || die "$(printf 'the docker daemon is not answering.\n%s' \
    "       Try: systemctl start docker")"

ok "docker $(docker version --format '{{.Server.Version}}' 2>/dev/null || echo '?'), compose $(docker compose version --short 2>/dev/null || echo '?')"

DOCKER_UNIT=docker.service
systemctl list-unit-files docker.service >/dev/null 2>&1 || DOCKER_UNIT=""
if [ -z "$DOCKER_UNIT" ]; then
    warn "no docker.service found; the unit will not order itself after docker"
    DOCKER_UNIT=network-online.target
fi

# a port already in use gives a confusing failure three steps later
PORT_WANTED=${PORT_OVERRIDE:-}
if [ -z "$PORT_WANTED" ] && [ -r "$CONFIG" ]; then
    PORT_WANTED=$(sed -n 's/^PIVIEW_PORT=//p' "$CONFIG" | tail -1)
fi
PORT_WANTED=${PORT_WANTED:-7070}
if command -v ss >/dev/null 2>&1 && [ "$(systemctl is-active piview.service 2>/dev/null || true)" != "active" ]; then
    if ss -lnt 2>/dev/null | awk '{print $4}' | grep -qE "[:.]${PORT_WANTED}\$"; then
        warn "something is already listening on port ${PORT_WANTED}; the service will fail to start unless you pass --port"
    fi
fi

UPGRADE=0
[ -d "$PREFIX/piview" ] && UPGRADE=1

# ---------------------------------------------------------------- stop

if [ "$UPGRADE" = "1" ] && systemctl is-active --quiet piview.service; then
    step "Stopping the running service"
    systemctl stop piview.service
    ok "stopped"
fi

# ---------------------------------------------------------------- files

step "Installing into $PREFIX"

install -d -m 0755 "$PREFIX" "$CONFIG_DIR" "$STATE_DIR" "$STATE_DIR/programs"

# the stack replaces itself wholesale; nothing outside it is touched
rm -rf "$PREFIX/src" "$PREFIX/piview" "$PREFIX/Makefile" "$PREFIX/docker-compose.yml" "$PREFIX/samples"
cp -a "$HERE/stack/." "$PREFIX/"
cp -a "$HERE/VERSION" "$PREFIX/VERSION" 2>/dev/null || true
[ -f "$HERE/README.md" ] && cp -a "$HERE/README.md" "$PREFIX/README.md"
ok "stack copied"

# seed the programs directory once; a later install never overwrites what is
# there, because these files are the user's to edit
seeded=0
if [ -d "$PREFIX/samples" ]; then
    for f in "$PREFIX/samples"/*.pl; do
        [ -e "$f" ] || continue
        if [ ! -e "$STATE_DIR/programs/$(basename "$f")" ]; then
            cp -a "$f" "$STATE_DIR/programs/"
            seeded=$((seeded+1))
        fi
    done
fi
[ "$seeded" -gt 0 ] && ok "seeded $seeded sample program(s) into $STATE_DIR/programs"

# ---------------------------------------------------------------- config

step "Configuring"

if [ -f "$CONFIG" ]; then
    ok "keeping the existing $CONFIG"
    # the version has to follow the install or compose looks for old image tags
    if grep -q '^PIVIEW_VERSION=' "$CONFIG"; then
        sed -i "s|^PIVIEW_VERSION=.*|PIVIEW_VERSION=${PKG_VERSION}|" "$CONFIG"
    else
        printf 'PIVIEW_VERSION=%s\n' "$PKG_VERSION" >> "$CONFIG"
    fi
else
    sed -e "s|__VERSION__|${PKG_VERSION}|" "$HERE/piview.env.example" > "$CONFIG"
    sed -i "s|^PIVIEW_PROGRAMS=.*|PIVIEW_PROGRAMS=${STATE_DIR}/programs|" "$CONFIG"
    chmod 0644 "$CONFIG"
    ok "wrote $CONFIG"
fi

[ -n "$PORT_OVERRIDE" ] && sed -i "s|^PIVIEW_PORT=.*|PIVIEW_PORT=${PORT_OVERRIDE}|" "$CONFIG"
if [ -n "$BIND_OVERRIDE" ]; then
    sed -i "s|^PIVIEW_BIND=.*|PIVIEW_BIND=${BIND_OVERRIDE}|" "$CONFIG"
    case "$BIND_OVERRIDE" in
        127.0.0.1|localhost|::1) ;;
        *) warn "the web view will accept connections on ${BIND_OVERRIDE} and it has no authentication of its own - put a proxy that does in front of it" ;;
    esac
fi

# one set of values for the unit and for anyone running compose by hand
ln -sfn "$CONFIG" "$PREFIX/.env"

# ---------------------------------------------------------------- images

# shellcheck disable=SC1090
set -a; . "$CONFIG"; set +a

have_images() {
    docker image inspect "piview/prolog:${PKG_VERSION}" >/dev/null 2>&1 &&
    docker image inspect "piview/piview:${PKG_VERSION}" >/dev/null 2>&1
}

if [ -f "$HERE/images.tar.gz" ] && ! have_images; then
    step "Loading the bundled images"
    # docker load reports a broken layer on stderr and still exits 0, which
    # would leave a half-unpacked image that only fails when a container is
    # started - so the output is checked rather than the status
    load_log=$(mktemp)
    gunzip -c "$HERE/images.tar.gz" | docker load 2>&1 | tee "$load_log" || true
    if grep -qiE '^(error|failed)|error unpacking|failed to (extract|register)' "$load_log"; then
        rm -f "$load_log"
        die "$(printf 'the bundled images did not unpack cleanly - see above.\n%s' \
            "       Run install.sh --build to build them here instead.")"
    fi
    rm -f "$load_log"
    have_images || die "the bundled images did not load; try install.sh --build"
    ok "loaded"
fi

case "$BUILD" in
    always) DO_BUILD=1 ;;
    never)  DO_BUILD=0 ;;
    auto)   if have_images; then DO_BUILD=0; else DO_BUILD=1; fi ;;
esac

if [ "$DO_BUILD" = "1" ]; then
    step "Building the images (a few minutes the first time)"
    ( cd "$PREFIX" && docker compose build ) || die "the image build failed - see the output above"
    ok "built"
elif ! have_images; then
    die "no images for ${PKG_VERSION} are present and --no-build was given"
else
    ok "using the images already present"
fi

# ---------------------------------------------------------------- service

step "Installing the service"

install -d -m 0755 "$(dirname "$UNIT")" "$BINDIR"

sed -e "s|__PREFIX__|${PREFIX}|g" \
    -e "s|__CONFIG__|${CONFIG}|g" \
    -e "s|__DOCKER__|${DOCKER}|g" \
    -e "s|__DOCKER_UNIT__|${DOCKER_UNIT}|g" \
    "$HERE/piview.service.in" > "$UNIT"
chmod 0644 "$UNIT"

install -m 0755 "$HERE/piviewctl" "$BINDIR/piviewctl"
sed -i -e "s|^PREFIX=.*|PREFIX=${PREFIX}|" -e "s|^CONFIG=.*|CONFIG=${CONFIG}|" "$BINDIR/piviewctl"

systemctl daemon-reload
systemctl enable piview.service >/dev/null 2>&1
ok "piview.service installed and enabled"

# ---------------------------------------------------------------- start

if [ "$START" = "1" ]; then
    step "Starting piview"

    if ! systemctl start piview.service; then
        printf '\n'
        systemctl --no-pager --lines=20 status piview.service || true
        printf '\n'
        die "$(printf 'the service did not start - the log above says why.\n%s\n%s' \
            "       Everything is installed, so fix the cause and run: systemctl start piview" \
            "       Common ones: the port is taken, or the images will not run on this host.")"
    fi

    printf '  waiting for the web view'
    up=0
    for _ in $(seq 1 60); do
        if curl -fsS -o /dev/null "http://127.0.0.1:${PIVIEW_PORT:-7070}/api/status" 2>/dev/null; then
            up=1; break
        fi
        printf '.'; sleep 1
    done
    printf '\n'

    if [ "$up" = "1" ]; then
        ok "answering"
    else
        warn "it did not answer within 60s - check: piviewctl logs"
    fi
fi

# ---------------------------------------------------------------- done

ADDR=${PIVIEW_BIND:-127.0.0.1}
say ""
say "${G}piview ${PKG_VERSION} is installed.${N}"
say ""
say "  web view    http://${ADDR}:${PIVIEW_PORT:-7070}"
say "  mcp         http://${ADDR}:${PIVIEW_PORT:-7070}/mcp"
say "  programs    ${STATE_DIR}/programs    ${D}(drop .pl files here)${N}"
say "  config      ${CONFIG}"
say ""
say "  ${D}piviewctl status | logs | restart | shell | uninstall${N}"
say ""
if [ "${PIVIEW_BIND:-127.0.0.1}" = "127.0.0.1" ]; then
    say "  ${D}Listening on loopback only. piview has no authentication of its own,${N}"
    say "  ${D}so reach it over an SSH tunnel, or put an authenticating proxy in front:${N}"
    say "  ${D}  ssh -N -L ${PIVIEW_PORT:-7070}:127.0.0.1:${PIVIEW_PORT:-7070} $(id -un)@$(hostname -f 2>/dev/null || hostname)${N}"
    say ""
fi
