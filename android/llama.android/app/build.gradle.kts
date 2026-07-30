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
    flavorDimensions += "distribution"

    buildFeatures {
        buildConfig = true
    }

    defaultConfig {
        applicationId = "com.inetconnector.dmc"

        minSdk = 33
        targetSdk = 36

        versionCode = 4
        versionName = "1.1.1"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        buildConfigField("String", "PLAY_PRODUCT_ID", "\"inetmind_full_unlock\"")
        buildConfigField("String", "INETMIND_API_BASE_URL", "\"https://apps.inetconnector.com/inetmind/v1\"")
        vectorDrawables {
            useSupportLibrary = true
        }
    }

    productFlavors {
        create("full") {
            dimension = "distribution"
            buildConfigField("Boolean", "ENABLE_PLAY_BILLING", "false")
            buildConfigField("Boolean", "ENABLE_OFFLINE_KNOWLEDGE", "true")
            buildConfigField("String", "PRIVACY_POLICY_URL", "\"https://inetconnector.github.io/DMC/privacy/\"")
        }
        create("play") {
            dimension = "distribution"
            buildConfigField("Boolean", "ENABLE_PLAY_BILLING", "true")
            buildConfigField("Boolean", "ENABLE_OFFLINE_KNOWLEDGE", "false")
            buildConfigField("String", "PRIVACY_POLICY_URL", "\"https://inetconnector.github.io/DMC/privacy/play/\"")
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
            assets.srcDir(layout.buildDirectory.dir("generated/licenseAssets"))
        }
        getByName("full") {
            assets.srcDir(layout.buildDirectory.dir("generated/webuiAssets"))
            assets.srcDir(layout.projectDirectory.dir("../../../knowledge"))
        }
        getByName("play") {
            assets.srcDir(layout.buildDirectory.dir("generated/playWebuiAssets"))
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

val playExcludedTerms = mapOf(
    "ICD-10" to "reference data",
    "ICD-11" to "reference data",
    "icd10" to "reference",
    "icd11" to "reference",
    "BfArM" to "official publisher",
    "Orphadata" to "official publisher",
    "medical classifications" to "specialized reference data",
    "medizinische Klassifikationen" to "spezialisierte Referenzdaten"
)

val copyPlayWebUiAssets by tasks.registering(Copy::class) {
    from(layout.projectDirectory.dir("../../../upstream/llama.cpp/tools/server/public"))
    into(layout.buildDirectory.dir("generated/playWebuiAssets/webui"))
    filteringCharset = "UTF-8"
    filesMatching(listOf("**/*.html", "**/*.js", "**/*.json", "**/*.txt")) {
        filter { line: String ->
            playExcludedTerms.entries.fold(line) { content, (term, replacement) ->
                content.replace(term, replacement)
            }
        }
    }
}

val copyLicenseAssets by tasks.registering(Copy::class) {
    into(layout.buildDirectory.dir("generated/licenseAssets/licenses"))
    from(layout.projectDirectory.file("../../../LICENSE")) {
        rename { "DMC-LICENSE.txt" }
    }
    from(layout.projectDirectory.file("../../../THIRD_PARTY_NOTICES.md"))
    from(layout.projectDirectory.file("../../../third_party_licenses/Apache-2.0.txt"))
    from(layout.projectDirectory.file("../../../third_party_licenses/NanoHTTPD-BSD-3-Clause.txt"))
    from(layout.projectDirectory.file("../../../third_party_licenses/WEB_UI_NOTICES.txt"))
    from(layout.projectDirectory.file("../../../upstream/llama.cpp/LICENSE")) {
        rename { "LLAMA-CPP-LICENSE.txt" }
    }
}

tasks.named("preBuild") {
    dependsOn(copyWebUiAssets)
    dependsOn(copyPlayWebUiAssets)
    dependsOn(copyLicenseAssets)
}

dependencies {
    implementation(libs.bundles.androidx)
    implementation(libs.material)
    implementation("org.nanohttpd:nanohttpd:2.3.1")
    implementation("com.google.mlkit:text-recognition:16.0.1")
    implementation("com.google.mlkit:image-labeling:17.0.9")
    implementation("com.android.billingclient:billing:9.1.0")

    implementation(project(":lib"))

    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}
