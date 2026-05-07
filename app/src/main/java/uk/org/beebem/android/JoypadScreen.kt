package uk.org.beebem.android

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
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
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties

// ── Layout constants ───────────────────────────────────────────────────────
private val GAP = 6.dp  // gap between buttons within a cluster

// ── Colour palette ─────────────────────────────────────────────────────────
private val BODY_BG      = Color(0xFF0E0E1A)
private val PANEL_BG     = Color(0xFF1A1A2C)
private val PANEL_BORDER = Color(0xFF282840)

private val DPAD_DARK   = Color(0xFF222238)
private val DPAD_LIT    = Color(0xFF30304C)
private val DPAD_BORDER = Color(0xFF4A4A6A)

private val FIRE_A_DARK = Color(0xFF9A1010);  private val FIRE_A_LIT = Color(0xFFCC2020)
private val FIRE_B_DARK = Color(0xFF102070);  private val FIRE_B_LIT = Color(0xFF1A40AA)
private val FIRE_C_DARK = Color(0xFF0E5020);  private val FIRE_C_LIT = Color(0xFF1A7830)

private val CFG_DARK    = Color(0xFF2C1800)
private val CFG_LIT     = Color(0xFFE0A020)

// ── Helpers ────────────────────────────────────────────────────────────────

private fun displayName(id: String) = when (id) {
    "up"    -> "Up"
    "down"  -> "Down"
    "left"  -> "Left"
    "right" -> "Right"
    "fire1" -> "Fire A"
    "fire2" -> "Fire B"
    "fire3" -> "Fire C"
    else    -> id
}

// ── Top-level composable ───────────────────────────────────────────────────

@Composable
fun JoypadScreen(
    mapping: JoypadMapping,
    onMappingChange: (JoypadMapping) -> Unit,
    onConfiguringChanged: (Boolean) -> Unit = {},
    modifier: Modifier = Modifier,
) {
    var configuring by remember { mutableStateOf(false) }
    var pickingFor  by remember { mutableStateOf<String?>(null) }

    // Pause the emulator while in config mode, resume when done or when this
    // composable leaves composition (e.g. user switches to keyboard mode).
    val latestCallback  by rememberUpdatedState(onConfiguringChanged)
    val latestConfiguring by rememberUpdatedState(configuring)
    DisposableEffect(Unit) {
        onDispose { if (latestConfiguring) latestCallback(false) }
    }

    pickingFor?.let { buttonId ->
        KeyPickerDialog(
            buttonLabel = displayName(buttonId),
            onKeyPicked = { key ->
                onMappingChange(
                    when (buttonId) {
                        "up"    -> mapping.copy(up    = key)
                        "down"  -> mapping.copy(down  = key)
                        "left"  -> mapping.copy(left  = key)
                        "right" -> mapping.copy(right = key)
                        "fire1" -> mapping.copy(fire1 = key)
                        "fire2" -> mapping.copy(fire2 = key)
                        "fire3" -> mapping.copy(fire3 = key)
                        else    -> mapping
                    }
                )
                pickingFor = null
                // configuring intentionally NOT cleared here — user stays in
                // reprogram mode until they tap CFG/DONE again, so they can
                // remap all buttons in one session.
            },
            onDismiss = { pickingFor = null },
        )
    }

    Row(
        modifier = modifier
            .background(BODY_BG)
            .padding(horizontal = 10.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        // ── Left panel: inverted-T  (up on top, left+right below) ──────
        Panel(Modifier.weight(1f)) {
            val btnSize = (maxWidth - GAP) / 2f
            DPad(
                upKey    = mapping.up,
                leftKey  = mapping.left,
                rightKey = mapping.right,
                btnSize  = btnSize,
                configuring = configuring,
                onConfigure = { btn -> pickingFor = btn },
            )
        }

        // ── Centre: branding + CFG toggle ──────────────────────────────
        Column(
            modifier = Modifier.padding(horizontal = 10.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            Text("BBC",   fontSize = 9.sp, fontWeight = FontWeight.Bold,
                color = Color(0xFF3A3A5A), letterSpacing = 3.sp)
            Text("MICRO", fontSize = 6.sp, fontWeight = FontWeight.Bold,
                color = Color(0xFF2A2A44), letterSpacing = 2.sp)
            Spacer(Modifier.height(8.dp))

            val cfgActive = configuring
            Text(
                text = if (cfgActive) "DONE" else "CFG",
                fontSize = 8.sp,
                fontWeight = FontWeight.Bold,
                color = if (cfgActive) CFG_LIT else Color(0xFF444466),
                modifier = Modifier
                    .background(
                        if (cfgActive) Color(0xFF221400) else Color(0xFF1A1A2A),
                        RoundedCornerShape(4.dp),
                    )
                    .border(
                        0.5.dp,
                        if (cfgActive) CFG_LIT else Color(0xFF303044),
                        RoundedCornerShape(4.dp),
                    )
                    .padding(horizontal = 8.dp, vertical = 5.dp)
                    .pointerInput(Unit) {
                        detectTapGestures {
                            val next = !configuring
                            configuring = next
                            latestCallback(next)
                        }
                    },
            )
        }

        // ── Right panel: L-shape  (A top-left, B bottom-left, C bottom-right) ─
        Panel(Modifier.weight(1f)) {
            val btnSize = (maxWidth - GAP) / 2f
            FireCluster(
                fire1Key = mapping.fire1,
                fire2Key = mapping.fire2,
                fire3Key = mapping.fire3,
                btnSize  = btnSize,
                configuring = configuring,
                onConfigure = { btn -> pickingFor = btn },
            )
        }
    }
}

// Shared panel chrome — rounded card with padding; content measured via BoxWithConstraints.
@Composable
private fun Panel(
    modifier: Modifier = Modifier,
    content: @Composable BoxWithConstraintsScope.() -> Unit,
) {
    Box(
        modifier = modifier
            .background(PANEL_BG, RoundedCornerShape(18.dp))
            .border(1.dp, PANEL_BORDER, RoundedCornerShape(18.dp))
            .padding(12.dp),
        contentAlignment = Alignment.Center,
    ) {
        BoxWithConstraints(
            modifier = Modifier.fillMaxWidth(),
            contentAlignment = Alignment.Center,
            content = content,
        )
    }
}

// ── D-pad — inverted T ─────────────────────────────────────────────────────
//
//      [UP]
//  [LEFT][RIGHT]
//
@Composable
private fun DPad(
    upKey: JoypadKey, leftKey: JoypadKey, rightKey: JoypadKey,
    btnSize: Dp,
    configuring: Boolean,
    onConfigure: (String) -> Unit,
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(GAP),
    ) {
        JoyButton("↑", upKey,    "up",    DPAD_DARK, DPAD_LIT, DPAD_BORDER, btnSize, configuring, onConfigure)
        Row(horizontalArrangement = Arrangement.spacedBy(GAP)) {
            JoyButton("←", leftKey,  "left",  DPAD_DARK, DPAD_LIT, DPAD_BORDER, btnSize, configuring, onConfigure)
            JoyButton("→", rightKey, "right", DPAD_DARK, DPAD_LIT, DPAD_BORDER, btnSize, configuring, onConfigure)
        }
    }
}

// ── Fire cluster — L shape ─────────────────────────────────────────────────
//
//  [A]
//  [B][C]
//
@Composable
private fun FireCluster(
    fire1Key: JoypadKey, fire2Key: JoypadKey, fire3Key: JoypadKey,
    btnSize: Dp,
    configuring: Boolean,
    onConfigure: (String) -> Unit,
) {
    Column(
        horizontalAlignment = Alignment.Start,
        verticalArrangement = Arrangement.spacedBy(GAP),
    ) {
        JoyButton("A", fire1Key, "fire1", FIRE_A_DARK, FIRE_A_LIT, FIRE_A_LIT, btnSize, configuring, onConfigure)
        Row(horizontalArrangement = Arrangement.spacedBy(GAP)) {
            JoyButton("B", fire2Key, "fire2", FIRE_B_DARK, FIRE_B_LIT, FIRE_B_LIT, btnSize, configuring, onConfigure)
            JoyButton("C", fire3Key, "fire3", FIRE_C_DARK, FIRE_C_LIT, FIRE_C_LIT, btnSize, configuring, onConfigure)
        }
    }
}

// ── Unified square button with top-edge bevel ─────────────────────────────

@Composable
private fun JoyButton(
    symbol: String,
    key: JoypadKey,
    buttonId: String,
    darkColor: Color,
    litColor: Color,
    borderColor: Color,
    btnSize: Dp,
    configuring: Boolean,
    onConfigure: (String) -> Unit,
) {
    val dark   = if (configuring) CFG_DARK  else darkColor
    val lit    = if (configuring) CFG_LIT   else litColor
    val border = if (configuring) CFG_LIT   else borderColor
    val shape  = RoundedCornerShape(10.dp)

    Box(
        modifier = Modifier
            .size(btnSize)
            .drawBehind {
                val cr = 10.dp.toPx()
                drawRoundRect(lit, Offset.Zero, size, CornerRadius(cr))
                drawRoundRect(
                    dark,
                    Offset(0f, size.height * 0.20f),
                    Size(size.width, size.height * 0.80f),
                    CornerRadius(cr * 0.65f),
                )
            }
            .border(1.dp, border, shape)
            .pointerInput(key, configuring) {
                detectTapGestures(
                    onPress = {
                        if (configuring) {
                            onConfigure(buttonId)
                        } else {
                            if (key.shifted) BeebEmNative.nativeKeyDown(0, 0)
                            BeebEmNative.nativeKeyDown(key.row, key.col)
                            try {
                                awaitRelease()
                            } finally {
                                BeebEmNative.nativeKeyUp(key.row, key.col)
                                if (key.shifted) BeebEmNative.nativeKeyUp(0, 0)
                            }
                        }
                    }
                )
            },
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(symbol,    color = Color.White,       fontSize = 20.sp, fontWeight = FontWeight.Bold, lineHeight = 22.sp)
            Text(key.label, color = Color(0xFF9999BB), fontSize = 7.sp,  lineHeight = 8.sp)
        }
    }
}

// ── Key picker dialog ──────────────────────────────────────────────────────

@Composable
private fun KeyPickerDialog(
    buttonLabel: String,
    onKeyPicked: (JoypadKey) -> Unit,
    onDismiss: () -> Unit,
) {
    // Zoom: 1.0 = compact (fits without scrolling), up to 2.5 = large/readable.
    var zoom by remember { mutableStateOf(1f) }
    val keyW    = (22.dp  * zoom)
    val rowH    = (28.dp  * zoom)
    val keyGap  = (2.dp   * zoom).coerceAtLeast(1.dp)
    val rowGap  = (3.dp   * zoom).coerceAtLeast(2.dp)
    val keyFont = (7f     * zoom).sp

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false),
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .fillMaxHeight(0.92f)
                .padding(horizontal = 12.dp)
                .background(Color(0xFF1A1A2E), RoundedCornerShape(12.dp))
                .padding(12.dp),
        ) {
            // ── Header: title + zoom controls ─────────────────────────
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    "Map: $buttonLabel",
                    color = Color.White,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.weight(1f),
                )
                TextButton(
                    onClick = { zoom = (zoom - 0.25f).coerceAtLeast(0.75f) },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                ) {
                    Text("−", color = Color(0xFF8888AA), fontSize = 14.sp)
                }
                Text(
                    "${(zoom * 100).toInt()}%",
                    color = Color(0xFF666688),
                    fontSize = 9.sp,
                    modifier = Modifier.width(32.dp),
                    textAlign = TextAlign.Center,
                )
                TextButton(
                    onClick = { zoom = (zoom + 0.25f).coerceAtMost(2.5f) },
                    contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                ) {
                    Text("+", color = Color(0xFF8888AA), fontSize = 14.sp)
                }
            }

            Spacer(Modifier.height(8.dp))

            // ── Keyboard grid (horizontal + vertical scroll) ──────────
            Box(
                modifier = Modifier
                    .weight(1f)
                    .horizontalScroll(rememberScrollState())
                    .verticalScroll(rememberScrollState()),
            ) {
                Column(verticalArrangement = Arrangement.spacedBy(rowGap)) {
                    PICKER_KEY_ROWS.forEach { row ->
                        Row(
                            modifier = Modifier.height(rowH),
                            horizontalArrangement = Arrangement.spacedBy(keyGap),
                        ) {
                            row.forEach { key ->
                                Box(
                                    modifier = Modifier
                                        .width(keyW)
                                        .fillMaxHeight()
                                        .background(
                                            if (key.shifted) Color(0xFF1A2A5E) else Color(0xFF2A2A5E),
                                            RoundedCornerShape(3.dp),
                                        )
                                        .border(
                                            0.5.dp,
                                            if (key.shifted) Color(0xFF334488) else Color(0xFF444477),
                                            RoundedCornerShape(3.dp),
                                        )
                                        .pointerInput(key) {
                                            detectTapGestures { onKeyPicked(key) }
                                        },
                                    contentAlignment = Alignment.Center,
                                ) {
                                    Text(
                                        text = key.label,
                                        color = if (key.shifted) Color(0xFFBBBBDD) else Color.White,
                                        fontSize = keyFont,
                                        textAlign = TextAlign.Center,
                                        maxLines = 1,
                                    )
                                }
                            }
                        }
                    }
                }
            }

            Spacer(Modifier.height(6.dp))
            TextButton(onClick = onDismiss, modifier = Modifier.align(Alignment.End)) {
                Text("Cancel", color = Color(0xFF8888AA), fontSize = 11.sp)
            }
        }
    }
}
