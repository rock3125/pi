#!/usr/bin/env bash
#
# Build an installer for piview.
#
#   ./piview/install/build-installer.sh                  sources; the server builds
#   ./piview/install/build-installer.sh --with-images     images too; the server builds nothing
#
# Produces piview/install/dist/piview-<version>-installer.tar.gz and a matching
# .sha256.  Copy it to the server, unpack, run install.sh.
#
# The plain package is a few hundred kilobytes and needs the server to have
# network access for the base images and the build dependencies.  --with-images
# is around 600 MB and needs nothing but docker.

set -euo pipefail

HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd -- "$HERE/../.." && pwd)
DIST="$HERE/dist"

VERSION=1.0.0
WITH_IMAGES=0

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    B=$'\033[1m'; G=$'\033[32m'; N=$'\033[0m'
else
    B=""; G=""; N=""
fi
step() { printf '%s==>%s %s\n' "$B" "$N" "$*"; }
die()  { printf 'error: %s\n' "$*" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --with-images) WITH_IMAGES=1; shift ;;
        --version)     VERSION=${2:?--version needs a value}; shift 2 ;;
        --out)         DIST=${2:?--out needs a directory}; shift 2 ;;
        -h|--help)
            printf 'build-installer.sh [--with-images] [--version <v>] [--out <dir>]\n'
            exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

# the two packages install identically but are very different sizes, so they
# get names you can tell apart on a server
if [ "$WITH_IMAGES" = "1" ]; then
    NAME="piview-${VERSION}-installer-offline"
else
    NAME="piview-${VERSION}-installer"
fi
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
STAGE="$WORK/$NAME"

# ---------------------------------------------------------------- assemble

step "Assembling ${NAME}"

mkdir -p "$STAGE/stack"

# what the server needs to build both images: the interpreter's sources, the
# server and ui sources, and the dockerfiles
copy() {
    local src=$1 dest=$2
    [ -e "$ROOT/$src" ] || die "missing from the repository: $src"
    mkdir -p "$(dirname "$STAGE/stack/$dest")"
    cp -a "$ROOT/$src" "$STAGE/stack/$dest"
}

copy Makefile          Makefile
copy src               src
copy samples           samples
copy piview/server     piview/server
copy piview/ui         piview/ui
copy piview/docker     piview/docker
copy piview/README.md  piview/README.md

# host build output has no business in an image build context
rm -rf "$STAGE/stack/piview/ui/node_modules" \
       "$STAGE/stack/piview/ui/dist" \
       "$STAGE/stack/piview/server/build" \
       "$STAGE/stack/piview/server/.gradle" \
       "$STAGE/stack/build"

cp -a "$HERE/payload/docker-compose.yml"   "$STAGE/stack/docker-compose.yml"
cp -a "$HERE/payload/install.sh"           "$STAGE/install.sh"
cp -a "$HERE/payload/uninstall.sh"         "$STAGE/stack/uninstall.sh"
cp -a "$HERE/payload/piviewctl"            "$STAGE/piviewctl"
cp -a "$HERE/payload/piview.service.in"    "$STAGE/piview.service.in"
cp -a "$HERE/payload/piview.env.example"   "$STAGE/piview.env.example"
cp -a "$HERE/payload/README.md"            "$STAGE/README.md"

printf '%s\n' "$VERSION" > "$STAGE/VERSION"
chmod +x "$STAGE/install.sh" "$STAGE/piviewctl" "$STAGE/stack/uninstall.sh"

printf '  %s\n' "$(du -sh "$STAGE" | cut -f1) of sources"

# ---------------------------------------------------------------- images

if [ "$WITH_IMAGES" = "1" ]; then
    step "Building the images to bundle"
    command -v docker >/dev/null 2>&1 || die "docker is needed for --with-images"

    ( cd "$STAGE/stack" \
      && PIVIEW_VERSION="$VERSION" COMPOSE_PROJECT_NAME=piviewbuild docker compose build ) \
      || die "the image build failed"

    step "Saving the images"
    docker save "piview/prolog:${VERSION}" "piview/piview:${VERSION}" \
        | gzip -9 > "$STAGE/images.tar.gz"
    printf '  %s of images\n' "$(du -sh "$STAGE/images.tar.gz" | cut -f1)"
fi

# ---------------------------------------------------------------- package

step "Packaging"

mkdir -p "$DIST"
TARBALL="$DIST/${NAME}.tar.gz"
rm -f "$TARBALL" "$TARBALL.sha256"

tar -C "$WORK" -czf "$TARBALL" "$NAME"
( cd "$DIST" && sha256sum "${NAME}.tar.gz" > "${NAME}.tar.gz.sha256" )

printf '\n%spiview %s installer built.%s\n\n' "$G" "$VERSION" "$N"
printf '  %s  (%s)\n' "$TARBALL" "$(du -h "$TARBALL" | cut -f1)"
printf '  %s\n\n' "$TARBALL.sha256"
printf 'On the server:\n\n'
printf '  scp %s user@server:\n' "${NAME}.tar.gz"
printf '  ssh user@server\n'
printf '  tar xzf %s.tar.gz && cd %s\n' "$NAME" "$NAME"
printf '  sudo ./install.sh\n\n'
