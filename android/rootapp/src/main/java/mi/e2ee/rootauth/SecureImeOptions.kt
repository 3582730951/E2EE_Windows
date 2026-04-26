package mi.e2ee.rootauth

import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType

internal fun secureKeyboardOptions(
    keyboardType: KeyboardType = KeyboardType.Text,
    imeAction: ImeAction = ImeAction.Default
): KeyboardOptions = KeyboardOptions(
    autoCorrectEnabled = false,
    keyboardType = keyboardType,
    imeAction = imeAction
)
