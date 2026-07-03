// :core — pure-JVM Kotlin module. NO Android plugin, NO android.* imports.
// This is the Android parallel of the NeuroPulseShared SPM package plus the
// platform-independent slices of app/ios/NeuroPulse. Everything here runs and
// tests on a plain JVM so privacy-critical logic is verifiable in CI without
// an Android SDK (ISC-2, ISC-3, ISC-4 in app/android/ISA.md).

plugins {
    kotlin("jvm") version "2.0.21"
    kotlin("plugin.serialization") version "2.0.21"
    `java-library`
}

java {
    toolchain { languageVersion.set(JavaLanguageVersion.of(17)) }
}

dependencies {
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.7.3")

    testImplementation(kotlin("test"))
    testImplementation("org.junit.jupiter:junit-jupiter:5.10.2")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

tasks.test {
    useJUnitPlatform()
}
