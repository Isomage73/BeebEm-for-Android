package uk.org.beebem.android

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
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
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.compose.ui.zIndex
import kotlin.math.roundToInt

// ── Layout constants ───────────────────────────────────────────────────────
private val JOYPAD_HEIGHT = 160.dp   // total area reserved for the gamepad
private val BUTTON_SIZE   = 62.dp    // visual square size of each draggable button
private val CFG_WIDTH     = 56.dp    // CFG toggle bounding box (used for overlap)
private val CFG_HEIGHT    = 24.dp

// ── Colour palette ─────────────────────────────────────────────────────────
private val BODY_BG      = Color(0xFF0E0E1A)

private val DPAD_DARK   = Color(0xFF222238)
private val DPAD_LIT    = Color(0xFF30304C)
private val DPAD_BORDER = Color(0xFF4A4A6A)

private val FIRE_A_DARK = Color(0xFF9A1010);  private val FIRE_A_LIT = Color(0xFFCC2020)
private val FIRE_B_DARK = Color(0xFF102070);  private val FIRE_B_LIT = Color(0xFF1A40AA)
private val FIRE_C_DARK = Color(0xFF0E5020);  private val FIRE_C_LIT = Color(0xFF1A7830)

private val CFG_DARK    = Color(0xFF2C1800)
private val CFG_LIT     = Color(0xFFE0A020)

// ── Per-button visual properties ──────────────────────────────────────────

private data class ButtonStyle(val symbol: String, val dark: Color, val lit: Color, val border: Color)

private val BUTTON_STYLE = mapOf(
    "up"    to ButtonStyle("↑", DPAD_DARK,   DPAD_LIT,   DPAD_BORDER),
    "down"  to ButtonStyle("↓", DPAD_DARK,   DPAD_LIT,   DPAD_BORDER),
    "left"  to ButtonStyle("←", DPAD_DARK,   DPAD_LIT,   DPAD_BORDER),
    "right" to ButtonStyle("→", DPAD_DARK,   DPAD_LIT,   DPAD_BORDER),
    "fire1" to ButtonStyle("A", FIRE_A_DARK, FIRE_A_LIT, FIRE_A_LIT),
    "fire2" to ButtonStyle("B", FIRE_B_DARK, FIRE_B_LIT, FIRE_B_LIT),
    "fire3" to ButtonStyle("C", FIRE_C_DARK, FIRE_C_LIT, FIRE_C_LIT),
)

private fun displayName(id: String) = when (id) {
    "up"    -> "Up";    "down"  -> "Down"
    "left"  -> "Left";  "right" -> "Right"
    "fire1" -> "Fire A"; "fire2" -> "Fire B"; "fire3" -> "Fire C"
    else    -> id
}

private fun keyOf(m: JoypadMapping, id: String): JoypadKey = when (id) {
    "up"    -> m.up;    "down"  -> m.down
    "left"  -> m.left;  "right" -> m.right
    "fire1" -> m.fire1; "fire2" -> m.fire2; "fire3" -> m.fire3
    else    -> m.fire1
}

private fun mappingWithKey(m: JoypadMapping, id: String, k: JoypadKey): JoypadMapping = when (id) {
    "up"    -> m.copy(up    = k);  "down"  -> m.copy(down  = k)
    "left"  -> m.copy(left  = k);  "right" -> m.copy(right = k)
    "fire1" -> m.copy(fire1 = k);  "fire2" -> m.copy(fire2 = k); "fire3" -> m.copy(fire3 = k)
    else    -> m
}

// Standard AABB intersection — touching edges (==) is NOT considered overlap,
// so buttons can sit flush against each other.
private fun aabbOverlaps(
    ax: Float, ay: Float, aw: Float, ah: Float,
    bx: Float, by: Float, bw: Float, bh: Float,
): Boolean = ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by

private fun toggleHidden(layout: JoypadLayout, buttonId: String): JoypadLayout {
    val next = layout.hidden.toMutableSet()
    if (!next.add(buttonId)) next.remove(buttonId)
    return layout.copy(hidden = next)
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

    val latestCallback    by rememberUpdatedState(onConfiguringChanged)
    val latestConfiguring by rememberUpdatedState(configuring)
    DisposableEffect(Unit) {
        onDispose { if (latestConfiguring) latestCallback(false) }
    }

    pickingFor?.let { buttonId ->
        KeyPickerDialog(
            buttonLabel = displayName(buttonId),
            onKeyPicked = { key ->
                onMappingChange(mappingWithKey(mapping, buttonId, key))
                pickingFor = null
            },
            onDismiss = { pickingFor = null },
        )
    }

    Box(
        modifier = modifier
            .background(BODY_BG)
            .height(JOYPAD_HEIGHT)
            .padding(8.dp),
    ) {
        BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
            val density = LocalDensity.current
            val areaWidthDp  = maxWidth.value     // BoxWithConstraints' Dp.value is the dp number
            val areaHeightDp = maxHeight.value
            val btnDp        = BUTTON_SIZE.value
            val cfgWDp       = CFG_WIDTH.value
            val cfgHDp       = CFG_HEIGHT.value

            // CFG sits at top-centre of the content area. Its bounding box is
            // treated as an obstacle for button placement.
            val cfgX = ((areaWidthDp - cfgWDp) / 2f).coerceAtLeast(0f)
            val cfgY = 0f

            // Resolve the effective layout: caller-provided positions overlaid on
            // defaults, then clamped to the area so a config saved on a different
            // screen size doesn't push buttons out of view.
            val effectiveLayout = remember(mapping.layout, areaWidthDp, areaHeightDp) {
                val defaults = defaultJoypadLayout(areaWidthDp, areaHeightDp, btnDp).positions
                val merged = (defaults + mapping.layout.positions).mapValues { (_, p) ->
                    FreePos(
                        xDp = p.xDp.coerceIn(0f, (areaWidthDp  - btnDp).coerceAtLeast(0f)),
                        yDp = p.yDp.coerceIn(0f, (areaHeightDp - btnDp).coerceAtLeast(0f)),
                    )
                }
                JoypadLayout(positions = merged, hidden = mapping.layout.hidden)
            }

            // Check whether moving `buttonId` to `target` would overlap any other
            // visible button or the CFG toggle.
            fun wouldOverlap(buttonId: String, target: FreePos): Boolean {
                if (aabbOverlaps(target.xDp, target.yDp, btnDp, btnDp, cfgX, cfgY, cfgWDp, cfgHDp)) return true
                for ((otherId, otherPos) in effectiveLayout.positions) {
                    if (otherId == buttonId) continue
                    // Hidden buttons are invisible during play, but in CFG mode we
                    // keep their slots reserved so the user can find them to un-hide.
                    if (aabbOverlaps(target.xDp, target.yDp, btnDp, btnDp, otherPos.xDp, otherPos.yDp, btnDp, btnDp)) return true
                }
                return false
            }

            JOYPAD_BUTTON_IDS.forEach { buttonId ->
                val isHidden = buttonId in effectiveLayout.hidden
                if (isHidden && !configuring) return@forEach

                // key(buttonId) gives each button stable Compose identity so
                // its dragOffset / dragging state stay attached even when the
                // forEach skips siblings (e.g. when other buttons get hidden,
                // or when entering / leaving CFG mode reorders the slot table).
                // Without this, recomposition can attach the wrong dragOffset
                // to a button and it visibly jumps to another position.
                key(buttonId) {
                    val pos     = effectiveLayout.positions[buttonId] ?: FreePos(0f, 0f)
                    val style   = BUTTON_STYLE.getValue(buttonId)
                    val joyKey  = keyOf(mapping, buttonId)

                    DraggableJoyButton(
                        buttonId       = buttonId,
                        style          = style,
                        keyLabel       = joyKey.label,
                        pos            = pos,
                        buttonSize     = BUTTON_SIZE,
                        areaWidthDp    = areaWidthDp,
                        areaHeightDp   = areaHeightDp,
                        configuring    = configuring,
                        isHidden       = isHidden,
                        canPlace       = { target -> !wouldOverlap(buttonId, target) },
                        onPlaced       = { newPos ->
                            onMappingChange(mapping.copy(
                                layout = effectiveLayout.copy(
                                    positions = effectiveLayout.positions + (buttonId to newPos)
                                )
                            ))
                        },
                        onTapInConfig  = { pickingFor = buttonId },
                        onDoubleTapInConfig = {
                            onMappingChange(mapping.copy(layout = toggleHidden(effectiveLayout, buttonId)))
                        },
                        onKeyPress = {
                            if (joyKey.shifted) BeebEmNative.nativeKeyDown(0, 0)
                            BeebEmNative.nativeKeyDown(joyKey.row, joyKey.col)
                        },
                        onKeyRelease = {
                            BeebEmNative.nativeKeyUp(joyKey.row, joyKey.col)
                            if (joyKey.shifted) BeebEmNative.nativeKeyUp(0, 0)
                        },
                    )
                }
            }

            // CFG toggle anchored at the top-centre of the content area.
            Box(
                modifier = Modifier
                    .offset { IntOffset(x = with(density) { cfgX.dp.roundToPx() }, y = 0) }
                    .size(CFG_WIDTH, CFG_HEIGHT),
                contentAlignment = Alignment.Center,
            ) {
                CfgToggle(
                    configuring = configuring,
                    onClick = {
                        val next = !configuring
                        configuring = next
                        latestCallback(next)
                    },
                )
            }
        }
    }
}

// ── Draggable button ──────────────────────────────────────────────────────

@Composable
private fun DraggableJoyButton(
    buttonId: String,
    style: ButtonStyle,
    keyLabel: String,
    pos: FreePos,
    buttonSize: Dp,
    areaWidthDp: Float,
    areaHeightDp: Float,
    configuring: Boolean,
    isHidden: Boolean,
    canPlace: (FreePos) -> Boolean,
    onPlaced: (FreePos) -> Unit,
    onTapInConfig: () -> Unit,
    onDoubleTapInConfig: () -> Unit,
    onKeyPress: () -> Unit,
    onKeyRelease: () -> Unit,
) {
    val density = LocalDensity.current
    val btnPx   = with(density) { buttonSize.toPx() }

    var dragOffset by remember { mutableStateOf(Offset.Zero) }
    var dragging   by remember { mutableStateOf(false) }

    // Wrap each callback in rememberUpdatedState so the long-lived pointerInput
    // coroutines (which only restart when their keys change) always invoke the
    // latest version. Without this, after the user moves a button, the parent
    // recomposes with fresh closures, but the tap-detector coroutine keeps its
    // captured lambdas — so a subsequent double-tap on any button would notify
    // the parent using the pre-drag mapping and wipe out the drag's new position.
    val latestCanPlace            by rememberUpdatedState(canPlace)
    val latestOnPlaced            by rememberUpdatedState(onPlaced)
    val latestOnTapInConfig       by rememberUpdatedState(onTapInConfig)
    val latestOnDoubleTapInConfig by rememberUpdatedState(onDoubleTapInConfig)
    val latestOnKeyPress          by rememberUpdatedState(onKeyPress)
    val latestOnKeyRelease        by rememberUpdatedState(onKeyRelease)

    val dark   = if (configuring) CFG_DARK else style.dark
    val lit    = if (configuring) CFG_LIT  else style.lit
    val border = if (configuring) CFG_LIT  else style.border
    val shape  = RoundedCornerShape(12.dp)
    val alpha  = if (isHidden) 0.32f else 1f

    Box(
        modifier = Modifier
            .offset {
                IntOffset(
                    x = (pos.xDp.dp.roundToPx() + dragOffset.x).roundToInt(),
                    y = (pos.yDp.dp.roundToPx() + dragOffset.y).roundToInt(),
                )
            }
            .size(buttonSize)
            .zIndex(if (dragging) 1f else 0f)
            .drawBehind {
                val cr = 12.dp.toPx()
                drawRoundRect(lit.copy(alpha = lit.alpha * alpha), Offset.Zero, size, CornerRadius(cr))
                drawRoundRect(
                    dark.copy(alpha = dark.alpha * alpha),
                    Offset(0f, size.height * 0.20f),
                    Size(size.width, size.height * 0.80f),
                    CornerRadius(cr * 0.65f),
                )
            }
            .border(1.dp, border.copy(alpha = border.alpha * alpha), shape)
            .pointerInput(buttonId, configuring) {
                if (configuring) {
                    detectTapGestures(
                        onTap = { latestOnTapInConfig() },
                        onDoubleTap = { latestOnDoubleTapInConfig() },
                    )
                } else {
                    detectTapGestures(
                        onPress = {
                            latestOnKeyPress()
                            try { awaitRelease() } finally { latestOnKeyRelease() }
                        }
                    )
                }
            }
            .pointerInput(buttonId, configuring, pos, areaWidthDp, areaHeightDp) {
                if (!configuring) return@pointerInput
                detectDragGestures(
                    onDragStart = { dragging = true },
                    onDragCancel = {
                        dragging = false
                        dragOffset = Offset.Zero
                    },
                    onDragEnd = {
                        // Compute the proposed new top-left in dp, clamped to the area.
                        val deltaXDp = with(density) { dragOffset.x.toDp().value }
                        val deltaYDp = with(density) { dragOffset.y.toDp().value }
                        val maxX = (areaWidthDp  - buttonSize.value).coerceAtLeast(0f)
                        val maxY = (areaHeightDp - buttonSize.value).coerceAtLeast(0f)
                        val proposed = FreePos(
                            xDp = (pos.xDp + deltaXDp).coerceIn(0f, maxX),
                            yDp = (pos.yDp + deltaYDp).coerceIn(0f, maxY),
                        )
                        dragOffset = Offset.Zero
                        dragging = false
                        if (latestCanPlace(proposed)) {
                            latestOnPlaced(proposed)
                        }
                        // else: dragOffset already reset → button visually bounces back.
                    },
                    onDrag = { change, delta ->
                        change.consume()
                        dragOffset += delta
                    },
                )
            },
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Text(
                style.symbol,
                color = Color.White.copy(alpha = alpha),
                fontSize = 22.sp,
                fontWeight = FontWeight.Bold,
                lineHeight = 24.sp,
            )
            Text(
                if (isHidden) "hidden" else keyLabel,
                color = Color(0xFF9999BB),
                fontSize = 7.sp,
                lineHeight = 8.sp,
            )
        }
    }
}

// ── CFG toggle (anchored, not draggable) ──────────────────────────────────

@Composable
private fun CfgToggle(
    configuring: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Text(
        text = if (configuring) "DONE" else "CFG",
        fontSize = 9.sp,
        fontWeight = FontWeight.Bold,
        color = if (configuring) CFG_LIT else Color(0xFF666688),
        modifier = modifier
            .background(
                if (configuring) Color(0xFF221400) else Color(0xFF1A1A2A),
                RoundedCornerShape(4.dp),
            )
            .border(
                0.5.dp,
                if (configuring) CFG_LIT else Color(0xFF303044),
                RoundedCornerShape(4.dp),
            )
            .padding(horizontal = 8.dp, vertical = 4.dp)
            .pointerInput(Unit) {
                detectTapGestures { onClick() }
            },
    )
}

// ── Key picker dialog ──────────────────────────────────────────────────────

@Composable
private fun KeyPickerDialog(
    buttonLabel: String,
    onKeyPicked: (JoypadKey) -> Unit,
    onDismiss: () -> Unit,
) {
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
