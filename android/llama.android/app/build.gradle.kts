plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.jetbrains.kotlin.android)
}

val releaseStoreFilePath = providers.gradleProperty("dmcReleaseStoreFile").orNull
    ?: System.getenv("DMC_RELEASE_STORE_FILE")
val releaseStorePassword = providers.gradleProperty("dmcReleaseStorePassword").orNull
    ?: System.getenv("DMC_RELEASE_STORE_PASSWORD")
val releaseKeyAlias = providers.gradleProperty("dmcReleaseKeyAlias").orNull
    ?: System.getenv("DMC_RELEASE_KEY_ALIAS")
val releaseKeyPassword = providers.gradleProperty("dmcReleaseKeyPassword").orNull
    ?: System.getenv("DMC_RELEASE_KEY_PASSWORD")
val releaseSigningReady =
    !releaseStoreFilePath.isNullOrBlank() &&
        !releaseStorePassword.isNullOrBlank() &&
        !releaseKeyAlias.isNullOrBlank() &&
        !releaseKeyPassword.isNullOrBlank() &&
        file(releaseStoreFilePath).exists()

android {
    namespace = "com.inetconnector.dmc"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.inetconnector.dmc"

        minSdk = 33
        targetSdk = 36

        versionCode = 1
        versionName = "1.0.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        vectorDrawables {
            useSupportLibrary = true
        }
    }

    if (releaseSigningReady) {
        signingConfigs {
            create("release") {
                storeFile = file(releaseStoreFilePath!!)
                storePassword = releaseStorePassword
                keyAlias = releaseKeyAlias
                keyPassword = releaseKeyPassword
            }
        }
    }

    buildTypes {
        debug {
            isMinifyEnabled = false
            isShrinkResources = false
            proguardFiles(
                getDefaultProguardFile("proguard-android.txt"),
                "proguard-rules.pro"
            )
        }
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            if (releaseSigningReady) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    sourceSets {
        getByName("main") {
            assets.srcDir(layout.buildDirectory.dir("generated/webuiAssets"))
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }

    aaptOptions {
        // Keep SvelteKit's "_app" asset tree. Android's default ignore pattern drops
        // directories starting with "_" which would strip the JS bundle from the APK.
        ignoreAssetsPattern = "!.svn:!.git:!.ds_store:!*.scc:.*:!CVS:!thumbs.db:!picasa.ini:!*~"
    }
}

val copyWebUiAssets by tasks.registering(Copy::class) {
    from(layout.projectDirectory.dir("../../../upstream/llama.cpp/tools/server/public"))
    into(layout.buildDirectory.dir("generated/webuiAssets/webui"))
}

tasks.named("preBuild") {
    dependsOn(copyWebUiAssets)
}

dependencies {
    implementation(libs.bundles.androidx)
    implementation(libs.material)
    implementation("org.nanohttpd:nanohttpd:2.3.1")
    implementation("com.google.mlkit:text-recognition:16.0.1")
    implementation("com.google.mlkit:image-labeling:17.0.9")

    implementation(project(":lib"))

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}
