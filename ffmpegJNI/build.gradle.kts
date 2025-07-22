plugins {
    base
}
import org.gradle.api.DefaultTask
import org.gradle.api.provider.Property
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.TaskAction
import org.gradle.process.ExecOperations
import javax.inject.Inject

// 脚本顶层先声明你的构建参数
val ndkPath: String = project.property("ndkPath") as String
val ffmpegBaseDir: String = project.property("ffmpegBaseDir") as String
val androidPlatform: String = project.property("androidPlatform") as String
val abis = listOf("arm64-v8a", "x86_64")

abstract class BuildNativeTask : DefaultTask() {

    // 把 execOperations 声明成抽象 val 并注入
    @get:Inject
    abstract val execOperations: ExecOperations

    // 所有参数都用 Property<T> 并在 getter 上打注解
    @get:Input
    abstract val abi: Property<String>

    @get:Input
    abstract val ndkPathProp: Property<String>

    @get:Input
    abstract val ffmpegBaseDirProp: Property<String>

    @get:Input
    abstract val androidPlatformProp: Property<String>

    @TaskAction
    fun build() {
        val abi = abi.get()
        val ndk = ndkPathProp.get()
        val ffmpeg = ffmpegBaseDirProp.get()
        val platform = androidPlatformProp.get()

        val cmakeToolchain = "$ndk/build/cmake/android.toolchain.cmake"
        val buildDir = project.layout.buildDirectory.get().asFile.resolve("jni_build/$abi")
        if (buildDir.exists()) buildDir.deleteRecursively()
        buildDir.mkdirs()

        // 用注入的 execOperations 依次执行三条命令
        execOperations.exec {
            commandLine(
                "cmake", "-S", ".", "-B", buildDir.absolutePath,
                "-G", "Ninja",
                "-DCMAKE_TOOLCHAIN_FILE=$cmakeToolchain",
                "-DANDROID_ABI=$abi",
                "-DANDROID_PLATFORM=$platform",
                "-DCMAKE_BUILD_TYPE=Release",
                "-DBUILD_ANDROID_LIB=ON",
                "-DFFMPEG_ANDROID_INCLUDE_DIR=$ffmpeg/$abi/include",
                "-DFFMPEG_ANDROID_LIB_DIR=$ffmpeg/$abi/lib_static"
            )
        }

        execOperations.exec {
            workingDir = buildDir
            commandLine("cmake", "--build", ".", "--", "-j${Runtime.getRuntime().availableProcessors()}")
            standardOutput = System.out
            errorOutput    = System.err
        }

        execOperations.exec {
            commandLine("ln", "-sf", buildDir.resolve("compile_commands.json").absolutePath, "compile_commands.json")
        }

        println("✅ Built JNI .so for $abi")
    }
}

abis.forEach { abiName ->
    tasks.register<BuildNativeTask>("buildNative_$abiName") {
        group = "native"
        description = "Build JNI for ABI: $abiName"

        abi.set(abiName)
        ndkPathProp.set(ndkPath)
        ffmpegBaseDirProp.set(ffmpegBaseDir)
        androidPlatformProp.set(androidPlatform)
    }
}

tasks.register("buildNative") {
    group = "native"
    description = "Build native JNI for all ABIs"
    dependsOn(abis.map { "buildNative_$it" })
}


tasks.named<Delete>("clean") {
    doLast {
        println("Cleaning additional files...")
        file("compile_commands.json").delete()
    }
}


tasks.register<Exec>("test") {
    group = "verification"
    description = "Run CMake test build (like test.sh)"

    commandLine("cmake", "-S", ".", "-B", "build", "-GNinja", "-DBUILD_ANDROID_LIB=OFF")
    doLast {
        exec {
            commandLine("cmake", "--build", "build")
        }
        exec {
            commandLine("ln", "-sf", "build/compile_commands.json", "compile_commands.json")
        }
    }
}
