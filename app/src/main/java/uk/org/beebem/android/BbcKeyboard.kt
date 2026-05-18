package uk.org.beebem.android

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

// BBC key: label, BBC matrix row/col, width relative to a standard key,
// and the character produced when the Shift key is held (shown on the key when Shift is active).
// row == -99 signals BREAK (uses nativeBreakKey).
private data class BbcKey(
    val label: String,
    val row: Int,
    val col: Int,
    val weight: Float = 1f,
    val shiftLabel: String = "",   // shown on key face when Shift is active
)

private val BREAK_KEY = BbcKey("BRK", -99, -99, 1.5f)

private val ROWS: List<List<BbcKey>> = listOf(
    // Function keys + BREAK
    listOf(
        BbcKey("f0", 2, 0), BbcKey("f1", 7, 1), BbcKey("f2", 7, 2), BbcKey("f3", 7, 3),
        BbcKey("f4", 1, 4), BbcKey("f5", 7, 4), BbcKey("f6", 7, 5), BbcKey("f7", 1, 6),
        BbcKey("f8", 7, 6), BbcKey("f9", 7, 7), BREAK_KEY,
    ),
    // Number row — shifted characters shown on key face when Shift is active.
    // "^" (row 1 col 8): SHIFT+^ = ~ (0x7E), rendered as ÷ in BBC Mode 7.
    // "_" (row 2 col 8): SHIFT+_ = £.
    listOf(
        BbcKey("ESC", 7, 0, 1.5f),
        BbcKey("1",  3, 0, shiftLabel = "!"),
        BbcKey("2",  3, 1, shiftLabel = "\""),
        BbcKey("3",  1, 1, shiftLabel = "#"),
        BbcKey("4",  1, 2, shiftLabel = "$"),
        BbcKey("5",  1, 3, shiftLabel = "%"),
        BbcKey("6",  3, 4, shiftLabel = "&"),
        BbcKey("7",  2, 4, shiftLabel = "'"),
        BbcKey("8",  1, 5, shiftLabel = "("),
        BbcKey("9",  2, 6, shiftLabel = ")"),
        BbcKey("0",  2, 7),
        BbcKey("-",  1, 7, shiftLabel = "="),
        BbcKey("^",  1, 8, shiftLabel = "÷"),
        BbcKey("_",  2, 8, shiftLabel = "£"),
        BbcKey("DEL", 5, 9, 1.5f),
    ),
    // QWERTY row — "@" (row 4 col 7) sits between P and [ as on the physical BBC keyboard.
    listOf(
        BbcKey("TAB", 6, 0, 1.5f),
        BbcKey("Q", 1, 0), BbcKey("W", 2, 1), BbcKey("E", 2, 2),
        BbcKey("R", 3, 3), BbcKey("T", 2, 3), BbcKey("Y", 4, 4), BbcKey("U", 3, 5),
        BbcKey("I", 2, 5), BbcKey("O", 3, 6), BbcKey("P", 3, 7),
        BbcKey("@", 4, 7),
        BbcKey("[", 3, 8, shiftLabel = "{"),
        BbcKey("]", 5, 8, shiftLabel = "}"),
        BbcKey("\\", 7, 8, shiftLabel = "|"),
        BbcKey("RET", 4, 9, 1.5f),
    ),
    // ASDF row — ":" (row 4 col 8): SHIFT+: = *.
    listOf(
        BbcKey("CAPS", 4, 0, 1.5f),
        BbcKey("A", 4, 1), BbcKey("S", 5, 1), BbcKey("D", 3, 2),
        BbcKey("F", 4, 3), BbcKey("G", 5, 3), BbcKey("H", 5, 4),
        BbcKey("J", 4, 5), BbcKey("K", 4, 6), BbcKey("L", 5, 6),
        BbcKey(";", 5, 7, shiftLabel = "+"),
        BbcKey(":", 4, 8, shiftLabel = "*"),
        BbcKey("COPY", 6, 9, 1.5f),
    ),
    // ZXCV row
    listOf(
        BbcKey("SHF", 0, 0, 2f),
        BbcKey("Z", 6, 1), BbcKey("X", 4, 2), BbcKey("C", 5, 2),
        BbcKey("V", 6, 3), BbcKey("B", 6, 4), BbcKey("N", 5, 5),
        BbcKey("M", 6, 5),
        BbcKey(",", 6, 6, shiftLabel = "<"),
        BbcKey(".", 6, 7, shiftLabel = ">"),
        BbcKey("/", 6, 8, shiftLabel = "?"),
        BbcKey("SHF", 0, 0, 2f),
    ),
    // Bottom row
    listOf(
        BbcKey("CTL", 0, 1, 1.5f),
        BbcKey("SPACE", 6, 2, 7f),
        BbcKey("↑", 3, 9), BbcKey("↓", 2, 9), BbcKey("←", 1, 9), BbcKey("→", 7, 9),
    ),
)

// BBC key colour scheme:
//   Red   — function keys and BREAK (matches the original BBC Micro hardware)
//   Cream — alphanumeric + space + symbols (the main keycap colour on the real keyboard)
//   Gray  — modifier and control keys (ESC, TAB, SHIFT, CAPS, CTRL, DEL, RET, COPY, arrows)

private val KEY_RED_BG       = Color(0xFFBB1F1F)
private val KEY_RED_BG_LIT   = Color(0xFFDD3030)
private val KEY_CREAM_BG     = Color(0xFFDDD3B0)
private val KEY_CREAM_BG_LIT = Color(0xFFEDE3C0)
private val KEY_GRAY_BG      = Color(0xFF787868)
private val KEY_GRAY_BG_LIT  = Color(0xFF989882)
private val KEY_ACTIVE_BG    = Color(0xFFE0A020)
private val KEY_ACTIVE_LIT   = Color(0xFFF0C040)

private val KEY_TEXT_LIGHT   = Color(0xFFF0F0F0)
private val KEY_TEXT_DARK    = Color(0xFF1A1A14)

private val SPECIAL_LABELS = setOf("ESC", "TAB", "DEL", "RET", "CAPS", "SHF", "CTL", "COPY",
    "↑", "↓", "←", "→")

private fun isFunctionKey(key: BbcKey) =
    key.row == -99 || (key.label.length == 2 && key.label[0] == 'f' && key.label[1].isDigit())

private fun isModifierKey(key: BbcKey) = key.label == "SHF" || key.label == "CTL"

private fun keyColors(key: BbcKey, isActive: Boolean): Triple<Color, Color, Color> {
    if (isActive) return Triple(KEY_ACTIVE_BG, KEY_ACTIVE_LIT, KEY_TEXT_DARK)
    if (isFunctionKey(key)) return Triple(KEY_RED_BG, KEY_RED_BG_LIT, KEY_TEXT_LIGHT)
    if (key.label in SPECIAL_LABELS) return Triple(KEY_GRAY_BG, KEY_GRAY_BG_LIT, KEY_TEXT_DARK)
    return Triple(KEY_CREAM_BG, KEY_CREAM_BG_LIT, KEY_TEXT_DARK)
}

@Composable
fun BbcKeyboard(modifier: Modifier = Modifier) {
    // Press counters allow either of the two SHF keys (and multi-touch in general)
    // to keep the modifier latched while held — only the 0→1 and 1→0 transitions
    // dispatch nativeKeyDown/Up, so the BBC matrix sees a single clean press.
    var shiftPressCount by remember { mutableStateOf(0) }
    var ctrlPressCount  by remember { mutableStateOf(0) }

    val shiftActive = shiftPressCount > 0
    val ctrlActive  = ctrlPressCount  > 0

    fun isActive(key: BbcKey): Boolean = when (key.label) {
        "SHF" -> shiftActive
        "CTL" -> ctrlActive
        else  -> false
    }

    fun onModifierPress(label: String) {
        when (label) {
            "SHF" -> {
                if (shiftPressCount == 0) BeebEmNative.nativeKeyDown(0, 0)
                shiftPressCount++
            }
            "CTL" -> {
                if (ctrlPressCount == 0) BeebEmNative.nativeKeyDown(0, 1)
                ctrlPressCount++
            }
        }
    }

    fun onModifierRelease(label: String) {
        when (label) {
            "SHF" -> {
                shiftPressCount = (shiftPressCount - 1).coerceAtLeast(0)
                if (shiftPressCount == 0) BeebEmNative.nativeKeyUp(0, 0)
            }
            "CTL" -> {
                ctrlPressCount = (ctrlPressCount - 1).coerceAtLeast(0)
                if (ctrlPressCount == 0) BeebEmNative.nativeKeyUp(0, 1)
            }
        }
    }

    Column(
        modifier = modifier.background(Color(0xFF1A1814)),
        verticalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        for (row in ROWS) {
            KeyRow(
                keys = row,
                shiftActive = shiftActive,
                isActive = ::isActive,
                onModifierPress = ::onModifierPress,
                onModifierRelease = ::onModifierRelease,
            )
        }
    }
}

@Composable
private fun KeyRow(
    keys: List<BbcKey>,
    shiftActive: Boolean,
    isActive: (BbcKey) -> Boolean,
    onModifierPress: (String) -> Unit,
    onModifierRelease: (String) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(32.dp),
        horizontalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        for (key in keys) {
            KeyButton(
                key = key,
                isActive = isActive(key),
                shiftActive = shiftActive,
                onModifierPress = onModifierPress,
                onModifierRelease = onModifierRelease,
                modifier = Modifier.weight(key.weight),
            )
        }
    }
}

@Composable
private fun KeyButton(
    key: BbcKey,
    isActive: Boolean,
    shiftActive: Boolean,
    onModifierPress: (String) -> Unit,
    onModifierRelease: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    val (bgDark, bgLight, textColor) = keyColors(key, isActive)
    val radius = 3.dp
    val displayLabel = if (shiftActive && key.shiftLabel.isNotEmpty()) key.shiftLabel else key.label

    Box(
        contentAlignment = Alignment.Center,
        modifier = modifier
            .fillMaxHeight()
            .drawBehind {
                val cr = radius.toPx()
                drawRoundRect(
                    color = bgLight,
                    topLeft = Offset(0f, 0f),
                    size = Size(size.width, size.height),
                    cornerRadius = CornerRadius(cr, cr),
                )
                drawRoundRect(
                    color = bgDark,
                    topLeft = Offset(0f, size.height * 0.18f),
                    size = Size(size.width, size.height * 0.84f),
                    cornerRadius = CornerRadius(cr * 0.6f, cr * 0.6f),
                )
            }
            .border(0.5.dp, bgLight.copy(alpha = 0.5f), RoundedCornerShape(radius))
            .pointerInput(key) {
                detectTapGestures(
                    onPress = { _ ->
                        if (key.row == -99) {
                            BeebEmNative.nativeBreakKey(false)
                            return@detectTapGestures
                        }
                        if (isModifierKey(key)) {
                            onModifierPress(key.label)
                            try {
                                awaitRelease()
                            } finally {
                                onModifierRelease(key.label)
                            }
                        } else {
                            BeebEmNative.nativeKeyDown(key.row, key.col)
                            try {
                                awaitRelease()
                            } finally {
                                BeebEmNative.nativeKeyUp(key.row, key.col)
                            }
                        }
                    }
                )
            }
    ) {
        Text(
            text = displayLabel,
            color = textColor,
            fontSize = 8.sp,
            fontWeight = FontWeight.Bold,
            textAlign = TextAlign.Center,
            maxLines = 1,
        )
    }
}
