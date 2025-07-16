plugins {
    id("com.android.library")
}

android {
    namespace = "com.example.mylib"
    compileSdk = 36  // 你可以写 36，但正式 SDK 可能尚未发布

    defaultConfig {
        minSdk = 21

        // ABI 过滤（可选）
//        ndk {
//            //noinspection ChromeOsAbiSupport
//            abiFilters += listOf("armeabi-v7a", "arm64-v8a")
//        }

        externalNativeBuild {
            cmake {
                cppFlags += ""
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
