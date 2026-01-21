plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val miOpaqueEnabled = (project.findProperty("miE2eeOpaque") as? String)
    ?.equals("true", ignoreCase = true) ?: true
val miOllvmEnabled = (project.findProperty("miE2eeOllvm") as? String)
    ?.equals("true", ignoreCase = true) ?: false
val miOllvmClang = project.findProperty("miE2eeOllvmClang") as? String
val miOllvmClangxx = project.findProperty("miE2eeOllvmClangxx") as? String
val miNdkVersion = (project.findProperty("miE2eeNdkVersion") as? String)
    ?.takeIf { it.isNotBlank() }
    ?: System.getenv("MI_E2EE_ANDROID_NDK_VERSION")?.takeIf { it.isNotBlank() }

android {
    namespace = "mi.e2ee.android"
    compileSdk = 34
    if (miNdkVersion != null) {
        ndkVersion = miNdkVersion
    }

    defaultConfig {
        applicationId = "mi.e2ee.android.ui"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "0.1"
        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += "-DMI_E2EE_ANDROID_USE_RUST_OPAQUE=" +
                    if (miOpaqueEnabled) "ON" else "OFF"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        debug {
            externalNativeBuild {
                cmake {
                    arguments += "-DMI_E2EE_ANDROID_OLLVM=OFF"
                }
            }
        }
        release {
            ndk {
                debugSymbolLevel = "NONE"
            }
            externalNativeBuild {
                cmake {
                    arguments += "-DMI_E2EE_ANDROID_OLLVM=" +
                        if (miOllvmEnabled) "ON" else "OFF"
                    if (!miOllvmClang.isNullOrBlank()) {
                        arguments += "-DMI_E2EE_OLLVM_C_COMPILER=$miOllvmClang"
                    }
                    if (!miOllvmClangxx.isNullOrBlank()) {
                        arguments += "-DMI_E2EE_OLLVM_CXX_COMPILER=$miOllvmClangxx"
                    }
                    if (miOllvmEnabled) {
                        arguments += "-DMI_E2EE_ENABLE_LTO=OFF"
                        arguments += "-DMI_E2EE_PGO_INSTRUMENT=OFF"
                        arguments += "-DMI_E2EE_PGO_USE=OFF"
                    }
                }
            }
        }
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    composeOptions {
        kotlinCompilerExtensionVersion = "1.5.14"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    sourceSets["main"].java.srcDirs(
        "src/main/java",
        "../ui/src/main/java"
    )

    packaging {
        resources.excludes.add("META-INF/*")
    }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2024.09.02"))
    implementation("androidx.activity:activity-compose:1.9.2")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.camera:camera-core:1.3.4")
    implementation("androidx.camera:camera-camera2:1.3.4")
    implementation("androidx.camera:camera-lifecycle:1.3.4")
    implementation("androidx.camera:camera-view:1.3.4")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.4")
    implementation("androidx.navigation:navigation-compose:2.8.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")
    debugImplementation("androidx.compose.ui:ui-tooling")
}
