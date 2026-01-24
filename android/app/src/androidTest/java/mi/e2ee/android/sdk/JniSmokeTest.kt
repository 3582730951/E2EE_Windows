package mi.e2ee.android.sdk

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class JniSmokeTest {
    @Test
    fun loadVersionAndCapabilities() {
        assumeTrue("native sdk unavailable", NativeSdk.available)
        val version = NativeSdk.getVersion()
        assertTrue("sdk abi missing", version.abi > 0)
        val caps = NativeSdk.getCapabilities()
        assertTrue("sdk capabilities missing", caps != 0)
    }
}
