plugins {
    id("com.android.application") version "8.4.2" apply false
    id("org.jetbrains.kotlin.android") version "1.9.24" apply false
}

subprojects {
    configurations.configureEach {
        exclude(group = "androidx.profileinstaller", module = "profileinstaller")
    }
}
