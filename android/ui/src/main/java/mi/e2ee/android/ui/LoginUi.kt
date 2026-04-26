package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.graphics.Brush
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextAlign
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
    onScanQr: () -> Unit = {},
    initialUsername: String = "",
    initialPassword: String = "",
    initialRootCode: String = "",
    errorMessage: String? = null,
    statusMessage: String? = null,
    remoteError: String? = null
) {
    val colors = phaseOneColors()
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
    val cardStatus = bannerMessage?.takeIf { bannerIsError }

    Box(modifier = Modifier.fillMaxSize()) {
        LoginBackground()
        Column(
            modifier = Modifier
                .fillMaxSize()
                .statusBarsPadding()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 16.dp, vertical = 18.dp),
            verticalArrangement = Arrangement.spacedBy(18.dp)
        ) {
            // Spacer(modifier = Modifier.weight(0.38f))
            Spacer(modifier = Modifier.heightIn(min = 24.dp))
            Column(
                modifier = Modifier.fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                AppMark()
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(
                        text = tr("login_sign_in", "Sign in"),
                        style = phaseOneLargeTitleTextStyle(),
                        color = colors.onSurface,
                        textAlign = TextAlign.Center
                    )
                    Spacer(modifier = Modifier.width(1.dp))
                    Text(
                        text = tr("login_account_hint", "Use your account to continue"),
                        style = MaterialTheme.typography.bodyMedium,
                        color = colors.onSurfaceMuted,
                        textAlign = TextAlign.Center
                    )
                }
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    MediaHintChip(
                        kind = MediaHintKind.Link,
                        label = tr("login_chip_private", "Private"),
                        showLabel = true
                    )
                    MediaHintChip(
                        kind = MediaHintKind.File,
                        label = tr("login_chip_device", "Device-bound"),
                        showLabel = true
                    )
                }
            }
            InsetGroupedCard(modifier = Modifier.fillMaxWidth(), contentPadding = PaddingValues(16.dp)) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 2.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    if (!cardStatus.isNullOrBlank()) {
                        LoginErrorBanner(errorCode = cardStatus)
                    }

                    LoginInputField(
                        label = tr("login_phone_email", "Phone or email"),
                        leadingIcon = MiOwnedIcons.Person,
                        value = email.value,
                        onValueChange = { email.value = it },
                        placeholder = tr("login_phone_email", "Phone or email"),
                        keyboardOptions = KeyboardOptions(
                            autoCorrectEnabled = false,
                            keyboardType = KeyboardType.Email,
                            imeAction = ImeAction.Next
                        ),
                        singleLine = true,
                        showLabel = false
                    )
                    LoginInputField(
                        label = tr("login_password", "Password"),
                        leadingIcon = MiOwnedIcons.Lock,
                        value = password.value,
                        onValueChange = { password.value = it },
                        placeholder = tr("login_password", "Password"),
                        keyboardOptions = KeyboardOptions(
                            autoCorrectEnabled = false,
                            keyboardType = KeyboardType.Password,
                            imeAction = ImeAction.Done
                        ),
                        visualTransformation = PasswordVisualTransformation(),
                        showLabel = false
                    )
                    if (showAdvanced.value || rootCode.value.isNotBlank()) {
                        LoginInputField(
                            label = tr("login_root_code_short", "Approval code"),
                            leadingIcon = MiOwnedIcons.ShieldCheck,
                            value = rootCode.value,
                            onValueChange = { value ->
                                val cleaned = value.trim().filter {
                                    it.isLetterOrDigit() || it == ':' || it == '|'
                                }
                                rootCode.value = cleaned.take(160)
                            },
                            placeholder = tr("login_root_code_short", "Approval code"),
                            keyboardOptions = KeyboardOptions(
                                autoCorrectEnabled = false,
                                keyboardType = KeyboardType.Ascii,
                                imeAction = ImeAction.Done
                            ),
                            singleLine = true,
                            showLabel = false
                        )
                    }
                    PrimaryButton(
                        label = tr("login_sign_in", "Sign in"),
                        enabled = email.value.isNotBlank() && password.value.isNotBlank(),
                        onClick = { onLogin(email.value.trim(), password.value, rootCode.value) }
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        SecondaryButton(
                            label = tr("login_qr_show", "QR sign in"),
                            modifier = Modifier.weight(1f),
                            fillMaxWidth = false,
                            onClick = onScanQr
                        )
                        SecondaryButton(
                            label = tr("login_new_here", "Register"),
                            modifier = Modifier.weight(1f),
                            fillMaxWidth = false,
                            onClick = onRegister
                        )
                    }
                    Surface(
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(14.dp),
                        color = colors.surfaceVariant.copy(alpha = 0.82f)
                    ) {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(horizontal = 12.dp, vertical = 10.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Column(modifier = Modifier.weight(1f)) {
                                Text(
                                    text = tr("login_advanced_title", "Advanced"),
                                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.SemiBold),
                                    color = colors.onSurface
                                )
                                Text(
                                    text = tr("login_advanced_hint", "Root approval code for high-risk sign-in"),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = colors.onSurfaceMuted
                                )
                            }
                            TextButton(onClick = { showAdvanced.value = !showAdvanced.value }) {
                                Text(
                                text = if (showAdvanced.value) {
                                    tr("login_hide_advanced", "Hide advanced")
                                } else {
                                    tr("login_show_advanced", "Advanced")
                                },
                                    style = MaterialTheme.typography.labelLarge
                                )
                            }
                        }
                    }
                }
            }
            Spacer(modifier = Modifier.heightIn(min = 18.dp))
        }
    }
}

@Composable
private fun LoginErrorBanner(errorCode: String) {
    val colors = phaseOneColors()
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(colors.danger.copy(alpha = 0.10f))
            .padding(horizontal = 10.dp, vertical = 8.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            UiTokenIcon(
                label = "!",
                size = 18.dp,
                cornerRadius = 6.dp,
                containerColor = colors.danger.copy(alpha = 0.12f),
                contentColor = colors.danger
            )
            Spacer(modifier = Modifier.width(6.dp))
            Text(
                text = tr("login_error", "Login failed - %s").format(errorCode),
                style = MaterialTheme.typography.labelLarge,
                color = colors.danger,
                maxLines = 2
            )
        }
    }
}

@Composable
private fun LoginInputField(
    label: String,
    leadingIcon: androidx.compose.ui.graphics.vector.ImageVector,
    value: String,
    onValueChange: (String) -> Unit,
    placeholder: String,
    keyboardOptions: KeyboardOptions,
    singleLine: Boolean = true,
    visualTransformation: VisualTransformation = VisualTransformation.None,
    showLabel: Boolean = true
) {
    val colors = phaseOneColors()
    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        if (showLabel) {
            Text(
                text = label,
                style = MaterialTheme.typography.labelMedium,
                color = colors.onSurfaceMuted
            )
        }
        OutlinedTextField(
            value = value,
            onValueChange = onValueChange,
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(min = 48.dp),
            leadingIcon = {
                Icon(
                    imageVector = leadingIcon,
                    contentDescription = label,
                    tint = colors.onSurfaceMuted
                )
            },
            placeholder = { Text(placeholder) },
            keyboardOptions = keyboardOptions,
            visualTransformation = visualTransformation,
            shape = RoundedCornerShape(14.dp),
            singleLine = singleLine,
            colors = OutlinedTextFieldDefaults.colors(
                focusedTextColor = colors.onSurface,
                unfocusedTextColor = colors.onSurface,
                focusedBorderColor = colors.primary.copy(alpha = 0.24f),
                unfocusedBorderColor = colors.outline.copy(alpha = 0.18f),
                focusedContainerColor = colors.surfaceVariant.copy(alpha = 0.72f),
                unfocusedContainerColor = colors.surfaceVariant.copy(alpha = 0.72f)
            )
        )
    }
}

@Composable
private fun AppMark() {
    Surface(
        shape = RoundedCornerShape(26.dp),
        color = phaseOneColors().glass,
        border = androidx.compose.foundation.BorderStroke(1.dp, phaseOneColors().glassBorder)
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center
        ) {
            IdentityAvatar(
                label = "MI E2EE",
                seed = "app-mark",
                kind = IdentityAvatarKind.System,
                size = 44.dp,
                badgeIcon = MiOwnedIcons.ShieldCheck
            )
            Spacer(modifier = Modifier.width(10.dp))
            Column {
                Text(
                    text = tr("app_name", "MI E2EE"),
                    style = MaterialTheme.typography.bodyLarge.copy(fontWeight = FontWeight.SemiBold)
                )
                Text(
                    text = tr("login_mark_subtitle", "Secure messaging"),
                    style = MaterialTheme.typography.bodySmall,
                    color = phaseOneColors().onSurfaceMuted
                )
            }
        }
    }
}

@Composable
private fun LoginBackground() {
    val colors = phaseOneColors()
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(
                        colors.background,
                        colors.surfaceVariant.copy(alpha = 0.74f),
                        colors.background
                    )
                )
            )
    )
}

@Preview(showBackground = true, widthDp = 360, heightDp = 800)
@Composable
private fun LoginPreview() {
    LoginApp()
}
