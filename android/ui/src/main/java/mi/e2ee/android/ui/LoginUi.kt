package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.ButtonDefaults
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
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
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
    onLogin: (String, String, String) -> Unit = { _, _, _ -> },
    onShowQr: (String) -> Unit = {},
    onScanQr: () -> Unit = {},
    initialUsername: String = "",
    initialPassword: String = "",
    initialRootCode: String = "",
    errorMessage: String? = null,
    statusMessage: String? = null,
    remoteError: String? = null
) {
    val email = remember { mutableStateOf(initialUsername) }
    val password = remember { mutableStateOf(initialPassword) }
    val rootCode = remember { mutableStateOf(initialRootCode) }
    val bannerMessage = when {
        !errorMessage.isNullOrBlank() -> errorMessage
        !remoteError.isNullOrBlank() -> remoteError
        !statusMessage.isNullOrBlank() -> statusMessage
        else -> null
    }
    val bannerIsError = !errorMessage.isNullOrBlank() || !remoteError.isNullOrBlank()
    val headerStatus = bannerMessage?.takeIf { !bannerIsError }
    val cardStatus = bannerMessage?.takeIf { bannerIsError }

    Box(modifier = Modifier.fillMaxSize()) {
        LoginBackground()
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 18.dp, vertical = 16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                AppMark()
                Text(
                    text = tr("login_title", "Welcome back"),
                    style = MaterialTheme.typography.headlineMedium
                )
                Text(
                    text = tr("login_subtitle", "Secure sign-in for private conversations."),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                if (!headerStatus.isNullOrBlank()) {
                    LoginStatusChip(message = headerStatus)
                }
            }

            SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 2.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    if (!cardStatus.isNullOrBlank()) {
                        LoginErrorBanner(errorCode = cardStatus)
                    }

                    LoginInputField(
                        label = tr("login_phone_email", "Phone or email"),
                        value = email.value,
                        onValueChange = { email.value = it },
                        placeholder = tr("login_phone_email", "Phone or email"),
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Email,
                            imeAction = ImeAction.Next
                        ),
                        singleLine = true
                    )
                    LoginInputField(
                        label = tr("login_password", "Password"),
                        value = password.value,
                        onValueChange = { password.value = it },
                        placeholder = tr("login_password", "Password"),
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Password,
                            imeAction = ImeAction.Done
                        ),
                        visualTransformation = PasswordVisualTransformation()
                    )
                    LoginInputField(
                        label = tr("login_root_code_short", "Root auth (optional)"),
                        value = rootCode.value,
                        onValueChange = { value ->
                            val cleaned = value.trim().filter {
                                it.isLetterOrDigit() || it == ':' || it == '|'
                            }
                            rootCode.value = cleaned.take(160)
                        },
                        placeholder = tr("login_root_code_placeholder", "code:signature"),
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Ascii,
                            imeAction = ImeAction.Done
                        ),
                        singleLine = true
                    )
                    PrimaryButton(
                        label = tr("login_sign_in", "Sign in"),
                        enabled = email.value.isNotBlank() && password.value.isNotBlank(),
                        onClick = { onLogin(email.value.trim(), password.value, rootCode.value) }
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        LoginUtilityButton(
                            label = tr("login_qr_show", "Show QR"),
                            token = "QR",
                            onClick = { onShowQr(email.value.trim()) },
                            modifier = Modifier.weight(1f)
                        )
                        LoginUtilityButton(
                            label = tr("login_scan_qr", "Scan QR"),
                            token = "SC",
                            onClick = onScanQr,
                            modifier = Modifier.weight(1f)
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.weight(1f))
            AuthFooterRow(
                leftText = tr("login_new_here", "New here? Create an account"),
                onLeftClick = onRegister,
                rightPrefix = tr("login_privacy_prefix", "By continuing you agree to"),
                rightLink = tr("login_privacy_policy", "Privacy Policy")
            )
        }
    }
}

@Composable
private fun LoginErrorBanner(errorCode: String) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(MaterialTheme.colorScheme.error.copy(alpha = 0.10f))
            .padding(horizontal = 10.dp, vertical = 8.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            UiTokenIcon(
                label = "!",
                size = 18.dp,
                cornerRadius = 6.dp,
                containerColor = MaterialTheme.colorScheme.error.copy(alpha = 0.12f),
                contentColor = MaterialTheme.colorScheme.error
            )
            Spacer(modifier = Modifier.width(6.dp))
            Text(
                text = tr("login_error", "Login failed - %s").format(errorCode),
                style = MaterialTheme.typography.labelLarge,
                color = MaterialTheme.colorScheme.error,
                maxLines = 2
            )
        }
    }
}

@Composable
private fun LoginStatusChip(message: String) {
    Row(
        modifier = Modifier
            .clip(RoundedCornerShape(999.dp))
            .background(MaterialTheme.colorScheme.secondary.copy(alpha = 0.12f))
            .padding(horizontal = 10.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        UiTokenIcon(
            label = "i",
            size = 16.dp,
            cornerRadius = 5.dp,
            containerColor = MaterialTheme.colorScheme.secondary.copy(alpha = 0.16f),
            contentColor = MaterialTheme.colorScheme.secondary
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = message,
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.secondary,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
private fun LoginInputField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    placeholder: String,
    keyboardOptions: KeyboardOptions,
    singleLine: Boolean = true,
    visualTransformation: VisualTransformation = VisualTransformation.None
) {
    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        OutlinedTextField(
            value = value,
            onValueChange = onValueChange,
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(min = 50.dp),
            placeholder = { Text(placeholder) },
            keyboardOptions = keyboardOptions,
            visualTransformation = visualTransformation,
            shape = RoundedCornerShape(16.dp),
            singleLine = singleLine
        )
    }
}

@Composable
private fun AppMark() {
    Row(verticalAlignment = Alignment.CenterVertically) {
        UiTokenIcon(
            label = "MI",
            size = 36.dp,
            cornerRadius = 12.dp,
            containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.16f),
            contentColor = MaterialTheme.colorScheme.primary
        )
        Spacer(modifier = Modifier.width(10.dp))
        Column {
            Text(text = tr("app_name", "MI Secure"), style = MaterialTheme.typography.titleSmall)
            Text(
                text = tr("app_tagline", "Private chat"),
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
private fun LoginBackground() {
    val base = MaterialTheme.colorScheme.background
    val isDark = base.luminance() < 0.3f
    val tint = MaterialTheme.colorScheme.primary.copy(alpha = if (isDark) 0.07f else 0.09f)
    val accent = MaterialTheme.colorScheme.secondary.copy(alpha = if (isDark) 0.05f else 0.06f)
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
                .size(88.dp)
                .offset(x = 250.dp, y = (-18).dp)
                .clip(CircleShape)
                .background(tint)
        )
        Box(
            modifier = Modifier
                .size(72.dp)
                .offset(x = (-26).dp, y = 600.dp)
                .clip(CircleShape)
                .background(accent)
        )
    }
}

@Composable
private fun LoginUtilityButton(
    label: String,
    token: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    OutlinedButton(
        onClick = onClick,
        modifier = modifier.height(42.dp),
        shape = RoundedCornerShape(16.dp),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.5f)),
        colors = ButtonDefaults.outlinedButtonColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.22f)
        )
    ) {
        UiTokenIcon(
            label = token,
            size = 20.dp,
            cornerRadius = 6.dp,
            containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.14f),
            contentColor = MaterialTheme.colorScheme.primary
        )
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = label,
            style = MaterialTheme.typography.bodyMedium
        )
    }
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun LoginPreview() {
    LoginApp()
}
