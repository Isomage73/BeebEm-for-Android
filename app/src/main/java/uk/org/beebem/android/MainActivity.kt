package uk.org.beebem.android

import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import uk.org.beebem.android.ui.theme.BeebDroidTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val ok = BeebEmNative.nativeInit(assets, filesDir.absolutePath)
        Log.i("BeebDroid", "nativeInit returned $ok")

        enableEdgeToEdge()
        setContent {
            BeebDroidTheme {
                EmulatorPlaceholder()
            }
        }
    }
}

@Composable
fun EmulatorPlaceholder() {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
    )
}
