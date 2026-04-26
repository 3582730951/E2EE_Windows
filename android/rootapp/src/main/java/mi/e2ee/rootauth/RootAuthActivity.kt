package mi.e2ee.rootauth

import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface

class RootAuthActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        hardenWindowForPrivateUi()
        val endpointThreatReport = EndpointThreatDetector.evaluate(this)
        if (endpointThreatReport.blocked) {
            finishAndRemoveTask()
            return
        }
        setContent {
            MaterialTheme {
                Surface {
                    RootAuthScreen()
                }
            }
        }
    }

    private fun hardenWindowForPrivateUi() {
        window.setFlags(
            WindowManager.LayoutParams.FLAG_SECURE,
            WindowManager.LayoutParams.FLAG_SECURE
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            window.decorView.importantForAutofill =
                View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.decorView.importantForContentCapture =
                View.IMPORTANT_FOR_CONTENT_CAPTURE_NO_EXCLUDE_DESCENDANTS
        }
    }
}
