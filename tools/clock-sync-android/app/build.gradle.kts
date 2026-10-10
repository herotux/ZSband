plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "org.herotux.mibandclocksync"
    compileSdk = 35

    defaultConfig {
        applicationId = "org.herotux.mibandclocksync"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
    }
}
