# piview — server install

A PI prolog interpreter with an MCP server and a web view, as a docker compose
stack under systemd.

Tested on **Ubuntu 24.04**, **Debian 13** and **Fedora 44**, on a host that
already runs `docker` with the compose plugin. Anything else that has those two
will very likely work — pass `--force`.

## Install

```sh
tar xzf piview-<version>-installer.tar.gz
cd piview-<version>-installer
sudo ./install.sh
```

That builds the two images (a few minutes the first time), installs the stack in
`/opt/piview`, writes `/etc/piview/piview.env`, enables `piview.service` and
starts it. If the package was built with `--with-images` the images come with it
and nothing is compiled on the server.

```
  --prefix <dir>     where the stack lives           (default /opt/piview)
  --port <n>         port for the web view           (default 7070)
  --bind <addr>      address for the web view        (default 127.0.0.1)
  --no-build         fail rather than build; for the bundled-image package
  --no-start         install without starting
  --force            install on an untested distribution
```

Re-running `install.sh` upgrades in place. Your configuration, your programs and
the running database are left alone.

## Reaching it

**The default is loopback, and it is the right default.** piview has no
authentication: anyone who can open it can run any goal, rewrite the database,
and use `load` to read any file the interpreter can reach. Get to it one of two
ways.

An SSH tunnel, for one person:

```sh
ssh -N -L 7070:127.0.0.1:7070 user@server
# then http://127.0.0.1:7070
```

Or a reverse proxy that authenticates, for more than one. Point it at
`127.0.0.1:7070` and leave `PIVIEW_BIND` alone — it needs to pass WebSocket
upgrades through for the live view to work:

```nginx
location / {
    proxy_pass         http://127.0.0.1:7070;
    proxy_http_version 1.1;
    proxy_set_header   Upgrade    $http_upgrade;
    proxy_set_header   Connection "upgrade";
    proxy_set_header   Host       $host;
    auth_basic         "piview";
    auth_basic_user_file /etc/nginx/piview.htpasswd;
}
```

Setting `PIVIEW_BIND=0.0.0.0` puts an unauthenticated interpreter on the
network. The installer warns; it is still your call.

## Running it

```sh
piviewctl status        # service, containers, what the interpreter holds
piviewctl logs -f       # follow the container logs
piviewctl restart       # after editing the config
piviewctl config        # edit the config, then restart
piviewctl programs      # list the programs directory
piviewctl prolog        # a prolog prompt on the running interpreter
piviewctl uninstall     # remove it
```

## Programs

Drop `.pl` files in `/var/lib/piview/programs`. They are mounted read-only into
both containers at `/app/programs`, appear in the web view's sample list, and
can be loaded from the browser or by an agent.

To load one at boot, set `PI_PROGRAM` in `/etc/piview/piview.env` and restart.

## Configuration

`/etc/piview/piview.env`, read by both the systemd unit and by `docker compose`
run from `/opt/piview`.

| Setting | Default | What it does |
| --- | --- | --- |
| `PIVIEW_BIND` | `127.0.0.1` | address the web view listens on |
| `PIVIEW_PORT` | `7070` | port for the web view and `/mcp` |
| `PI_BIND` | `127.0.0.1` | address pi's raw TCP port is published on |
| `PI_PORT` | `7071` | pi's raw TCP port |
| `PI_PROGRAM` | `/app/programs/family.pl` | loaded when the interpreter starts |
| `PIVIEW_PROGRAMS` | `/var/lib/piview/programs` | host directory mounted at `/app/programs` |

## Connecting an agent

The MCP endpoint is `/mcp` on the web port, so through a tunnel or proxy:

```
http://127.0.0.1:7070/mcp
```

There is no authentication on it either. Everything above applies.

## Upgrading

Build a new installer, copy it over, run `sudo ./install.sh`. It stops the
service, replaces `/opt/piview`, keeps your configuration and programs, rebuilds
or loads the new images, and starts again.

## Uninstalling

```sh
sudo piviewctl uninstall            # keeps /etc/piview and /var/lib/piview
sudo piviewctl uninstall --purge    # takes those and the images too
```

## What gets installed where

```
/opt/piview/                the stack: sources, compose file, uninstaller
/etc/piview/piview.env      configuration
/var/lib/piview/programs/   your .pl files
/usr/local/bin/piviewctl    management command
/etc/systemd/system/piview.service
```
