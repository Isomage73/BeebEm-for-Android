package uk.org.beebem.android

import android.content.Context
import org.json.JSONObject
import java.io.File

data class JoypadKey(val row: Int, val col: Int, val label: String, val shifted: Boolean = false)

data class JoypadMapping(
    val up:    JoypadKey,
    val down:  JoypadKey,
    val left:  JoypadKey,
    val right: JoypadKey,
    val fire1: JoypadKey,
    val fire2: JoypadKey,
    val fire3: JoypadKey,
)

val DefaultJoypadMapping = JoypadMapping(
    up    = JoypadKey(3, 9, "↑"),
    down  = JoypadKey(2, 9, "↓"),
    left  = JoypadKey(1, 9, "←"),
    right = JoypadKey(7, 9, "→"),
    fire1 = JoypadKey(6, 2, "SPC"),
    fire2 = JoypadKey(0, 0, "SHF"),
    fire3 = JoypadKey(0, 1, "CTL"),
)

// Key rows for the mapping picker dialog — mirrors the BBC keyboard layout exactly.
val PICKER_KEY_ROWS: List<List<JoypadKey>> = listOf(
    // Function keys (BREAK is excluded — it uses nativeBreakKey, not nativeKeyDown)
    listOf(
        JoypadKey(2, 0, "f0"), JoypadKey(7, 1, "f1"), JoypadKey(7, 2, "f2"), JoypadKey(7, 3, "f3"),
        JoypadKey(1, 4, "f4"), JoypadKey(7, 4, "f5"), JoypadKey(7, 5, "f6"), JoypadKey(1, 6, "f7"),
        JoypadKey(7, 6, "f8"), JoypadKey(7, 7, "f9"),
    ),
    // Number row: ESC 1 2 3 4 5 6 7 8 9 0 - ^ _ DEL
    listOf(
        JoypadKey(7, 0, "ESC"),
        JoypadKey(3, 0, "1"),  JoypadKey(3, 1, "2"),  JoypadKey(1, 1, "3"),
        JoypadKey(1, 2, "4"),  JoypadKey(1, 3, "5"),  JoypadKey(3, 4, "6"),
        JoypadKey(2, 4, "7"),  JoypadKey(1, 5, "8"),  JoypadKey(2, 6, "9"),
        JoypadKey(2, 7, "0"),  JoypadKey(1, 7, "-"),
        JoypadKey(1, 8, "^"),  JoypadKey(2, 8, "_"),
        JoypadKey(5, 9, "DEL"),
    ),
    // QWERTY row: TAB Q W E R T Y U I O P @ [ ] \ RET
    listOf(
        JoypadKey(6, 0, "TAB"),
        JoypadKey(1, 0, "Q"), JoypadKey(2, 1, "W"), JoypadKey(2, 2, "E"),
        JoypadKey(3, 3, "R"), JoypadKey(2, 3, "T"), JoypadKey(4, 4, "Y"), JoypadKey(3, 5, "U"),
        JoypadKey(2, 5, "I"), JoypadKey(3, 6, "O"), JoypadKey(3, 7, "P"),
        JoypadKey(4, 7, "@"),
        JoypadKey(3, 8, "["), JoypadKey(5, 8, "]"), JoypadKey(7, 8, "\\"),
        JoypadKey(4, 9, "RET"),
    ),
    // ASDF row: CAPS A S D F G H J K L ; : COPY
    listOf(
        JoypadKey(4, 0, "CAPS"),
        JoypadKey(4, 1, "A"), JoypadKey(5, 1, "S"), JoypadKey(3, 2, "D"),
        JoypadKey(4, 3, "F"), JoypadKey(5, 3, "G"), JoypadKey(5, 4, "H"),
        JoypadKey(4, 5, "J"), JoypadKey(4, 6, "K"), JoypadKey(5, 6, "L"),
        JoypadKey(5, 7, ";"), JoypadKey(4, 8, ":"),
        JoypadKey(6, 9, "COPY"),
    ),
    // ZXCV row: SHF Z X C V B N M , . / SHF
    listOf(
        JoypadKey(0, 0, "SHF"),
        JoypadKey(6, 1, "Z"), JoypadKey(4, 2, "X"), JoypadKey(5, 2, "C"),
        JoypadKey(6, 3, "V"), JoypadKey(6, 4, "B"), JoypadKey(5, 5, "N"),
        JoypadKey(6, 5, "M"), JoypadKey(6, 6, ","), JoypadKey(6, 7, "."),
        JoypadKey(6, 8, "/"),
    ),
    // Bottom row: CTL SPC ↑ ↓ ← →
    listOf(
        JoypadKey(0, 1, "CTL"), JoypadKey(6, 2, "SPC"),
        JoypadKey(3, 9, "↑"), JoypadKey(2, 9, "↓"), JoypadKey(1, 9, "←"), JoypadKey(7, 9, "→"),
    ),
    // Shifted symbols — sent with SHIFT held
    listOf(
        JoypadKey(3, 0, "!",  shifted = true), JoypadKey(3, 1, "\"", shifted = true),
        JoypadKey(1, 1, "#",  shifted = true), JoypadKey(1, 2, "$",  shifted = true),
        JoypadKey(1, 3, "%",  shifted = true), JoypadKey(3, 4, "&",  shifted = true),
        JoypadKey(2, 4, "'",  shifted = true), JoypadKey(1, 5, "(",  shifted = true),
        JoypadKey(2, 6, ")",  shifted = true), JoypadKey(1, 7, "=",  shifted = true),
        JoypadKey(1, 8, "÷",  shifted = true), JoypadKey(2, 8, "£",  shifted = true),
        JoypadKey(6, 8, "?",  shifted = true), JoypadKey(5, 7, "+",  shifted = true),
        JoypadKey(4, 8, "*",  shifted = true), JoypadKey(6, 6, "<",  shifted = true),
        JoypadKey(6, 7, ">",  shifted = true), JoypadKey(3, 8, "{",  shifted = true),
        JoypadKey(5, 8, "}",  shifted = true), JoypadKey(7, 8, "|",  shifted = true),
    ),
)

object JoypadConfigManager {

    private const val FILENAME = "joypad_config.json"

    fun load(context: Context): Pair<JoypadMapping, MutableMap<String, JoypadMapping>> {
        val file = File(context.filesDir, FILENAME)
        if (!file.exists()) return Pair(DefaultJoypadMapping, mutableMapOf())
        return try {
            val root = JSONObject(file.readText())
            val default = jsonToMapping(root.getJSONObject("default"))
            val discMappings = mutableMapOf<String, JoypadMapping>()
            val discs = root.optJSONObject("discMappings") ?: JSONObject()
            discs.keys().forEach { key -> discMappings[key] = jsonToMapping(discs.getJSONObject(key)) }
            Pair(default, discMappings)
        } catch (_: Exception) {
            Pair(DefaultJoypadMapping, mutableMapOf())
        }
    }

    fun save(
        context: Context,
        default: JoypadMapping,
        discMappings: Map<String, JoypadMapping>,
    ) {
        val root = JSONObject()
        root.put("default", mappingToJson(default))
        val discs = JSONObject()
        discMappings.forEach { (name, mapping) -> discs.put(name, mappingToJson(mapping)) }
        root.put("discMappings", discs)
        File(context.filesDir, FILENAME).writeText(root.toString(2))
    }

    private fun mappingToJson(m: JoypadMapping) = JSONObject().apply {
        put("up",    keyToJson(m.up))
        put("down",  keyToJson(m.down))
        put("left",  keyToJson(m.left))
        put("right", keyToJson(m.right))
        put("fire1", keyToJson(m.fire1))
        put("fire2", keyToJson(m.fire2))
        put("fire3", keyToJson(m.fire3))
    }

    private fun keyToJson(k: JoypadKey) = JSONObject().apply {
        put("row", k.row); put("col", k.col); put("label", k.label)
        if (k.shifted) put("shifted", true)
    }

    private fun jsonToMapping(obj: JSONObject) = JoypadMapping(
        up    = jsonToKey(obj.getJSONObject("up")),
        down  = jsonToKey(obj.getJSONObject("down")),
        left  = jsonToKey(obj.getJSONObject("left")),
        right = jsonToKey(obj.getJSONObject("right")),
        fire1 = jsonToKey(obj.getJSONObject("fire1")),
        fire2 = jsonToKey(obj.getJSONObject("fire2")),
        fire3 = jsonToKey(obj.getJSONObject("fire3")),
    )

    private fun jsonToKey(obj: JSONObject) = JoypadKey(
        row     = obj.getInt("row"),
        col     = obj.getInt("col"),
        label   = obj.getString("label"),
        shifted = obj.optBoolean("shifted", false),
    )
}
