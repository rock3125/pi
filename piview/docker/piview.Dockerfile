# The MCP server and the web view, in one jar.
#
# Build context is the repository root:
#   docker build -f piview/docker/piview.Dockerfile .

# ---------------------------------------------------------------- the ui
FROM node:24-slim AS ui
WORKDIR /ui
COPY piview/ui/package.json piview/ui/package-lock.json* ./
RUN npm install --no-audit --no-fund
COPY piview/ui ./
RUN npm run build

# ---------------------------------------------------------------- the server
FROM eclipse-temurin:21-jdk AS server
WORKDIR /build
COPY piview/server ./

# node is not in this image, so the gradle build is told to skip its own UI
# step and the bundle from the stage above is dropped straight into the
# resources it would otherwise have produced
COPY --from=ui /ui/dist ./src/main/resources/web
RUN ./gradlew --no-daemon -PskipUi=true shadowJar

# ---------------------------------------------------------------- runtime
FROM eclipse-temurin:21-jre
# as in the prolog image, netcat is only here for the compose health check
RUN apt-get update \
 && apt-get install --no-install-recommends -y netcat-openbsd \
 && rm -rf /var/lib/apt/lists/*
RUN useradd --create-home --shell /usr/sbin/nologin piview
WORKDIR /app

COPY --from=server /build/build/libs/piview-1.0.0.jar ./piview.jar
# the same path as in the prolog image: the ui hands pi a path to `load`, and
# pi resolves it in its own container
COPY samples ./samples

USER piview
EXPOSE 7070

ENTRYPOINT ["java", "-jar", "/app/piview.jar"]
CMD ["--host", "prolog", "--port", "7071", "--bind", "0.0.0.0", "--web-port", "7070", "--samples", "/app/samples"]
