package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
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
    val showAdvanced = remember { mutableStateOf(initialRootCode.isNotBlank()) }
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
                .padding(horizontal = 20.dp, vertical = 18.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                AppMark()
                Text(
                    text = tr("login_title", "Welcome back"),
                    style = MaterialTheme.typography.titleLarge
                )
                Text(
                    text = tr("login_subtitle", "Secure sign-in for private conversations."),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                if (!headerStatus.isNullOrBlank()) {
                    LoginSupportStatusRow(message = headerStatus)
                }
            }

            SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 2.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
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
                    LoginAdvancedAccessRow(
                        expanded = showAdvanced.value,
                        onToggle = { showAdvanced.value = !showAdvanced.value }
                    )
                    if (showAdvanced.value || rootCode.value.isNotBlank()) {
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
                    }
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
private fun LoginAdvancedAccessRow(
    expanded: Boolean,
    onToggle: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .clickable(onClick = onToggle)
            .padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(
            text = if (expanded) {
                tr("login_hide_advanced", "Hide advanced sign-in")
            } else {
                tr("login_show_advanced", "Use root auth")
            },
            style = MaterialTheme.typography.labelLarge,
            color = MaterialTheme.colorScheme.primary
        )
        Spacer(modifier = Modifier.weight(1f))
        Text(
            text = tr("login_optional", "Optional"),
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
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
private fun LoginSupportStatusRow(message: String) {
    Row(
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        UiTokenIcon(
            label = "i",
            size = 14.dp,
            cornerRadius = 4.dp,
            containerColor = MaterialTheme.colorScheme.secondary.copy(alpha = 0.12f),
            contentColor = MaterialTheme.colorScheme.secondary,
            textStyle = MaterialTheme.typography.labelSmall
        )
        Text(
            text = message,
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
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
                .heightIn(min = 48.dp),
            placeholder = { Text(placeholder) },
            keyboardOptions = keyboardOptions,
            visualTransformation = visualTransformation,
            shape = RoundedCornerShape(14.dp),
            singleLine = singleLine
        )
    }
}

@Composable
private fun AppMark() {
    Row(verticalAlignment = Alignment.CenterVertically) {
        UiTokenIcon(
            label = "MI",
            size = 32.dp,
            cornerRadius = 10.dp,
            containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.16f),
            contentColor = MaterialTheme.colorScheme.primary
        )
        Spacer(modifier = Modifier.width(8.dp))
        Column {
            Text(text = tr("app_name", "MI Secure"), style = MaterialTheme.typography.titleSmall)
            Text(
                text = tr("app_tagline", "Private chat"),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
private fun LoginBackground() {
    val base = MaterialTheme.colorScheme.background
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.linearGradient(
                    colors = listOf(base, MaterialTheme.colorScheme.surface.copy(alpha = 0.98f)),
                    start = Offset.Zero,
                    end = Offset(0f, 1200f)
                )
            )
    )
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
        modifier = modifier.height(40.dp),
        shape = RoundedCornerShape(14.dp),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.5f)),
        colors = ButtonDefaults.outlinedButtonColors(
            containerColor = MaterialTheme.colorScheme.surface
        )
    ) {
        UiTokenIcon(
            label = token,
            size = 18.dp,
            cornerRadius = 6.dp,
            containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.14f),
            contentColor = MaterialTheme.colorScheme.primary
        )
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = label,
            style = MaterialTheme.typography.labelLarge
        )
    }
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun LoginPreview() {
    LoginApp()
}
