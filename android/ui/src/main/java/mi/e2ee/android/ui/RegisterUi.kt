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
import androidx.compose.material.icons.filled.Person
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
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
fun RegisterScreen(
    onLogin: () -> Unit = {},
    onCreateAccount: (String, String) -> Unit = { _, _ -> },
    errorMessage: String? = null,
    statusMessage: String? = null
) {
    val name = remember { mutableStateOf("") }
    val email = remember { mutableStateOf("") }
    val password = remember { mutableStateOf("") }
    val confirm = remember { mutableStateOf("") }
    val localError = remember { mutableStateOf<String?>(null) }
    val passwordMismatchText = tr("register_password_mismatch", "Passwords do not match")

    Box(modifier = Modifier.fillMaxSize()) {
        RegisterBackground()
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 24.dp, vertical = 32.dp),
            verticalArrangement = Arrangement.spacedBy(20.dp)
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(16.dp)) {
                RegisterMark()
                Text(
                    text = tr("register_title", "Create account"),
                    style = MaterialTheme.typography.displayLarge
                )
                Text(
                    text = tr("register_subtitle", "Secure onboarding for private chats."),
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
                    if (!localError.value.isNullOrBlank()) {
                        RegisterErrorBanner(message = localError.value!!)
                    }
                    if (!errorMessage.isNullOrBlank()) {
                        RegisterErrorBanner(message = errorMessage)
                    }
                    if (!statusMessage.isNullOrBlank()) {
                        RegisterStatusBanner(message = statusMessage)
                    }
                    OutlinedTextField(
                        value = name.value,
                        onValueChange = { name.value = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(tr("register_name", "Display name")) },
                        leadingIcon = {
                            Icon(
                                imageVector = Icons.Filled.Person,
                                contentDescription = "Name"
                            )
                        },
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Text,
                            imeAction = ImeAction.Next
                        ),
                        shape = RoundedCornerShape(16.dp)
                    )
                    OutlinedTextField(
                        value = email.value,
                        onValueChange = { email.value = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(tr("register_phone_email", "Phone or email")) },
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
                        placeholder = { Text(tr("register_password", "Password")) },
                        leadingIcon = {
                            Icon(
                                imageVector = Icons.Filled.Lock,
                                contentDescription = "Password"
                            )
                        },
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Password,
                            imeAction = ImeAction.Next
                        ),
                        shape = RoundedCornerShape(16.dp)
                    )
                    OutlinedTextField(
                        value = confirm.value,
                        onValueChange = { confirm.value = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(tr("register_confirm_password", "Confirm password")) },
                        leadingIcon = {
                            Icon(
                                imageVector = Icons.Filled.Lock,
                                contentDescription = "Confirm password"
                            )
                        },
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Password,
                            imeAction = ImeAction.Done
                        ),
                        shape = RoundedCornerShape(16.dp)
                    )
                    PrimaryButton(
                        label = tr("register_create", "Create account"),
                        enabled = email.value.isNotBlank() &&
                            password.value.isNotBlank() &&
                            confirm.value.isNotBlank(),
                        onClick = {
                            if (password.value != confirm.value) {
                                localError.value = passwordMismatchText
                            } else {
                                localError.value = null
                                onCreateAccount(email.value.trim(), password.value)
                            }
                        }
                    )
                }
            }

            AuthFooterRow(
                leftText = tr("register_have_account", "Already have an account? Sign in"),
                onLeftClick = onLogin,
                rightPrefix = tr("register_privacy_prefix", "By continuing you agree to"),
                rightLink = tr("register_privacy_policy", "Privacy Policy")
            )
            Spacer(modifier = Modifier.weight(1f))
        }
    }
}

@Composable
private fun RegisterErrorBanner(message: String) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .background(MaterialTheme.colorScheme.error.copy(alpha = 0.12f))
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        Text(
            text = message,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.error
        )
    }
}

@Composable
private fun RegisterStatusBanner(message: String) {
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
private fun RegisterMark() {
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
private fun RegisterBackground() {
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
private fun RegisterPreview() {
    ChatTheme { RegisterScreen() }
}
