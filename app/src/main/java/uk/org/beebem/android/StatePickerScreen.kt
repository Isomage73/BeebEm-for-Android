package uk.org.beebem.android

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.io.File

@Composable
fun StatePickerScreen(
    states: List<File>,
    onLoad: (File) -> Unit,
    onDelete: (File) -> Unit,
    onDismiss: () -> Unit,
) {
    // Scrim — tap outside the card to dismiss.
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xCC000000))
            .pointerInput(Unit) { detectTapGestures { onDismiss() } },
        contentAlignment = Alignment.Center,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth(0.85f)
                .background(Color(0xFF0E0E1E), RoundedCornerShape(12.dp))
                .border(1.dp, Color(0xFF1E1E30), RoundedCornerShape(12.dp))
                .padding(16.dp)
                .pointerInput(Unit) { detectTapGestures { /* consume touches inside card */ } },
        ) {
            Text(
                "Load Snapshot",
                color = Color.White,
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
            )
            Spacer(Modifier.height(12.dp))

            if (states.isEmpty()) {
                Text(
                    "No saved snapshots.",
                    color = Color(0xFF888899),
                    fontSize = 11.sp,
                    modifier = Modifier.padding(vertical = 8.dp),
                )
            } else {
                LazyColumn(
                    verticalArrangement = Arrangement.spacedBy(4.dp),
                    modifier = Modifier.heightIn(max = 300.dp),
                ) {
                    items(states, key = { it.absolutePath }) { file ->
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .background(Color(0xFF1A1A2E), RoundedCornerShape(6.dp))
                                .pointerInput(file) {
                                    detectTapGestures { onLoad(file) }
                                }
                                .padding(start = 10.dp, end = 0.dp, top = 4.dp, bottom = 4.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(
                                StateManager.displayName(file),
                                color = Color(0xFFCCCCDD),
                                fontSize = 11.sp,
                                modifier = Modifier.weight(1f),
                            )
                            TextButton(
                                onClick = { onDelete(file) },
                                contentPadding = PaddingValues(horizontal = 6.dp, vertical = 0.dp),
                            ) {
                                Text("DEL", color = Color(0xFF884444), fontSize = 9.sp, fontWeight = FontWeight.Bold)
                            }
                        }
                    }
                }
            }

            Spacer(Modifier.height(8.dp))
            HorizontalDivider(color = Color(0xFF1E1E30))
            TextButton(
                onClick = onDismiss,
                modifier = Modifier.align(Alignment.End),
            ) {
                Text("Cancel", color = Color(0xFF8888AA), fontSize = 11.sp)
            }
        }
    }
}
