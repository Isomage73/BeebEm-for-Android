package uk.org.beebem.android

import android.content.res.Configuration
import android.os.Bundle
import android.util.Log
import android.view.KeyEvent
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import uk.org.beebem.android.ui.theme.BeebDroidTheme
import java.io.File

enum class InputMode { NONE, KEYBOARD, JOYPAD }

class MainActivity : ComponentActivity() {

    private var glView: BeebGLSurfaceView? = null

    // Set to true by any screen that contains a text input (e.g. BbcSearchScreen).
    // dispatchKeyEvent defers to Compose while this is true so typing works normally.
    internal var textInputActive = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val ok = BeebEmNative.nativeInit(assets, filesDir.absolutePath)
        Log.i("BeebDroid", "nativeInit returned $ok")

        setContent {
            BeebDroidTheme {
                if (ok) {
                    EmulatorLayout(onViewCreated = { glView = it })
                }
            }
        }
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (textInputActive) return super.dispatchKeyEvent(event)

        val bbc = HARDWARE_KEY_MAP[event.keyCode] ?: return super.dispatchKeyEvent(event)

        if (bbc == BBC_BREAK) {
            if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                val withShift = event.keyCode == KeyEvent.KEYCODE_F12 ||
                        event.isShiftPressed
                BeebEmNative.nativeBreakKey(withShift)
            }
            return true
        }

        when (event.action) {
            KeyEvent.ACTION_DOWN -> if (event.repeatCount == 0)
                BeebEmNative.nativeKeyDown(bbc.row, bbc.col)
            KeyEvent.ACTION_UP   -> BeebEmNative.nativeKeyUp(bbc.row, bbc.col)
        }
        return true
    }

    override fun onResume() {
        super.onResume()
        glView?.resumeEmulation()
    }

    override fun onPause() {
        super.onPause()
        glView?.pauseEmulation()
    }

    override fun onDestroy() {
        super.onDestroy()
        BeebEmNative.nativeShutdown()
    }
}

@Composable
private fun EmulatorLayout(onViewCreated: (BeebGLSurfaceView) -> Unit) {
    val context = LocalContext.current
    val view = remember { BeebGLSurfaceView(context).also { onViewCreated(it) } }
    val scope = rememberCoroutineScope()

    val isLandscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE

    var inputMode by remember { mutableStateOf(if (isLandscape) InputMode.KEYBOARD else InputMode.NONE) }

    var disc0LocalFile by remember { mutableStateOf<File?>(null) }
    val disc0Name = disc0LocalFile?.name

    var showGamePicker  by remember { mutableStateOf(false) }
    var showSearch      by remember { mutableStateOf(false) }
    var showMenu        by remember { mutableStateOf(false) }
    var showStatePicker by remember { mutableStateOf(false) }
    var savedStates     by remember { mutableStateOf<List<java.io.File>>(emptyList()) }

    val joypadConfigPair = remember { JoypadConfigManager.load(context) }
    var joypadDefault by remember { mutableStateOf(joypadConfigPair.first) }
    val joypadDiscMappings = remember { joypadConfigPair.second }
    var currentJoypadMapping by remember { mutableStateOf(joypadConfigPair.first) }

    fun onMappingChanged(newMapping: JoypadMapping) {
        currentJoypadMapping = newMapping
        val discName = disc0LocalFile?.name
        if (discName != null) joypadDiscMappings[discName] = newMapping
        else                  joypadDefault = newMapping
        scope.launch(Dispatchers.IO) {
            JoypadConfigManager.save(context, joypadDefault, joypadDiscMappings)
        }
    }

    val pickDisc = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        scope.launch {
            val local = withContext(Dispatchers.IO) {
                DiscManager.copyUriToPrivate(context, uri)
            } ?: return@launch
            val ok = BeebEmNative.nativeMountDisc(0, local.absolutePath, false)
            if (ok) {
                disc0LocalFile = local
                currentJoypadMapping = joypadDiscMappings[local.name] ?: joypadDefault
            }
        }
    }

    val saveDisc = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("*/*")
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        val local = disc0LocalFile ?: return@rememberLauncherForActivityResult
        scope.launch {
            BeebEmNative.nativeFlushDisc(0)
            withContext(Dispatchers.IO) { DiscManager.copyPrivateToUri(context, local, uri) }
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        Column(modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
            .systemBarsPadding()
        ) {
            // Compact toolbar — always fits in portrait.
            // Disc operations are in the ⋮ overflow menu.
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(30.dp)
                    .background(Color(0xFF111122)),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                ToolbarButton(
                    label       = "⋮",  // ⋮
                    highlighted = showMenu,
                ) { showMenu = !showMenu }

                ToolbarButton("GAMES") { showGamePicker = true }
                ToolbarButton("SEARCH") { showSearch = true }

                Spacer(modifier = Modifier.weight(1f))

                ToolbarButton(
                    label       = "KB",
                    highlighted = inputMode == InputMode.KEYBOARD,
                ) {
                    inputMode = if (inputMode == InputMode.KEYBOARD) InputMode.NONE else InputMode.KEYBOARD
                }
                ToolbarButton(
                    label       = "PAD",
                    highlighted = inputMode == InputMode.JOYPAD,
                ) {
                    inputMode = if (inputMode == InputMode.JOYPAD) InputMode.NONE else InputMode.JOYPAD
                }
            }

            // Video — takes all remaining vertical space.
            AndroidView(
                factory  = { view },
                modifier = Modifier.weight(1f).fillMaxWidth(),
            )

            AnimatedVisibility(visible = inputMode == InputMode.KEYBOARD) {
                BbcKeyboard(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 2.dp, vertical = 2.dp),
                )
            }
            AnimatedVisibility(visible = inputMode == InputMode.JOYPAD) {
                JoypadScreen(
                    mapping              = currentJoypadMapping,
                    onMappingChange      = ::onMappingChanged,
                    onConfiguringChanged = { paused ->
                        if (paused) view.softPause() else view.softResume()
                    },
                    modifier             = Modifier.fillMaxWidth(),
                )
            }
        }

        // Overflow menu — floating overlay so the GLSurfaceView never resizes.
        // Pauses emulation while open via softPause (non-blocking queueEvent).
        if (showMenu) {
            DisposableEffect(Unit) {
                view.softPause()
                onDispose { view.softResume() }
            }
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .systemBarsPadding(),
            ) {
                Column(
                    modifier = Modifier
                        .align(Alignment.TopStart)
                        .padding(top = 30.dp)
                        .fillMaxWidth()
                        .background(Color(0xFF0E0E1E)),
                ) {
                    HorizontalDivider(color = Color(0xFF1E1E30))
                    MenuButton("SHF+BRK") {
                        showMenu = false
                        BeebEmNative.nativeBreakKey(true)
                        scope.launch {
                            delay(1500)
                            BeebEmNative.nativeKeyUp(0, 0)
                        }
                    }
                    MenuButton("Load Disc") {
                        showMenu = false
                        pickDisc.launch(arrayOf("*/*"))
                    }
                    MenuButton(
                        label   = "Save Disc",
                        enabled = disc0LocalFile != null,
                    ) {
                        showMenu = false
                        saveDisc.launch(disc0Name ?: "disc.ssd")
                    }
                    if (disc0LocalFile != null) {
                        MenuButton("Eject D0") {
                            showMenu = false
                            BeebEmNative.nativeEjectDisc(0)
                            disc0LocalFile = null
                            currentJoypadMapping = joypadDefault
                        }
                    }
                    HorizontalDivider(color = Color(0xFF1E1E30))
                    MenuButton("Save Snapshot") {
                        val path = StateManager.generatePath(context)
                        BeebEmNative.nativeSaveState(path)
                        showMenu = false
                    }
                    MenuButton("Load Snapshot") {
                        savedStates = StateManager.listStates(context)
                        showMenu = false
                        showStatePicker = true
                    }
                    HorizontalDivider(color = Color(0xFF1E1E30))
                }
            }
        }

        // Game picker — rendered as an in-window overlay so it shares the same
        // Android Window as the GLSurface. Using Dialog {} creates a separate
        // Window that disrupts vsync on the GL render thread.
        if (showGamePicker) {
            DisposableEffect(Unit) {
                view.softPause()
                onDispose { view.softResume() }
            }
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .systemBarsPadding(),
            ) {
                GamePickerScreen(
                    onGameMounted = { file ->
                        disc0LocalFile = file
                        currentJoypadMapping = joypadDiscMappings[file.name] ?: joypadDefault
                        scope.launch {
                            delay(300)
                            BeebEmNative.nativeBreakKey(true)
                            delay(1500)
                            BeebEmNative.nativeKeyUp(0, 0)
                        }
                    },
                    onDismiss = { showGamePicker = false },
                )
            }
        }

        // BBC Micro archive search — in-window overlay, same pattern as game picker.
        if (showSearch) {
            DisposableEffect(Unit) {
                view.softPause()
                onDispose { view.softResume() }
            }
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .systemBarsPadding(),
            ) {
                BbcSearchScreen(
                    onGameMounted = { file ->
                        disc0LocalFile = file
                        currentJoypadMapping = joypadDiscMappings[file.name] ?: joypadDefault
                        scope.launch {
                            delay(300)
                            BeebEmNative.nativeBreakKey(true)
                            delay(1500)
                            BeebEmNative.nativeKeyUp(0, 0)
                        }
                    },
                    onDismiss = { showSearch = false },
                )
            }
        }

        // State picker — in-window overlay, same pattern as game picker.
        if (showStatePicker) {
            DisposableEffect(Unit) {
                view.softPause()
                onDispose { view.softResume() }
            }
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .systemBarsPadding(),
            ) {
                StatePickerScreen(
                    states   = savedStates,
                    onLoad   = { file ->
                        BeebEmNative.nativeLoadState(file.absolutePath)
                        showStatePicker = false
                    },
                    onDelete = { file ->
                        StateManager.delete(file)
                        savedStates = savedStates - file
                    },
                    onDismiss = { showStatePicker = false },
                )
            }
        }
    }
}

@Composable
private fun ToolbarButton(
    label:       String,
    enabled:     Boolean = true,
    highlighted: Boolean = false,
    onClick:     () -> Unit,
) {
    TextButton(
        onClick        = onClick,
        enabled        = enabled,
        contentPadding = PaddingValues(horizontal = 6.dp, vertical = 0.dp),
    ) {
        Text(
            label,
            color = when {
                highlighted -> Color(0xFFE0A020)
                enabled     -> Color(0xFFAAAAAA)
                else        -> Color(0xFF555566)
            },
            fontSize = 10.sp,
            maxLines = 1,
        )
    }
}

@Composable
private fun MenuButton(
    label:   String,
    enabled: Boolean = true,
    onClick: () -> Unit,
) {
    TextButton(
        onClick        = onClick,
        enabled        = enabled,
        modifier       = Modifier.fillMaxWidth(),
        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 2.dp),
    ) {
        Text(
            label,
            color    = if (enabled) Color(0xFFCCCCCC) else Color(0xFF555566),
            fontSize = 11.sp,
            maxLines = 1,
            modifier = Modifier.fillMaxWidth(),
        )
    }
}
