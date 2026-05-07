package uk.org.beebem.android

import android.view.KeyEvent

data class BbcMatrixKey(val row: Int, val col: Int)

// Sentinel for keys that trigger BREAK rather than a matrix key.
val BBC_BREAK = BbcMatrixKey(-99, -99)

val HARDWARE_KEY_MAP: Map<Int, BbcMatrixKey> = mapOf(
    // Letters
    KeyEvent.KEYCODE_A to BbcMatrixKey(4, 1),
    KeyEvent.KEYCODE_B to BbcMatrixKey(6, 4),
    KeyEvent.KEYCODE_C to BbcMatrixKey(5, 2),
    KeyEvent.KEYCODE_D to BbcMatrixKey(3, 2),
    KeyEvent.KEYCODE_E to BbcMatrixKey(2, 2),
    KeyEvent.KEYCODE_F to BbcMatrixKey(4, 3),
    KeyEvent.KEYCODE_G to BbcMatrixKey(5, 3),
    KeyEvent.KEYCODE_H to BbcMatrixKey(5, 4),
    KeyEvent.KEYCODE_I to BbcMatrixKey(2, 5),
    KeyEvent.KEYCODE_J to BbcMatrixKey(4, 5),
    KeyEvent.KEYCODE_K to BbcMatrixKey(4, 6),
    KeyEvent.KEYCODE_L to BbcMatrixKey(5, 6),
    KeyEvent.KEYCODE_M to BbcMatrixKey(6, 5),
    KeyEvent.KEYCODE_N to BbcMatrixKey(5, 5),
    KeyEvent.KEYCODE_O to BbcMatrixKey(3, 6),
    KeyEvent.KEYCODE_P to BbcMatrixKey(3, 7),
    KeyEvent.KEYCODE_Q to BbcMatrixKey(1, 0),
    KeyEvent.KEYCODE_R to BbcMatrixKey(3, 3),
    KeyEvent.KEYCODE_S to BbcMatrixKey(5, 1),
    KeyEvent.KEYCODE_T to BbcMatrixKey(2, 3),
    KeyEvent.KEYCODE_U to BbcMatrixKey(3, 5),
    KeyEvent.KEYCODE_V to BbcMatrixKey(6, 3),
    KeyEvent.KEYCODE_W to BbcMatrixKey(2, 1),
    KeyEvent.KEYCODE_X to BbcMatrixKey(4, 2),
    KeyEvent.KEYCODE_Y to BbcMatrixKey(4, 4),
    KeyEvent.KEYCODE_Z to BbcMatrixKey(6, 1),

    // Digits (top row)
    KeyEvent.KEYCODE_0 to BbcMatrixKey(2, 7),
    KeyEvent.KEYCODE_1 to BbcMatrixKey(3, 0),
    KeyEvent.KEYCODE_2 to BbcMatrixKey(3, 1),
    KeyEvent.KEYCODE_3 to BbcMatrixKey(1, 1),
    KeyEvent.KEYCODE_4 to BbcMatrixKey(1, 2),
    KeyEvent.KEYCODE_5 to BbcMatrixKey(1, 3),
    KeyEvent.KEYCODE_6 to BbcMatrixKey(3, 4),
    KeyEvent.KEYCODE_7 to BbcMatrixKey(2, 4),
    KeyEvent.KEYCODE_8 to BbcMatrixKey(1, 5),
    KeyEvent.KEYCODE_9 to BbcMatrixKey(2, 6),

    // Function keys — PC F1-F10 → BBC f0-f9, F11 = BREAK, F12 = SHIFT+BREAK (handled in code)
    KeyEvent.KEYCODE_F1  to BbcMatrixKey(2, 0),  // BBC f0
    KeyEvent.KEYCODE_F2  to BbcMatrixKey(7, 1),  // BBC f1
    KeyEvent.KEYCODE_F3  to BbcMatrixKey(7, 2),  // BBC f2
    KeyEvent.KEYCODE_F4  to BbcMatrixKey(7, 3),  // BBC f3
    KeyEvent.KEYCODE_F5  to BbcMatrixKey(1, 4),  // BBC f4
    KeyEvent.KEYCODE_F6  to BbcMatrixKey(7, 4),  // BBC f5
    KeyEvent.KEYCODE_F7  to BbcMatrixKey(7, 5),  // BBC f6
    KeyEvent.KEYCODE_F8  to BbcMatrixKey(1, 6),  // BBC f7
    KeyEvent.KEYCODE_F9  to BbcMatrixKey(7, 6),  // BBC f8
    KeyEvent.KEYCODE_F10 to BbcMatrixKey(7, 7),  // BBC f9
    KeyEvent.KEYCODE_F11 to BBC_BREAK,            // BREAK
    KeyEvent.KEYCODE_F12 to BBC_BREAK,            // SHIFT+BREAK (caller checks meta state)

    // Modifiers
    KeyEvent.KEYCODE_SHIFT_LEFT   to BbcMatrixKey(0, 0),
    KeyEvent.KEYCODE_SHIFT_RIGHT  to BbcMatrixKey(0, 0),
    KeyEvent.KEYCODE_CTRL_LEFT    to BbcMatrixKey(0, 1),
    KeyEvent.KEYCODE_CTRL_RIGHT   to BbcMatrixKey(0, 1),
    KeyEvent.KEYCODE_CAPS_LOCK    to BbcMatrixKey(4, 0),

    // Navigation / editing
    KeyEvent.KEYCODE_ESCAPE       to BbcMatrixKey(7, 0),
    KeyEvent.KEYCODE_TAB          to BbcMatrixKey(6, 0),
    KeyEvent.KEYCODE_DEL          to BbcMatrixKey(5, 9),   // Backspace → BBC DELETE
    KeyEvent.KEYCODE_FORWARD_DEL  to BbcMatrixKey(5, 9),   // Delete key → BBC DELETE
    KeyEvent.KEYCODE_ENTER        to BbcMatrixKey(4, 9),
    KeyEvent.KEYCODE_NUMPAD_ENTER to BbcMatrixKey(4, 9),
    KeyEvent.KEYCODE_SPACE        to BbcMatrixKey(6, 2),
    KeyEvent.KEYCODE_INSERT       to BbcMatrixKey(6, 9),   // Insert → BBC COPY

    // Cursor keys
    KeyEvent.KEYCODE_DPAD_UP    to BbcMatrixKey(3, 9),
    KeyEvent.KEYCODE_DPAD_DOWN  to BbcMatrixKey(2, 9),
    KeyEvent.KEYCODE_DPAD_LEFT  to BbcMatrixKey(1, 9),
    KeyEvent.KEYCODE_DPAD_RIGHT to BbcMatrixKey(7, 9),

    // Symbols — unshifted position
    KeyEvent.KEYCODE_MINUS        to BbcMatrixKey(1, 7),   // -
    KeyEvent.KEYCODE_EQUALS       to BbcMatrixKey(1, 8),   // ^ (BBC caret key)
    KeyEvent.KEYCODE_BACKSLASH    to BbcMatrixKey(2, 8),   // _ (BBC underscore / £ key)
    KeyEvent.KEYCODE_AT           to BbcMatrixKey(4, 7),   // @
    KeyEvent.KEYCODE_LEFT_BRACKET to BbcMatrixKey(3, 8),   // [
    KeyEvent.KEYCODE_RIGHT_BRACKET to BbcMatrixKey(5, 8),  // ]
    KeyEvent.KEYCODE_SEMICOLON    to BbcMatrixKey(5, 7),   // ;
    KeyEvent.KEYCODE_APOSTROPHE   to BbcMatrixKey(4, 8),   // : (BBC colon key)
    KeyEvent.KEYCODE_COMMA        to BbcMatrixKey(6, 6),   // ,
    KeyEvent.KEYCODE_PERIOD       to BbcMatrixKey(6, 7),   // .
    KeyEvent.KEYCODE_SLASH        to BbcMatrixKey(6, 8),   // /
    KeyEvent.KEYCODE_GRAVE        to BbcMatrixKey(7, 8),   // \ (BBC backslash key)
)
