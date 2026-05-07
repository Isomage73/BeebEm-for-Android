package uk.org.beebem.android

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import coil.compose.AsyncImage
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

// Palette matches GamePickerScreen for visual consistency.
private val BG        = Color(0xFF080814)
private val HEADER_BG = Color(0xFF0E0E1E)
private val ITEM_BG   = Color(0xFF12121E)
private val ITEM_INST = Color(0xFF141A14)
private val DIVIDER   = Color(0xFF1E1E30)
private val ACCENT    = Color(0xFFE0A020)
private val TEXT_PRI  = Color(0xFFDDDDDD)
private val TEXT_SEC  = Color(0xFF7777AA)
private val TEXT_HEAD = Color(0xFF555580)
private val INPUT_BG  = Color(0xFF0A0A1A)

private sealed class SearchState {
    object Idle                                  : SearchState()
    object Searching                             : SearchState()
    data class Error(val msg: String)            : SearchState()
    data class Results(val items: List<BbcSearchResult>) : SearchState()
}

private sealed class DlState {
    object Idle                          : DlState()
    data class Downloading(val p: Float) : DlState()
    object Done                          : DlState()
    object Failed                        : DlState()
}

@Composable
fun BbcSearchScreen(
    onGameMounted: (File) -> Unit,
    onDismiss: () -> Unit,
) {
    val context  = LocalContext.current
    val scope    = rememberCoroutineScope()
    val keyboard = LocalSoftwareKeyboardController.current

    // Tell the Activity to pass hardware key events to Compose while this screen is up.
    val activity = context as? MainActivity
    DisposableEffect(Unit) {
        activity?.textInputActive = true
        onDispose { activity?.textInputActive = false }
    }

    var query          by remember { mutableStateOf("") }
    var searchState    by remember { mutableStateOf<SearchState>(SearchState.Idle) }
    val downloads      = remember { mutableStateMapOf<Int, DlState>() }
    var installedGames by remember { mutableStateOf<List<BbcSearchResult>>(emptyList()) }

    LaunchedEffect(Unit) {
        installedGames = withContext(Dispatchers.IO) { BbcMicroSearch.loadAllDownloaded(context) }
    }

    fun doSearch() {
        val q = query.trim()
        if (q.isEmpty()) return
        keyboard?.hide()
        searchState = SearchState.Searching
        scope.launch {
            searchState = try {
                val results = BbcMicroSearch.search(q)
                SearchState.Results(results)
            } catch (e: Exception) {
                SearchState.Error(e.message ?: "Network error")
            }
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(BG),
    ) {
        // ── Title bar ─────────────────────────────────────────────────────
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(HEADER_BG)
                .padding(horizontal = 12.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                "Search bbcmicro.co.uk",
                color      = ACCENT,
                fontSize   = 14.sp,
                fontWeight = FontWeight.Bold,
                modifier   = Modifier.weight(1f),
            )
            TextButton(onClick = onDismiss) {
                Text("Close", color = TEXT_SEC, fontSize = 11.sp)
            }
        }

        // ── Search bar ────────────────────────────────────────────────────
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(HEADER_BG)
                .padding(start = 12.dp, end = 12.dp, bottom = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            BasicTextField(
                value         = query,
                onValueChange = { query = it },
                singleLine    = true,
                textStyle     = TextStyle(color = TEXT_PRI, fontSize = 12.sp),
                cursorBrush   = SolidColor(ACCENT),
                keyboardOptions = KeyboardOptions(imeAction = ImeAction.Search),
                keyboardActions = KeyboardActions(onSearch = { doSearch() }),
                modifier = Modifier
                    .weight(1f)
                    .background(INPUT_BG, RoundedCornerShape(6.dp))
                    .border(0.5.dp, DIVIDER, RoundedCornerShape(6.dp))
                    .padding(horizontal = 10.dp, vertical = 6.dp),
                decorationBox = { inner ->
                    if (query.isEmpty()) {
                        Text("Game title…", color = TEXT_HEAD, fontSize = 12.sp)
                    }
                    inner()
                },
            )
            Spacer(Modifier.width(8.dp))
            TextButton(
                onClick  = ::doSearch,
                enabled  = query.trim().isNotEmpty(),
            ) {
                Text("Search", color = ACCENT, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            }
        }

        HorizontalDivider(color = DIVIDER)

        // ── Body ──────────────────────────────────────────────────────────
        when (val state = searchState) {
            SearchState.Idle -> {
                if (installedGames.isEmpty()) {
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Text("Enter a title to search the BBC Micro archive", color = TEXT_HEAD, fontSize = 11.sp)
                    }
                } else {
                    LazyColumn(modifier = Modifier.fillMaxSize()) {
                        item { SearchSectionHeader("Downloaded  (${installedGames.size})") }
                        items(installedGames, key = { it.id }) { result ->
                            SearchResultRow(
                                result  = result,
                                isLocal = true,
                                dlState = DlState.Idle,
                                onTap   = {
                                    val file = BbcMicroSearch.localFile(context, result) ?: return@SearchResultRow
                                    val ok   = BeebEmNative.nativeMountDisc(0, file.absolutePath, false)
                                    if (ok) { onGameMounted(file); onDismiss() }
                                },
                            )
                        }
                        item {
                            Text(
                                "Search above to find more games",
                                color    = TEXT_HEAD,
                                fontSize = 9.sp,
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(horizontal = 12.dp, vertical = 10.dp),
                            )
                        }
                    }
                }
            }

            SearchState.Searching -> {
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        CircularProgressIndicator(color = ACCENT, modifier = Modifier.size(32.dp))
                        Spacer(Modifier.height(12.dp))
                        Text("Searching…", color = TEXT_SEC, fontSize = 11.sp)
                    }
                }
            }

            is SearchState.Error -> {
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text("Search failed", color = Color(0xFFCC4444), fontSize = 12.sp)
                        Spacer(Modifier.height(6.dp))
                        Text(state.msg, color = TEXT_SEC, fontSize = 9.sp)
                        Spacer(Modifier.height(12.dp))
                        TextButton(onClick = ::doSearch) {
                            Text("Retry", color = ACCENT, fontSize = 11.sp)
                        }
                    }
                }
            }

            is SearchState.Results -> {
                if (state.items.isEmpty()) {
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Text("No results found", color = TEXT_SEC, fontSize = 11.sp)
                    }
                } else {
                    val downloaded = state.items.filter { BbcMicroSearch.isDownloaded(context, it) }
                    val online     = state.items.filter { !BbcMicroSearch.isDownloaded(context, it) }

                    LazyColumn(modifier = Modifier.fillMaxSize()) {
                        if (downloaded.isNotEmpty()) {
                            item { SearchSectionHeader("Downloaded  (${downloaded.size})") }
                            items(downloaded, key = { it.id }) { result ->
                                SearchResultRow(
                                    result      = result,
                                    isLocal     = true,
                                    dlState     = downloads[result.id] ?: DlState.Idle,
                                    onTap       = {
                                        val file = BbcMicroSearch.localFile(context, result) ?: return@SearchResultRow
                                        val ok   = BeebEmNative.nativeMountDisc(0, file.absolutePath, false)
                                        if (ok) { onGameMounted(file); onDismiss() }
                                    },
                                )
                            }
                        }
                        if (online.isNotEmpty()) {
                            item { SearchSectionHeader("Results  (${online.size})") }
                            items(online, key = { it.id }) { result ->
                                val dlState = downloads[result.id] ?: DlState.Idle
                                SearchResultRow(
                                    result  = result,
                                    isLocal = false,
                                    dlState = dlState,
                                    onTap   = {
                                        if (dlState is DlState.Downloading) return@SearchResultRow
                                        downloads[result.id] = DlState.Downloading(0f)
                                        scope.launch {
                                            val file = BbcMicroSearch.download(context, result) { p ->
                                                downloads[result.id] = DlState.Downloading(p)
                                            }
                                            if (file != null) {
                                                downloads[result.id] = DlState.Done
                                                installedGames = withContext(Dispatchers.IO) {
                                                    BbcMicroSearch.loadAllDownloaded(context)
                                                }
                                                val ok = BeebEmNative.nativeMountDisc(0, file.absolutePath, false)
                                                if (ok) { onGameMounted(file); onDismiss() }
                                            } else {
                                                downloads[result.id] = DlState.Failed
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
    }
}

@Composable
private fun SearchSectionHeader(label: String) {
    Text(
        label,
        color      = TEXT_HEAD,
        fontSize   = 9.sp,
        fontWeight = FontWeight.Bold,
        modifier   = Modifier
            .fillMaxWidth()
            .background(BG)
            .padding(horizontal = 12.dp, vertical = 6.dp),
    )
}

@Composable
private fun SearchResultRow(
    result:  BbcSearchResult,
    isLocal: Boolean,
    dlState: DlState,
    onTap:   () -> Unit,
) {
    val bg = if (isLocal) ITEM_INST else ITEM_BG

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(bg)
            .border(0.5.dp, DIVIDER)
            .pointerInput(result.id) { detectTapGestures { onTap() } }
            .padding(horizontal = 10.dp, vertical = 6.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            // Cover thumbnail
            val coverShape = RoundedCornerShape(4.dp)
            if (result.coverUrl != null) {
                AsyncImage(
                    model              = result.coverUrl,
                    contentDescription = result.title,
                    contentScale       = ContentScale.Crop,
                    modifier           = Modifier
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

            // Title + publisher + year
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    result.title,
                    color      = TEXT_PRI,
                    fontSize   = 12.sp,
                    fontWeight = FontWeight.Bold,
                    maxLines   = 1,
                    overflow   = TextOverflow.Ellipsis,
                )
                val sub = listOfNotNull(
                    result.publisher.takeIf { it.isNotEmpty() },
                    result.year.takeIf { it.isNotEmpty() },
                ).joinToString(" · ")
                if (sub.isNotEmpty()) {
                    Text(sub, color = TEXT_SEC, fontSize = 9.sp, maxLines = 1, overflow = TextOverflow.Ellipsis)
                }
            }

            Spacer(Modifier.width(8.dp))

            // Status badge
            when (dlState) {
                is DlState.Downloading -> {
                    CircularProgressIndicator(
                        progress    = { dlState.p },
                        color       = ACCENT,
                        modifier    = Modifier.size(20.dp),
                        strokeWidth = 2.dp,
                    )
                }
                DlState.Failed -> {
                    Text("!", color = Color(0xFFCC4444), fontSize = 14.sp, fontWeight = FontWeight.Bold)
                }
                else -> {
                    Text(
                        if (isLocal) "▶" else "↓",
                        color    = if (isLocal) Color(0xFF44AA44) else TEXT_SEC,
                        fontSize = 14.sp,
                    )
                }
            }
        }

        if (dlState is DlState.Downloading) {
            Spacer(Modifier.height(4.dp))
            LinearProgressIndicator(
                progress  = { dlState.p },
                modifier  = Modifier.fillMaxWidth().height(2.dp),
                color     = ACCENT,
                trackColor = Color(0xFF222234),
            )
        }
    }
}
