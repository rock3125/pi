# Installer builder

Makes a package that installs piview on a server.

```sh
./build-installer.sh                  # sources only; the server builds the images
./build-installer.sh --with-images    # images included; the server builds nothing
./build-installer.sh --version 1.1.0  # stamp a version
```

The result lands in `dist/`, with a `.sha256` beside it:

| | size | needs on the server |
| --- | --- | --- |
| `piview-<v>-installer.tar.gz` | ~150 KB | docker, compose, and network for base images and build dependencies |
| `piview-<v>-installer-offline.tar.gz` | ~160 MB | docker and compose, nothing else |

The first is what you want normally. The second is for a server with no route
to Docker Hub, Maven Central and npm, or when you would rather not spend a few
minutes of its CPU compiling.

## What is in the package

```
install.sh              run on the server, with sudo
piviewctl               management command, installed to /usr/local/bin
piview.service.in       systemd unit, templated at install time
piview.env.example      configuration, copied to /etc/piview/piview.env
VERSION
README.md               the install and operate guide
images.tar.gz           only with --with-images
stack/                  what ends up in /opt/piview
  docker-compose.yml    env-driven; the repository's own compose file is not used
  Makefile src/         the interpreter's sources
  samples/              seeded into /var/lib/piview/programs on first install
  piview/server piview/ui piview/docker
  uninstall.sh
```

`stack/` is the build context both Dockerfiles expect, so a server with the
sources can rebuild either image without the rest of the repository.

## Testing it

The installer is exercised two ways.

**Logic, on all three distributions.** `docker` and `systemctl` are stubbed and
`install.sh` is run inside `ubuntu:24.04`, `debian:13` and `fedora:44`, checking
what lands where, that the unit is templated, that an edited config and the
programs directory survive an upgrade, and that uninstall and `--purge` remove
the right things.

**End to end, for real.** A privileged `ubuntu:24.04` container running systemd
as pid 1 with its own docker daemon, given real volumes for `/var/lib/docker`
and `/var/lib/containerd` — overlay cannot be mounted on overlay, so the inner
daemon needs storage that is not the outer container's filesystem. The installer
runs unmodified against it: service starts, containers come up healthy, queries
answer over HTTP, the service survives a reboot of the host, an upgrade keeps
the configuration, and uninstall leaves nothing behind.

## Versioning

`--version` stamps `VERSION`, the image tags (`piview/prolog:<v>`,
`piview/piview:<v>`) and `PIVIEW_VERSION` in the installed config. Installing a
new version rewrites that line, so compose picks up the new tags; the rest of
the configuration is left as the operator wrote it.
