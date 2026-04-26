package mi.e2ee.rootauth

import android.content.Context
import android.content.pm.ApplicationInfo
import android.os.Debug
import java.io.File
import java.util.Locale

data class EndpointThreatReport(
    val blocked: Boolean,
    val reasons: List<String>
) {
    val summary: String
        get() = if (reasons.isEmpty()) {
            "No local endpoint threat signals"
        } else {
            reasons.joinToString(", ")
        }
}

object EndpointThreatDetector {
    private val instrumentationNeedles = listOf(
        "frida",
        "gum-js-loop",
        "re.frida.server",
        "xposed",
        "lsposed",
        "edxp",
        "magisk",
        "zygisk"
    )

    private val rootArtifacts = listOf(
        "/system/xbin/su",
        "/system/bin/su",
        "/sbin/su",
        "/system/app/Superuser.apk",
        "/sbin/.magisk",
        "/data/adb/magisk",
        "/data/adb/ksu"
    )

    fun evaluate(context: Context): EndpointThreatReport {
        val reasons = mutableListOf<String>()
        var blocked = false

        if (Debug.isDebuggerConnected() || Debug.waitingForDebugger()) {
            reasons += "debugger-attached"
            blocked = true
        }

        val tracerPid = readTracerPid()
        if (tracerPid > 0) {
            reasons += "tracerpid-$tracerPid"
            blocked = true
        }

        findNeedleInFile("/proc/self/maps", instrumentationNeedles)?.let {
            reasons += "instrumentation-$it"
            blocked = true
        }

        findNeedleInFile("/proc/self/status", instrumentationNeedles)?.let {
            reasons += "status-$it"
            blocked = true
        }

        installedInstrumentationPackage(context)?.let {
            reasons += "package-$it"
            blocked = true
        }

        firstExistingRootArtifact()?.let {
            reasons += "root-artifact-${File(it).name}"
            if (!BuildConfig.DEBUG) {
                blocked = true
            }
        }

        if (systemProperty("ro.debuggable") == "1") {
            reasons += "ro.debuggable=1"
            if (!BuildConfig.DEBUG) {
                blocked = true
            }
        }

        if ((context.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0) {
            reasons += "app-debuggable"
        }

        return EndpointThreatReport(blocked = blocked, reasons = reasons.distinct())
    }

    private fun readTracerPid(): Int {
        val status = runCatching { File("/proc/self/status").readLines() }
            .getOrDefault(emptyList())
        val tracerLine = status.firstOrNull { it.startsWith("TracerPid:") }
            ?: return 0
        return tracerLine.substringAfter(':').trim().toIntOrNull() ?: 0
    }

    private fun findNeedleInFile(path: String, needles: List<String>): String? {
        return runCatching {
            File(path).useLines { lines ->
                lines.firstNotNullOfOrNull { line ->
                    val lower = line.lowercase(Locale.US)
                    needles.firstOrNull { lower.contains(it) }
                }
            }
        }.getOrNull()
    }

    private fun installedInstrumentationPackage(context: Context): String? {
        val packages = listOf(
            "de.robv.android.xposed.installer",
            "org.lsposed.manager",
            "io.github.vvb2060.magisk",
            "com.topjohnwu.magisk",
            "re.frida.server"
        )
        return packages.firstOrNull { name ->
            runCatching {
                context.packageManager.getPackageInfo(name, 0)
                true
            }.getOrDefault(false)
        }
    }

    private fun firstExistingRootArtifact(): String? {
        return rootArtifacts.firstOrNull { File(it).exists() }
    }

    private fun systemProperty(name: String): String {
        return runCatching {
            val klass = Class.forName("android.os.SystemProperties")
            val method = klass.getMethod("get", String::class.java)
            method.invoke(null, name) as? String
        }.getOrNull().orEmpty()
    }
}
