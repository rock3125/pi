# The interpreter itself, built from source and served over TCP.
#
# Build context is the repository root:
#   docker build -f piview/docker/prolog.Dockerfile .

FROM gcc:14 AS build
WORKDIR /src
COPY Makefile ./
COPY src ./src
RUN make -j"$(nproc)"

FROM debian:trixie-slim
# netcat is only here for the compose health check: this image has no shell
# that understands /dev/tcp, and nothing else that can open a socket
RUN apt-get update \
 && apt-get install --no-install-recommends -y netcat-openbsd \
 && rm -rf /var/lib/apt/lists/*
RUN useradd --create-home --shell /usr/sbin/nologin pi
WORKDIR /app

COPY --from=build /src/prolog /usr/local/bin/prolog
COPY samples ./samples

USER pi
EXPOSE 8080

# `--bind '*'` is needed for piview in another container to reach this one.
# That is safe here only because the port is published to 127.0.0.1 in
# compose: pi's protocol has no authentication and its `load` command reads
# files from this container's filesystem.
#
# pi reads its prompt from stdin and exits on end-of-file, so the compose
# service keeps stdin open and allocates a tty.
CMD ["prolog", "--port", "8080", "--bind", "*", "samples/family.pl"]
