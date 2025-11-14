plugins {
    id("com.android.application")
    id("kotlin-android")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

android {
    namespace = "com.sims.sims_app"
    compileSdk = flutter.compileSdkVersion
    ndkVersion = "27.0.12077973"

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    kotlinOptions {
        jvmTarget = JavaVersion.VERSION_11.toString()
    }

    defaultConfig {
        // TODO: Specify your own unique Application ID (https://developer.android.com/studio/build/application-id.html).
        applicationId = "com.sims.sims_app"
        // You can update the following values to match your application needs.
        // For more information, see: https://flutter.dev/to/review-gradle-config.
        minSdk = 23  // Required by record_android plugin
        targetSdk = flutter.targetSdkVersion
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }

    buildTypes {
        release {
            // TODO: Add your own signing config for the release build.
            // Signing with the debug keys for now, so `flutter run --release` works.
            signingConfig = signingConfigs.getByName("debug")
        }
    }
}

flutter {
    source = "../.."
}

afterEvaluate {
    // Create copies for Flutter compatibility
    tasks.register("copyApkForFlutter") {
        doLast {
            val outputDir = layout.buildDirectory.dir("outputs/flutter-apk").get().asFile
            val debugApk = File(outputDir, "app-debug.apk")
            val flutterDebugApk = File(outputDir, "app--debug.apk")
            val releaseApk = File(outputDir, "app-release.apk")
            val flutterReleaseApk = File(outputDir, "app--release.apk")

            if (debugApk.exists()) {
                debugApk.copyTo(flutterDebugApk, overwrite = true)
                println("Created app--debug.apk")
            }
            if (releaseApk.exists()) {
                releaseApk.copyTo(flutterReleaseApk, overwrite = true)
                println("Created app--release.apk")
            }
        }
    }

    tasks.named("assembleDebug") {
        finalizedBy("copyApkForFlutter")
    }

    tasks.named("assembleRelease") {
        finalizedBy("copyApkForFlutter")
    }
}
