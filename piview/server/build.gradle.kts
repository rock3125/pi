import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    kotlin("jvm") version "2.4.10"
    kotlin("plugin.serialization") version "2.4.10"
    id("com.gradleup.shadow") version "9.6.1"
    application
}

group = "nz.pi"
version = "1.0.0"

repositories {
    mavenCentral()
}

dependencies {
    implementation("io.modelcontextprotocol:kotlin-sdk:0.15.0")

    implementation("io.ktor:ktor-server-core:3.5.2")
    implementation("io.ktor:ktor-server-cio:3.5.2")
    implementation("io.ktor:ktor-server-websockets:3.5.2")
    implementation("io.ktor:ktor-server-content-negotiation:3.5.2")
    implementation("io.ktor:ktor-server-cors:3.5.2")
    implementation("io.ktor:ktor-server-status-pages:3.5.2")
    implementation("io.ktor:ktor-server-sse:3.5.2")
    implementation("io.ktor:ktor-serialization-kotlinx-json:3.5.2")

    implementation("org.jetbrains.kotlinx:kotlinx-io-core:0.9.1")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.11.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.11.0")
    implementation("ch.qos.logback:logback-classic:1.6.1")

    testImplementation(kotlin("test"))
}

kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_21)
    }
}

java {
    sourceCompatibility = JavaVersion.VERSION_21
    targetCompatibility = JavaVersion.VERSION_21
}

application {
    mainClass.set("nz.pi.piview.MainKt")
}

// ---------------------------------------------------------------- the ui
//
// The React bundle is built by npm and folded into the jar under /web, so a
// single artefact carries both halves.  -PskipUi=true builds the server alone,
// which is what you want when node is not around.

val uiDir = layout.projectDirectory.dir("../ui")
val uiDist = uiDir.dir("dist")
val skipUi = (project.findProperty("skipUi") as String?) == "true"
val npm = if (System.getProperty("os.name").lowercase().contains("windows")) "npm.cmd" else "npm"

val npmInstall = tasks.register<Exec>("npmInstall") {
    description = "Installs the UI's node dependencies"
    workingDir = uiDir.asFile
    commandLine(npm, "install", "--no-audit", "--no-fund")
    inputs.file(uiDir.file("package.json"))
    outputs.dir(uiDir.dir("node_modules"))
}

val npmBuild = tasks.register<Exec>("npmBuild") {
    description = "Builds the React UI into ui/dist"
    dependsOn(npmInstall)
    workingDir = uiDir.asFile
    commandLine(npm, "run", "build")
    inputs.dir(uiDir.dir("src"))
    inputs.file(uiDir.file("package.json"))
    inputs.file(uiDir.file("vite.config.ts"))
    inputs.file(uiDir.file("index.html"))
    outputs.dir(uiDist)
}

tasks.processResources {
    if (!skipUi) {
        dependsOn(npmBuild)
        from(uiDist) { into("web") }
    }
}

tasks.test {
    useJUnitPlatform()
}

tasks.shadowJar {
    archiveClassifier.set("")
    mergeServiceFiles()
}
