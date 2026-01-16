package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Mail
import androidx.compose.material.icons.filled.QrCodeScanner
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp

@Composable
fun LoginApp() {
    ChatTheme {
        LoginScreen()
    }
}

@Composable
fun LoginScreen(
    onRegister: () -> Unit = {},
    onLogin: (String, String) -> Unit = { _, _ -> },
    errorMessage: String? = null,
    statusMessage: String? = null,
    remoteError: String? = null
) {
    val email = remember { mutableStateOf("") }
    val password = remember { mutableStateOf("") }

    Box(modifier = Modifier.fillMaxSize()) {
        LoginBackground()
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 24.dp, vertical = 32.dp),
            verticalArrangement = Arrangement.spacedBy(20.dp)
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
                AppMark()
                Text(
                    text = tr("login_title", "Welcome back"),
                    style = MaterialTheme.typography.displayLarge
                )
                Text(
                    text = tr("login_subtitle", "Secure sign-in for private conversations."),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }

            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surface
                ),
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(20.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    if (!errorMessage.isNullOrBlank()) {
                        LoginErrorBanner(errorCode = errorMessage)
                    }
                    if (!remoteError.isNullOrBlank()) {
                        LoginStatusBanner(message = remoteError)
                    }
                    if (!statusMessage.isNullOrBlank()) {
                        LoginStatusBanner(message = statusMessage)
                    }
                    OutlinedTextField(
                        value = email.value,
                        onValueChange = { email.value = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(tr("login_phone_email", "Phone or email")) },
                        leadingIcon = {
                            Icon(
                                imageVector = Icons.Filled.Mail,
                                contentDescription = "Email"
                            )
                        },
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Email,
                            imeAction = ImeAction.Next
                        ),
                        shape = RoundedCornerShape(16.dp)
                    )
                    OutlinedTextField(
                        value = password.value,
                        onValueChange = { password.value = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(tr("login_password", "Password")) },
                        leadingIcon = {
                            Icon(
                                imageVector = Icons.Filled.Lock,
                                contentDescription = "Password"
                            )
                        },
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Password,
                            imeAction = ImeAction.Done
                        ),
                        shape = RoundedCornerShape(16.dp)
                    )
                    PrimaryButton(
                        label = tr("login_sign_in", "Sign in"),
                        enabled = email.value.isNotBlank() && password.value.isNotBlank(),
                        onClick = { onLogin(email.value.trim(), password.value) }
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedButton(
                            onClick = {},
                            modifier = Modifier
                                .weight(1f)
                                .height(48.dp),
                            shape = RoundedCornerShape(16.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Filled.Mail,
                                contentDescription = "One-time code"
                            )
                            Spacer(modifier = Modifier.width(6.dp))
                            Text(
                                text = tr("login_one_time_code", "One-time code"),
                                style = MaterialTheme.typography.titleMedium
                            )
                        }
                        OutlinedButton(
                            onClick = {},
                            modifier = Modifier
                                .weight(1f)
                                .height(48.dp),
                            shape = RoundedCornerShape(16.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Filled.QrCodeScanner,
                                contentDescription = "Scan QR"
                            )
                            Spacer(modifier = Modifier.width(6.dp))
                            Text(text = tr("login_scan_qr", "Scan QR"), style = MaterialTheme.typography.titleMedium)
                        }
                    }
                }
            }

            AuthFooterRow(
                leftText = tr("login_new_here", "New here? Create an account"),
                onLeftClick = onRegister,
                rightPrefix = tr("login_privacy_prefix", "By continuing you agree to"),
                rightLink = tr("login_privacy_policy", "Privacy Policy")
            )
            Spacer(modifier = Modifier.weight(1f))
        }
    }
}

@Composable
private fun LoginErrorBanner(errorCode: String) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .background(MaterialTheme.colorScheme.error.copy(alpha = 0.12f))
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        Row(
            modifier = Modifier.align(Alignment.Center),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                imageVector = Icons.Filled.Warning,
                contentDescription = tr("login_error", "Login failed - %s").format(errorCode),
                tint = MaterialTheme.colorScheme.error,
                modifier = Modifier.size(18.dp)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = tr("login_error", "Login failed - %s").format(errorCode),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.error
            )
        }
    }
}

@Composable
private fun LoginStatusBanner(message: String) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .background(MaterialTheme.colorScheme.secondary.copy(alpha = 0.12f))
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        Text(
            text = message,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.secondary
        )
    }
}

@Composable
private fun AppMark() {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Box(
            modifier = Modifier
                .size(48.dp)
                .clip(CircleShape)
                .background(MaterialTheme.colorScheme.primary),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = "MI",
                style = MaterialTheme.typography.titleMedium,
                color = Color.White
            )
        }
        Spacer(modifier = Modifier.width(12.dp))
        Column {
            Text(text = tr("app_name", "MI Secure"), style = MaterialTheme.typography.titleLarge)
            Text(
                text = tr("app_tagline", "Private chat"),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
private fun LoginBackground() {
    val base = MaterialTheme.colorScheme.background
    val isDark = base.luminance() < 0.3f
    val tint = MaterialTheme.colorScheme.primary.copy(alpha = if (isDark) 0.12f else 0.15f)
    val accent = MaterialTheme.colorScheme.secondary.copy(alpha = if (isDark) 0.08f else 0.1f)
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.linearGradient(
                    colors = listOf(base, MaterialTheme.colorScheme.surfaceVariant),
                    start = Offset.Zero,
                    end = Offset(0f, 1200f)
                )
            )
    ) {
        Box(
            modifier = Modifier
                .size(240.dp)
                .offset(x = 140.dp, y = (-40).dp)
                .clip(CircleShape)
                .background(tint)
        )
        Box(
            modifier = Modifier
                .size(200.dp)
                .offset(x = (-80).dp, y = 440.dp)
                .clip(CircleShape)
                .background(accent)
        )
    }
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun LoginPreview() {
    LoginApp()
}
