plugins {
    id("com.android.application")
}

android {
    namespace = "org.ikemen.go"
    compileSdk = 35
    ndkVersion = "26.3.11579264"
    defaultConfig {
        applicationId = "org.ikemen.go"
        minSdk = 24
        targetSdk = 35
        versionCode = 3
        versionName = "1.2-stick"
        ndk {
            abiFilters += listOf("arm64-v8a")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_shared")
                cppFlags += listOf("-std=c++17")
            }
        }
    }
    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
        }
        getByName("debug") {
            isDebuggable = true
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
    sourceSets {
        getByName("main") {
            assets.srcDir(layout.buildDirectory.dir("gameAssets"))
        }
    }
}

tasks.register("syncGameAssets") {
    val dest = layout.buildDirectory.dir("gameAssets")
    outputs.dir(dest)
    doLast {
        val destDir = dest.get().asFile
        destDir.deleteRecursively()
        destDir.mkdirs()
        val repo = rootDir.parentFile
        val index = mutableListOf<String>()
        fun copyRel(rel: String) {
            val src = File(repo, rel.replace("/", File.separator))
            if (!src.exists()) return
            if (src.isDirectory) {
                src.walkTopDown().filter { it.isFile }.forEach { f ->
                    val r = f.relativeTo(repo).invariantSeparatorsPath
                    val out = File(destDir, r)
                    out.parentFile.mkdirs()
                    f.copyTo(out, overwrite = true)
                    index += r
                }
            } else {
                val out = File(destDir, rel)
                out.parentFile.mkdirs()
                src.copyTo(out, overwrite = true)
                index += rel
            }
        }
        copyRel("chars/kfm")
        copyRel("stages/kfm.def")
        copyRel("stages/kfm.sff")
        copyRel("data/net.ini")
        copyRel("data/fight.sff")
        copyRel("data/fight.def")
        copyRel("data/fightfx.sff")
        copyRel("data/fightfx.air")
        copyRel("data/glyphs.sff")
        copyRel("data/ikemen1")
        copyRel("cpp/lua")
        File(destDir, "asset_index.txt").writeText(index.joinToString("\n"))
    }
}

tasks.named("preBuild").configure {
    dependsOn("syncGameAssets")
}
