package uk.org.beebem.android

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import coil.compose.AsyncImage
import kotlinx.coroutines.launch
import java.io.File

// Colours
private val BG        = Color(0xFF080814)
private val HEADER_BG = Color(0xFF0E0E1E)
private val ITEM_BG   = Color(0xFF12121E)
private val ITEM_INST = Color(0xFF141A14)   // slightly greenish tint for installed
private val DIVIDER   = Color(0xFF1E1E30)
private val ACCENT    = Color(0xFFE0A020)
private val TEXT_PRI  = Color(0xFFDDDDDD)
private val TEXT_SEC  = Color(0xFF7777AA)
private val TEXT_HEAD = Color(0xFF555580)

private sealed class DownloadState {
    object Idle        : DownloadState()
    data class Downloading(val progress: Float) : DownloadState()
    object Done        : DownloadState()
    object Failed      : DownloadState()
}

private sealed class CatalogueState {
    object Loading                          : CatalogueState()
    data class Error(val msg: String)       : CatalogueState()
    data class Loaded(val games: List<GameEntry>) : CatalogueState()
}

@Composable
fun GamePickerScreen(
    onGameMounted: (File) -> Unit,
    onDismiss: () -> Unit,
) {
    val context = LocalContext.current
    val scope   = rememberCoroutineScope()

    var catalogue by remember { mutableStateOf<CatalogueState>(CatalogueState.Loading) }
    val downloads = remember { mutableStateMapOf<String, DownloadState>() }

    LaunchedEffect(Unit) {
        catalogue = try {
            CatalogueState.Loaded(GameCatalogue.fetch())
        } catch (e: Exception) {
            CatalogueState.Error(e.message ?: "Network error")
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(BG),
    ) {
            // Title bar
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(HEADER_BG)
                    .padding(horizontal = 12.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    "BBC Games stairwaytohell.com",
                    color = ACCENT,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.weight(1f),
                )
                TextButton(onClick = onDismiss) {
                    Text("Close", color = TEXT_SEC, fontSize = 11.sp)
                }
            }

            when (val state = catalogue) {
                is CatalogueState.Loading -> {
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Column(horizontalAlignment = Alignment.CenterHorizontally) {
                            CircularProgressIndicator(color = ACCENT, modifier = Modifier.size(32.dp))
                            Spacer(Modifier.height(12.dp))
                            Text("Loading catalogue…", color = TEXT_SEC, fontSize = 11.sp)
                        }
                    }
                }
                is CatalogueState.Error -> {
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Column(horizontalAlignment = Alignment.CenterHorizontally) {
                            Text("Failed to load catalogue", color = Color(0xFFCC4444), fontSize = 12.sp)
                            Spacer(Modifier.height(6.dp))
                            Text(state.msg, color = TEXT_SEC, fontSize = 9.sp)
                            Spacer(Modifier.height(12.dp))
                            TextButton(onClick = {
                                catalogue = CatalogueState.Loading
                                scope.launch {
                                    catalogue = try {
                                        CatalogueState.Loaded(GameCatalogue.fetch())
                                    } catch (e: Exception) {
                                        CatalogueState.Error(e.message ?: "Network error")
                                    }
                                }
                            }) {
                                Text("Retry", color = ACCENT, fontSize = 11.sp)
                            }
                        }
                    }
                }
                is CatalogueState.Loaded -> {
                    val installed = state.games.filter { GameCatalogue.isInstalled(context, it) }
                    val online    = state.games.filter { !GameCatalogue.isInstalled(context, it) }

                    LazyColumn(modifier = Modifier.fillMaxSize()) {
                        if (installed.isNotEmpty()) {
                            item { SectionHeader("Installed  (${installed.size})") }
                            items(installed, key = { it.key }) { game ->
                                GameRow(
                                    game        = game,
                                    isInstalled = true,
                                    dlState     = downloads[game.key] ?: DownloadState.Idle,
                                    onTap       = {
                                        val file = GameCatalogue.localFile(context, game) ?: return@GameRow
                                        val ok   = BeebEmNative.nativeMountDisc(0, file.absolutePath, false)
                                        if (ok) { onGameMounted(file); onDismiss() }
                                    },
                                )
                            }
                        }

                        item { SectionHeader("Online  (${online.size})") }
                        items(online, key = { it.key }) { game ->
                            val dlState = downloads[game.key] ?: DownloadState.Idle
                            GameRow(
                                game        = game,
                                isInstalled = false,
                                dlState     = dlState,
                                onTap       = {
                                    if (dlState is DownloadState.Downloading) return@GameRow
                                    downloads[game.key] = DownloadState.Downloading(0f)
                                    scope.launch {
                                        val file = GameCatalogue.download(context, game) { p ->
                                            downloads[game.key] = DownloadState.Downloading(p)
                                        }
                                        if (file != null) {
                                            downloads[game.key] = DownloadState.Done
                                            val ok = BeebEmNative.nativeMountDisc(0, file.absolutePath, false)
                                            if (ok) { onGameMounted(file); onDismiss() }
                                        } else {
                                            downloads[game.key] = DownloadState.Failed
                                        }
                                    }
                                },
                            )
                        }
                    }
                }
            }
    }
}

@Composable
private fun SectionHeader(label: String) {
    Text(
        label,
        color     = TEXT_HEAD,
        fontSize  = 9.sp,
        fontWeight = FontWeight.Bold,
        modifier  = Modifier
            .fillMaxWidth()
            .background(BG)
            .padding(horizontal = 12.dp, vertical = 6.dp),
    )
}

@Composable
private fun GameRow(
    game:        GameEntry,
    isInstalled: Boolean,
    dlState:     DownloadState,
    onTap:       () -> Unit,
) {
    val bg = if (isInstalled) ITEM_INST else ITEM_BG

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(bg)
            .border(0.5.dp, DIVIDER)
            .pointerInput(game.key) { detectTapGestures { onTap() } }
            .padding(horizontal = 10.dp, vertical = 6.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            // Cover thumbnail
            val coverShape = RoundedCornerShape(4.dp)
            if (game.coverUrl != null) {
                AsyncImage(
                    model            = game.coverUrl,
                    contentDescription = game.title,
                    contentScale     = ContentScale.Crop,
                    modifier         = Modifier
                        .size(52.dp)
                        .clip(coverShape)
                        .background(Color(0xFF1A1A30), coverShape),
                )
            } else {
                Box(
                    Modifier
                        .size(52.dp)
                        .background(Color(0xFF1A1A30), coverShape),
                    contentAlignment = Alignment.Center,
                ) {
                    Text("BBC", color = Color(0xFF3A3A5A), fontSize = 9.sp, fontWeight = FontWeight.Bold)
                }
            }

            Spacer(Modifier.width(10.dp))

            // Title + publisher
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    game.title,
                    color      = TEXT_PRI,
                    fontSize   = 12.sp,
                    fontWeight = FontWeight.Bold,
                    maxLines   = 1,
                    overflow   = TextOverflow.Ellipsis,
                )
                if (game.publisher.isNotEmpty()) {
                    Text(
                        game.publisher,
                        color    = TEXT_SEC,
                        fontSize = 9.sp,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }

            Spacer(Modifier.width(8.dp))

            // Status indicator
            when (dlState) {
                is DownloadState.Downloading -> {
                    CircularProgressIndicator(
                        progress   = { dlState.progress },
                        color      = ACCENT,
                        modifier   = Modifier.size(20.dp),
                        strokeWidth = 2.dp,
                    )
                }
                DownloadState.Failed -> {
                    Text("!", color = Color(0xFFCC4444), fontSize = 14.sp, fontWeight = FontWeight.Bold)
                }
                else -> {
                    Text(
                        if (isInstalled) "▶" else "↓",
                        color    = if (isInstalled) Color(0xFF44AA44) else TEXT_SEC,
                        fontSize = 14.sp,
                    )
                }
            }
        }

        // Download progress bar
        if (dlState is DownloadState.Downloading) {
            Spacer(Modifier.height(4.dp))
            LinearProgressIndicator(
                progress = { dlState.progress },
                modifier = Modifier.fillMaxWidth().height(2.dp),
                color    = ACCENT,
                trackColor = Color(0xFF222234),
            )
        }
    }
}
