#!/usr/bin/env bash
#
# Remove piview.
#
#   sudo piviewctl uninstall            leave the config and programs behind
#   sudo piviewctl uninstall --purge     take everything

set -euo pipefail

PREFIX=/opt/piview
CONFIG_DIR=/etc/piview
STATE_DIR=/var/lib/piview
UNIT=/etc/systemd/system/piview.service
BINDIR=/usr/local/bin

PURGE=0
YES=0

while [ $# -gt 0 ]; do
    case "$1" in
        --purge) PURGE=1; shift ;;
        -y|--yes) YES=1; shift ;;
        -h|--help)
            printf 'sudo piviewctl uninstall [--purge] [-y]\n\n'
            printf '  --purge   also remove %s, %s and the images\n' "$CONFIG_DIR" "$STATE_DIR"
            printf '  -y        do not ask\n'
            exit 0 ;;
        *) printf 'unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done

[ "$(id -u)" = "0" ] || { printf 'run this with sudo\n' >&2; exit 1; }

printf 'This removes the piview service and %s.\n' "$PREFIX"
if [ "$PURGE" = "1" ]; then
    printf 'With --purge it also removes %s, %s and the piview images.\n' "$CONFIG_DIR" "$STATE_DIR"
else
    printf 'Your configuration in %s and programs in %s are kept.\n' "$CONFIG_DIR" "$STATE_DIR"
fi

if [ "$YES" != "1" ]; then
    printf 'Go ahead? [y/N] '
    read -r reply
    case "$reply" in y|Y|yes|YES) ;; *) printf 'nothing done\n'; exit 0 ;; esac
fi

VERSION=$(cat "$PREFIX/VERSION" 2>/dev/null || echo "")

if systemctl list-unit-files piview.service >/dev/null 2>&1; then
    systemctl stop piview.service 2>/dev/null || true
    systemctl disable piview.service 2>/dev/null || true
fi

# take the containers and network down before the compose file goes away
if [ -d "$PREFIX" ] && command -v docker >/dev/null 2>&1; then
    ( cd "$PREFIX" && docker compose down --remove-orphans 2>/dev/null ) || true
fi

rm -f "$UNIT"
systemctl daemon-reload 2>/dev/null || true

rm -rf "$PREFIX"
rm -f "$BINDIR/piviewctl"

if [ "$PURGE" = "1" ]; then
    rm -rf "$CONFIG_DIR" "$STATE_DIR"
    if [ -n "$VERSION" ] && command -v docker >/dev/null 2>&1; then
        docker image rm "piview/piview:${VERSION}" "piview/prolog:${VERSION}" 2>/dev/null || true
    fi
    printf 'piview removed, configuration and programs included.\n'
else
    printf 'piview removed.  %s and %s are still there.\n' "$CONFIG_DIR" "$STATE_DIR"
fi
