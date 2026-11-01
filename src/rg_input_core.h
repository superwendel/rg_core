// rg_input_core - backend-neutral input state and ordered events
//
// Part of the Reverse Gravity (rg_) core libraries.
// Single-header C99 input model with no platform or SDL dependency: key and
// mouse state, an optional ordered event queue, and the feeder functions a
// windowing backend calls to fill them. rg_input.h layers the SDL3 adapter on
// top (sampling, event translation) and asserts that every RgKey and RgMod
// value equals its SDL3 counterpart, so the two can be mixed freely.
//
// USAGE (backend side, e.g. a Win32 message loop):
//   #include "rg_input_core.h"
//   RgInputState input; rg_input_init(&input);
//   // per frame:
//   rg_input_begin_frame(&input);
//   // for each message:
//   rg_input_set_key(&input, RG_KEY_A, true);
//   rg_input_set_mouse_position(&input, x, y);
//   rg_input_add_wheel(&input, dx, dy);
//   rg_input_push_text(&input, "é");           // appends; several per frame are kept
//   // ordered events: rg_input_event_queue_push / rg_input_event_queue_push_text
//
// OPTIONS:
//   #define RG_INPUT_TEXT_BUFFER_SIZE  - Text input buffer size (default: 32)
//   #define RG_INPUT_ASSERT(x)         - Custom assert macro (default: assert)
//   #define RG_INPUT_NO_SIMD           - Copy key state without SSE2/NEON
//
// NOTES:
//   - All functions have internal linkage and work in unity builds.
//   - RgKey values are USB HID usage ids, the same numbering SDL scancodes use.
//   - rg_input_begin_frame clears per-frame accumulators (text, wheel, mouse
//     deltas) and snapshots previous state; call it before feeding a frame.
//
// Author: Steven Wendel (superwendel)

#ifndef RG_INPUT_CORE_H
#define RG_INPUT_CORE_H

#include "rg_defs.h"

#include <stdbool.h>
#include <string.h>

// =============================================================================
// CONFIGURATION
// =============================================================================

#ifndef RG_INPUT_TEXT_BUFFER_SIZE
#define RG_INPUT_TEXT_BUFFER_SIZE 32u
#endif

#ifndef RG_INPUT_ASSERT
#include <assert.h>
#define RG_INPUT_ASSERT(x) assert(x)
#endif

#ifndef RG_INPUT_UNUSED
#define RG_INPUT_UNUSED(x) (void)(x)
#endif

// The key-state arrays are only byte aligned, and compilers lower their
// 512-byte memcpy to a misaligned rep movs that costs several times an
// explicit 16-byte loop. SSE2 is baseline on x86-64 and NEON on AArch64.
#if !defined(RG_INPUT_NO_SIMD) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define RG_INPUT_HAS_SSE2 1
#define RG_INPUT_HAS_NEON 0
#include <emmintrin.h>
#elif !defined(RG_INPUT_NO_SIMD) && defined(__ARM_NEON) && (defined(__GNUC__) || defined(__clang__))
#define RG_INPUT_HAS_SSE2 0
#define RG_INPUT_HAS_NEON 1
#include <arm_neon.h>
#else
#define RG_INPUT_HAS_SSE2 0
#define RG_INPUT_HAS_NEON 0
#endif

// Mouse button indices (for rg_input_is_mouse_button_*)
#define RG_MOUSE_BUTTON_LEFT 0
#define RG_MOUSE_BUTTON_MIDDLE 1
#define RG_MOUSE_BUTTON_RIGHT 2
#define RG_MOUSE_BUTTON_X1 3
#define RG_MOUSE_BUTTON_X2 4
#define RG_MOUSE_BUTTON_COUNT 5

// =============================================================================
// KEYS AND MODIFIERS
// =============================================================================

// X(name, value, label): every key, its USB HID usage id (== SDL3 scancode)
// and the label rg_key_name returns. RG_KEY_COUNT is the array size.
#define RG_KEY_LIST(X) \
	X(UNKNOWN, 0, "") \
	X(A, 4, "A") \
	X(B, 5, "B") \
	X(C, 6, "C") \
	X(D, 7, "D") \
	X(E, 8, "E") \
	X(F, 9, "F") \
	X(G, 10, "G") \
	X(H, 11, "H") \
	X(I, 12, "I") \
	X(J, 13, "J") \
	X(K, 14, "K") \
	X(L, 15, "L") \
	X(M, 16, "M") \
	X(N, 17, "N") \
	X(O, 18, "O") \
	X(P, 19, "P") \
	X(Q, 20, "Q") \
	X(R, 21, "R") \
	X(S, 22, "S") \
	X(T, 23, "T") \
	X(U, 24, "U") \
	X(V, 25, "V") \
	X(W, 26, "W") \
	X(X, 27, "X") \
	X(Y, 28, "Y") \
	X(Z, 29, "Z") \
	X(1, 30, "1") \
	X(2, 31, "2") \
	X(3, 32, "3") \
	X(4, 33, "4") \
	X(5, 34, "5") \
	X(6, 35, "6") \
	X(7, 36, "7") \
	X(8, 37, "8") \
	X(9, 38, "9") \
	X(0, 39, "0") \
	X(RETURN, 40, "Return") \
	X(ESCAPE, 41, "Escape") \
	X(BACKSPACE, 42, "Backspace") \
	X(TAB, 43, "Tab") \
	X(SPACE, 44, "Space") \
	X(MINUS, 45, "-") \
	X(EQUALS, 46, "=") \
	X(LEFTBRACKET, 47, "[") \
	X(RIGHTBRACKET, 48, "]") \
	X(BACKSLASH, 49, "\\") \
	X(NONUSHASH, 50, "#") \
	X(SEMICOLON, 51, ";") \
	X(APOSTROPHE, 52, "'") \
	X(GRAVE, 53, "`") \
	X(COMMA, 54, ",") \
	X(PERIOD, 55, ".") \
	X(SLASH, 56, "/") \
	X(CAPSLOCK, 57, "CapsLock") \
	X(F1, 58, "F1") \
	X(F2, 59, "F2") \
	X(F3, 60, "F3") \
	X(F4, 61, "F4") \
	X(F5, 62, "F5") \
	X(F6, 63, "F6") \
	X(F7, 64, "F7") \
	X(F8, 65, "F8") \
	X(F9, 66, "F9") \
	X(F10, 67, "F10") \
	X(F11, 68, "F11") \
	X(F12, 69, "F12") \
	X(PRINTSCREEN, 70, "PrintScreen") \
	X(SCROLLLOCK, 71, "ScrollLock") \
	X(PAUSE, 72, "Pause") \
	X(INSERT, 73, "Insert") \
	X(HOME, 74, "Home") \
	X(PAGEUP, 75, "PageUp") \
	X(DELETE, 76, "Delete") \
	X(END, 77, "End") \
	X(PAGEDOWN, 78, "PageDown") \
	X(RIGHT, 79, "Right") \
	X(LEFT, 80, "Left") \
	X(DOWN, 81, "Down") \
	X(UP, 82, "Up") \
	X(NUMLOCKCLEAR, 83, "Numlock") \
	X(KP_DIVIDE, 84, "Keypad /") \
	X(KP_MULTIPLY, 85, "Keypad *") \
	X(KP_MINUS, 86, "Keypad -") \
	X(KP_PLUS, 87, "Keypad +") \
	X(KP_ENTER, 88, "Keypad Enter") \
	X(KP_1, 89, "Keypad 1") \
	X(KP_2, 90, "Keypad 2") \
	X(KP_3, 91, "Keypad 3") \
	X(KP_4, 92, "Keypad 4") \
	X(KP_5, 93, "Keypad 5") \
	X(KP_6, 94, "Keypad 6") \
	X(KP_7, 95, "Keypad 7") \
	X(KP_8, 96, "Keypad 8") \
	X(KP_9, 97, "Keypad 9") \
	X(KP_0, 98, "Keypad 0") \
	X(KP_PERIOD, 99, "Keypad .") \
	X(NONUSBACKSLASH, 100, "NonUSBackslash") \
	X(APPLICATION, 101, "Application") \
	X(POWER, 102, "Power") \
	X(KP_EQUALS, 103, "Keypad =") \
	X(F13, 104, "F13") \
	X(F14, 105, "F14") \
	X(F15, 106, "F15") \
	X(F16, 107, "F16") \
	X(F17, 108, "F17") \
	X(F18, 109, "F18") \
	X(F19, 110, "F19") \
	X(F20, 111, "F20") \
	X(F21, 112, "F21") \
	X(F22, 113, "F22") \
	X(F23, 114, "F23") \
	X(F24, 115, "F24") \
	X(EXECUTE, 116, "Execute") \
	X(HELP, 117, "Help") \
	X(MENU, 118, "Menu") \
	X(SELECT, 119, "Select") \
	X(STOP, 120, "Stop") \
	X(AGAIN, 121, "Again") \
	X(UNDO, 122, "Undo") \
	X(CUT, 123, "Cut") \
	X(COPY, 124, "Copy") \
	X(PASTE, 125, "Paste") \
	X(FIND, 126, "Find") \
	X(MUTE, 127, "Mute") \
	X(VOLUMEUP, 128, "VolumeUp") \
	X(VOLUMEDOWN, 129, "VolumeDown") \
	X(KP_COMMA, 133, "Keypad ,") \
	X(KP_EQUALSAS400, 134, "Keypad = (AS400)") \
	X(INTERNATIONAL1, 135, "International1") \
	X(INTERNATIONAL2, 136, "International2") \
	X(INTERNATIONAL3, 137, "International3") \
	X(INTERNATIONAL4, 138, "International4") \
	X(INTERNATIONAL5, 139, "International5") \
	X(INTERNATIONAL6, 140, "International6") \
	X(INTERNATIONAL7, 141, "International7") \
	X(INTERNATIONAL8, 142, "International8") \
	X(INTERNATIONAL9, 143, "International9") \
	X(LANG1, 144, "Lang1") \
	X(LANG2, 145, "Lang2") \
	X(LANG3, 146, "Lang3") \
	X(LANG4, 147, "Lang4") \
	X(LANG5, 148, "Lang5") \
	X(LANG6, 149, "Lang6") \
	X(LANG7, 150, "Lang7") \
	X(LANG8, 151, "Lang8") \
	X(LANG9, 152, "Lang9") \
	X(ALTERASE, 153, "AltErase") \
	X(SYSREQ, 154, "SysReq") \
	X(CANCEL, 155, "Cancel") \
	X(CLEAR, 156, "Clear") \
	X(PRIOR, 157, "Prior") \
	X(RETURN2, 158, "Return") \
	X(SEPARATOR, 159, "Separator") \
	X(OUT, 160, "Out") \
	X(OPER, 161, "Oper") \
	X(CLEARAGAIN, 162, "Clear / Again") \
	X(CRSEL, 163, "CrSel") \
	X(EXSEL, 164, "ExSel") \
	X(KP_00, 176, "Keypad 00") \
	X(KP_000, 177, "Keypad 000") \
	X(THOUSANDSSEPARATOR, 178, "ThousandsSeparator") \
	X(DECIMALSEPARATOR, 179, "DecimalSeparator") \
	X(CURRENCYUNIT, 180, "CurrencyUnit") \
	X(CURRENCYSUBUNIT, 181, "CurrencySubUnit") \
	X(KP_LEFTPAREN, 182, "Keypad (") \
	X(KP_RIGHTPAREN, 183, "Keypad )") \
	X(KP_LEFTBRACE, 184, "Keypad {") \
	X(KP_RIGHTBRACE, 185, "Keypad }") \
	X(KP_TAB, 186, "Keypad Tab") \
	X(KP_BACKSPACE, 187, "Keypad Backspace") \
	X(KP_A, 188, "Keypad A") \
	X(KP_B, 189, "Keypad B") \
	X(KP_C, 190, "Keypad C") \
	X(KP_D, 191, "Keypad D") \
	X(KP_E, 192, "Keypad E") \
	X(KP_F, 193, "Keypad F") \
	X(KP_XOR, 194, "Keypad XOR") \
	X(KP_POWER, 195, "Keypad ^") \
	X(KP_PERCENT, 196, "Keypad %") \
	X(KP_LESS, 197, "Keypad <") \
	X(KP_GREATER, 198, "Keypad >") \
	X(KP_AMPERSAND, 199, "Keypad &") \
	X(KP_DBLAMPERSAND, 200, "Keypad &&") \
	X(KP_VERTICALBAR, 201, "Keypad |") \
	X(KP_DBLVERTICALBAR, 202, "Keypad ||") \
	X(KP_COLON, 203, "Keypad :") \
	X(KP_HASH, 204, "Keypad #") \
	X(KP_SPACE, 205, "Keypad Space") \
	X(KP_AT, 206, "Keypad @") \
	X(KP_EXCLAM, 207, "Keypad !") \
	X(KP_MEMSTORE, 208, "Keypad MemStore") \
	X(KP_MEMRECALL, 209, "Keypad MemRecall") \
	X(KP_MEMCLEAR, 210, "Keypad MemClear") \
	X(KP_MEMADD, 211, "Keypad MemAdd") \
	X(KP_MEMSUBTRACT, 212, "Keypad MemSubtract") \
	X(KP_MEMMULTIPLY, 213, "Keypad MemMultiply") \
	X(KP_MEMDIVIDE, 214, "Keypad MemDivide") \
	X(KP_PLUSMINUS, 215, "Keypad +/-") \
	X(KP_CLEAR, 216, "Keypad Clear") \
	X(KP_CLEARENTRY, 217, "Keypad ClearEntry") \
	X(KP_BINARY, 218, "Keypad Binary") \
	X(KP_OCTAL, 219, "Keypad Octal") \
	X(KP_DECIMAL, 220, "Keypad Decimal") \
	X(KP_HEXADECIMAL, 221, "Keypad Hexadecimal") \
	X(LCTRL, 224, "Left Ctrl") \
	X(LSHIFT, 225, "Left Shift") \
	X(LALT, 226, "Left Alt") \
	X(LGUI, 227, "Left GUI") \
	X(RCTRL, 228, "Right Ctrl") \
	X(RSHIFT, 229, "Right Shift") \
	X(RALT, 230, "Right Alt") \
	X(RGUI, 231, "Right GUI") \
	X(MODE, 257, "ModeSwitch") \
	X(SLEEP, 258, "Sleep") \
	X(WAKE, 259, "Wake") \
	X(CHANNEL_INCREMENT, 260, "ChannelIncrement") \
	X(CHANNEL_DECREMENT, 261, "ChannelDecrement") \
	X(MEDIA_PLAY, 262, "MediaPlay") \
	X(MEDIA_PAUSE, 263, "MediaPause") \
	X(MEDIA_RECORD, 264, "MediaRecord") \
	X(MEDIA_FAST_FORWARD, 265, "MediaFastForward") \
	X(MEDIA_REWIND, 266, "MediaRewind") \
	X(MEDIA_NEXT_TRACK, 267, "MediaTrackNext") \
	X(MEDIA_PREVIOUS_TRACK, 268, "MediaTrackPrevious") \
	X(MEDIA_STOP, 269, "MediaStop") \
	X(MEDIA_EJECT, 270, "Eject") \
	X(MEDIA_PLAY_PAUSE, 271, "MediaPlayPause") \
	X(MEDIA_SELECT, 272, "MediaSelect") \
	X(AC_NEW, 273, "AC New") \
	X(AC_OPEN, 274, "AC Open") \
	X(AC_CLOSE, 275, "AC Close") \
	X(AC_EXIT, 276, "AC Exit") \
	X(AC_SAVE, 277, "AC Save") \
	X(AC_PRINT, 278, "AC Print") \
	X(AC_PROPERTIES, 279, "AC Properties") \
	X(AC_SEARCH, 280, "AC Search") \
	X(AC_HOME, 281, "AC Home") \
	X(AC_BACK, 282, "AC Back") \
	X(AC_FORWARD, 283, "AC Forward") \
	X(AC_STOP, 284, "AC Stop") \
	X(AC_REFRESH, 285, "AC Refresh") \
	X(AC_BOOKMARKS, 286, "AC Bookmarks") \
	X(SOFTLEFT, 287, "SoftLeft") \
	X(SOFTRIGHT, 288, "SoftRight") \
	X(CALL, 289, "Call") \
	X(ENDCALL, 290, "EndCall") \
	X(RESERVED, 400, "")

// RgKey is an integer and RG_KEY_* are plain integer macros (not enumerators),
// so SDL_Scancode values (same numbering) assign and compare freely: MSVC's
// C5287 flags any comparison between two enum types, even through casts.
typedef int RgKey;
#define RG_KEY_UNKNOWN 0
#define RG_KEY_A 4
#define RG_KEY_B 5
#define RG_KEY_C 6
#define RG_KEY_D 7
#define RG_KEY_E 8
#define RG_KEY_F 9
#define RG_KEY_G 10
#define RG_KEY_H 11
#define RG_KEY_I 12
#define RG_KEY_J 13
#define RG_KEY_K 14
#define RG_KEY_L 15
#define RG_KEY_M 16
#define RG_KEY_N 17
#define RG_KEY_O 18
#define RG_KEY_P 19
#define RG_KEY_Q 20
#define RG_KEY_R 21
#define RG_KEY_S 22
#define RG_KEY_T 23
#define RG_KEY_U 24
#define RG_KEY_V 25
#define RG_KEY_W 26
#define RG_KEY_X 27
#define RG_KEY_Y 28
#define RG_KEY_Z 29
#define RG_KEY_1 30
#define RG_KEY_2 31
#define RG_KEY_3 32
#define RG_KEY_4 33
#define RG_KEY_5 34
#define RG_KEY_6 35
#define RG_KEY_7 36
#define RG_KEY_8 37
#define RG_KEY_9 38
#define RG_KEY_0 39
#define RG_KEY_RETURN 40
#define RG_KEY_ESCAPE 41
#define RG_KEY_BACKSPACE 42
#define RG_KEY_TAB 43
#define RG_KEY_SPACE 44
#define RG_KEY_MINUS 45
#define RG_KEY_EQUALS 46
#define RG_KEY_LEFTBRACKET 47
#define RG_KEY_RIGHTBRACKET 48
#define RG_KEY_BACKSLASH 49
#define RG_KEY_NONUSHASH 50
#define RG_KEY_SEMICOLON 51
#define RG_KEY_APOSTROPHE 52
#define RG_KEY_GRAVE 53
#define RG_KEY_COMMA 54
#define RG_KEY_PERIOD 55
#define RG_KEY_SLASH 56
#define RG_KEY_CAPSLOCK 57
#define RG_KEY_F1 58
#define RG_KEY_F2 59
#define RG_KEY_F3 60
#define RG_KEY_F4 61
#define RG_KEY_F5 62
#define RG_KEY_F6 63
#define RG_KEY_F7 64
#define RG_KEY_F8 65
#define RG_KEY_F9 66
#define RG_KEY_F10 67
#define RG_KEY_F11 68
#define RG_KEY_F12 69
#define RG_KEY_PRINTSCREEN 70
#define RG_KEY_SCROLLLOCK 71
#define RG_KEY_PAUSE 72
#define RG_KEY_INSERT 73
#define RG_KEY_HOME 74
#define RG_KEY_PAGEUP 75
#define RG_KEY_DELETE 76
#define RG_KEY_END 77
#define RG_KEY_PAGEDOWN 78
#define RG_KEY_RIGHT 79
#define RG_KEY_LEFT 80
#define RG_KEY_DOWN 81
#define RG_KEY_UP 82
#define RG_KEY_NUMLOCKCLEAR 83
#define RG_KEY_KP_DIVIDE 84
#define RG_KEY_KP_MULTIPLY 85
#define RG_KEY_KP_MINUS 86
#define RG_KEY_KP_PLUS 87
#define RG_KEY_KP_ENTER 88
#define RG_KEY_KP_1 89
#define RG_KEY_KP_2 90
#define RG_KEY_KP_3 91
#define RG_KEY_KP_4 92
#define RG_KEY_KP_5 93
#define RG_KEY_KP_6 94
#define RG_KEY_KP_7 95
#define RG_KEY_KP_8 96
#define RG_KEY_KP_9 97
#define RG_KEY_KP_0 98
#define RG_KEY_KP_PERIOD 99
#define RG_KEY_NONUSBACKSLASH 100
#define RG_KEY_APPLICATION 101
#define RG_KEY_POWER 102
#define RG_KEY_KP_EQUALS 103
#define RG_KEY_F13 104
#define RG_KEY_F14 105
#define RG_KEY_F15 106
#define RG_KEY_F16 107
#define RG_KEY_F17 108
#define RG_KEY_F18 109
#define RG_KEY_F19 110
#define RG_KEY_F20 111
#define RG_KEY_F21 112
#define RG_KEY_F22 113
#define RG_KEY_F23 114
#define RG_KEY_F24 115
#define RG_KEY_EXECUTE 116
#define RG_KEY_HELP 117
#define RG_KEY_MENU 118
#define RG_KEY_SELECT 119
#define RG_KEY_STOP 120
#define RG_KEY_AGAIN 121
#define RG_KEY_UNDO 122
#define RG_KEY_CUT 123
#define RG_KEY_COPY 124
#define RG_KEY_PASTE 125
#define RG_KEY_FIND 126
#define RG_KEY_MUTE 127
#define RG_KEY_VOLUMEUP 128
#define RG_KEY_VOLUMEDOWN 129
#define RG_KEY_KP_COMMA 133
#define RG_KEY_KP_EQUALSAS400 134
#define RG_KEY_INTERNATIONAL1 135
#define RG_KEY_INTERNATIONAL2 136
#define RG_KEY_INTERNATIONAL3 137
#define RG_KEY_INTERNATIONAL4 138
#define RG_KEY_INTERNATIONAL5 139
#define RG_KEY_INTERNATIONAL6 140
#define RG_KEY_INTERNATIONAL7 141
#define RG_KEY_INTERNATIONAL8 142
#define RG_KEY_INTERNATIONAL9 143
#define RG_KEY_LANG1 144
#define RG_KEY_LANG2 145
#define RG_KEY_LANG3 146
#define RG_KEY_LANG4 147
#define RG_KEY_LANG5 148
#define RG_KEY_LANG6 149
#define RG_KEY_LANG7 150
#define RG_KEY_LANG8 151
#define RG_KEY_LANG9 152
#define RG_KEY_ALTERASE 153
#define RG_KEY_SYSREQ 154
#define RG_KEY_CANCEL 155
#define RG_KEY_CLEAR 156
#define RG_KEY_PRIOR 157
#define RG_KEY_RETURN2 158
#define RG_KEY_SEPARATOR 159
#define RG_KEY_OUT 160
#define RG_KEY_OPER 161
#define RG_KEY_CLEARAGAIN 162
#define RG_KEY_CRSEL 163
#define RG_KEY_EXSEL 164
#define RG_KEY_KP_00 176
#define RG_KEY_KP_000 177
#define RG_KEY_THOUSANDSSEPARATOR 178
#define RG_KEY_DECIMALSEPARATOR 179
#define RG_KEY_CURRENCYUNIT 180
#define RG_KEY_CURRENCYSUBUNIT 181
#define RG_KEY_KP_LEFTPAREN 182
#define RG_KEY_KP_RIGHTPAREN 183
#define RG_KEY_KP_LEFTBRACE 184
#define RG_KEY_KP_RIGHTBRACE 185
#define RG_KEY_KP_TAB 186
#define RG_KEY_KP_BACKSPACE 187
#define RG_KEY_KP_A 188
#define RG_KEY_KP_B 189
#define RG_KEY_KP_C 190
#define RG_KEY_KP_D 191
#define RG_KEY_KP_E 192
#define RG_KEY_KP_F 193
#define RG_KEY_KP_XOR 194
#define RG_KEY_KP_POWER 195
#define RG_KEY_KP_PERCENT 196
#define RG_KEY_KP_LESS 197
#define RG_KEY_KP_GREATER 198
#define RG_KEY_KP_AMPERSAND 199
#define RG_KEY_KP_DBLAMPERSAND 200
#define RG_KEY_KP_VERTICALBAR 201
#define RG_KEY_KP_DBLVERTICALBAR 202
#define RG_KEY_KP_COLON 203
#define RG_KEY_KP_HASH 204
#define RG_KEY_KP_SPACE 205
#define RG_KEY_KP_AT 206
#define RG_KEY_KP_EXCLAM 207
#define RG_KEY_KP_MEMSTORE 208
#define RG_KEY_KP_MEMRECALL 209
#define RG_KEY_KP_MEMCLEAR 210
#define RG_KEY_KP_MEMADD 211
#define RG_KEY_KP_MEMSUBTRACT 212
#define RG_KEY_KP_MEMMULTIPLY 213
#define RG_KEY_KP_MEMDIVIDE 214
#define RG_KEY_KP_PLUSMINUS 215
#define RG_KEY_KP_CLEAR 216
#define RG_KEY_KP_CLEARENTRY 217
#define RG_KEY_KP_BINARY 218
#define RG_KEY_KP_OCTAL 219
#define RG_KEY_KP_DECIMAL 220
#define RG_KEY_KP_HEXADECIMAL 221
#define RG_KEY_LCTRL 224
#define RG_KEY_LSHIFT 225
#define RG_KEY_LALT 226
#define RG_KEY_LGUI 227
#define RG_KEY_RCTRL 228
#define RG_KEY_RSHIFT 229
#define RG_KEY_RALT 230
#define RG_KEY_RGUI 231
#define RG_KEY_MODE 257
#define RG_KEY_SLEEP 258
#define RG_KEY_WAKE 259
#define RG_KEY_CHANNEL_INCREMENT 260
#define RG_KEY_CHANNEL_DECREMENT 261
#define RG_KEY_MEDIA_PLAY 262
#define RG_KEY_MEDIA_PAUSE 263
#define RG_KEY_MEDIA_RECORD 264
#define RG_KEY_MEDIA_FAST_FORWARD 265
#define RG_KEY_MEDIA_REWIND 266
#define RG_KEY_MEDIA_NEXT_TRACK 267
#define RG_KEY_MEDIA_PREVIOUS_TRACK 268
#define RG_KEY_MEDIA_STOP 269
#define RG_KEY_MEDIA_EJECT 270
#define RG_KEY_MEDIA_PLAY_PAUSE 271
#define RG_KEY_MEDIA_SELECT 272
#define RG_KEY_AC_NEW 273
#define RG_KEY_AC_OPEN 274
#define RG_KEY_AC_CLOSE 275
#define RG_KEY_AC_EXIT 276
#define RG_KEY_AC_SAVE 277
#define RG_KEY_AC_PRINT 278
#define RG_KEY_AC_PROPERTIES 279
#define RG_KEY_AC_SEARCH 280
#define RG_KEY_AC_HOME 281
#define RG_KEY_AC_BACK 282
#define RG_KEY_AC_FORWARD 283
#define RG_KEY_AC_STOP 284
#define RG_KEY_AC_REFRESH 285
#define RG_KEY_AC_BOOKMARKS 286
#define RG_KEY_SOFTLEFT 287
#define RG_KEY_SOFTRIGHT 288
#define RG_KEY_CALL 289
#define RG_KEY_ENDCALL 290
#define RG_KEY_RESERVED 400
#define RG_KEY_COUNT 512

// Modifier bits (== SDL3 SDL_KMOD_*).
typedef u16 RgMod;
#define RG_MOD_NONE 0x0000u
#define RG_MOD_LSHIFT 0x0001u
#define RG_MOD_RSHIFT 0x0002u
#define RG_MOD_LEVEL5 0x0004u
#define RG_MOD_LCTRL 0x0040u
#define RG_MOD_RCTRL 0x0080u
#define RG_MOD_LALT 0x0100u
#define RG_MOD_RALT 0x0200u
#define RG_MOD_LGUI 0x0400u
#define RG_MOD_RGUI 0x0800u
#define RG_MOD_NUM 0x1000u
#define RG_MOD_CAPS 0x2000u
#define RG_MOD_MODE 0x4000u
#define RG_MOD_SCROLL 0x8000u
#define RG_MOD_CTRL (RG_MOD_LCTRL | RG_MOD_RCTRL)
#define RG_MOD_SHIFT (RG_MOD_LSHIFT | RG_MOD_RSHIFT)
#define RG_MOD_ALT (RG_MOD_LALT | RG_MOD_RALT)
#define RG_MOD_GUI (RG_MOD_LGUI | RG_MOD_RGUI)

// Backend-assigned ids; 0 means "none / any".
typedef u32 RgWindowId;
typedef u32 RgKeyboardId;
typedef u32 RgMouseId;
typedef u32 RgKeycode;         // layout-dependent key symbol (SDL_Keycode); 0 when unknown
typedef u32 RgMouseButtonFlags; // bit (1 << RG_MOUSE_BUTTON_*) per held button

typedef int RgWheelDirection; // RG_WHEEL_*; integer for the same reason as RgKey
#define RG_WHEEL_NORMAL 0
#define RG_WHEEL_FLIPPED 1

// =============================================================================
// TYPES
// =============================================================================

typedef enum RgInputEventKind
{
	RG_INPUT_EVENT_NONE = 0,
	RG_INPUT_EVENT_KEY_DOWN,
	RG_INPUT_EVENT_KEY_UP,
	RG_INPUT_EVENT_TEXT_INPUT,
	RG_INPUT_EVENT_TEXT_EDITING,
	RG_INPUT_EVENT_MOUSE_MOVE,
	RG_INPUT_EVENT_MOUSE_BUTTON_DOWN,
	RG_INPUT_EVENT_MOUSE_BUTTON_UP,
	RG_INPUT_EVENT_MOUSE_WHEEL,
	RG_INPUT_EVENT_WINDOW_FOCUS_GAINED,
	RG_INPUT_EVENT_WINDOW_FOCUS_LOST,
	RG_INPUT_EVENT_COUNT
} RgInputEventKind;

typedef struct RgInputStringView
{
	const char* data;
	size_t length; // UTF-8 bytes, excluding the stored terminating NUL.
} RgInputStringView;

typedef struct RgInputKeyEvent
{
	RgKeyboardId keyboard_id;
	RgKey scancode;
	RgKeycode keycode;
	u16 raw;
	bool repeat;
} RgInputKeyEvent;

typedef struct RgInputTextEvent
{
	RgInputStringView text;
	i32 start;  // IME character offset; may be -1.
	i32 length; // IME character count, not a byte count.
} RgInputTextEvent;

typedef struct RgInputMouseMotionEvent
{
	RgMouseId mouse_id;
	RgMouseButtonFlags buttons;
	f32 x;
	f32 y;
	f32 dx;
	f32 dy;
} RgInputMouseMotionEvent;

typedef struct RgInputMouseButtonEvent
{
	RgMouseId mouse_id;
	i8 button; // Zero-based RG_MOUSE_BUTTON_* or -1 when unknown.
	u8 clicks;
	f32 x;
	f32 y;
} RgInputMouseButtonEvent;

typedef struct RgInputMouseWheelEvent
{
	RgMouseId mouse_id;
	f32 x;
	f32 y;
	f32 mouse_x;
	f32 mouse_y;
	RgWheelDirection direction;
} RgInputMouseWheelEvent;

typedef struct RgInputEvent
{
	RgInputEventKind kind;
	u64 timestamp_ns;
	RgWindowId window_id;
	RgMod modifiers;
	union
	{
		RgInputKeyEvent key;
		RgInputTextEvent text;
		RgInputMouseMotionEvent motion;
		RgInputMouseButtonEvent button;
		RgInputMouseWheelEvent wheel;
	} data;
} RgInputEvent;

/**
 * Optional caller-owned ordered event queue.
 *
 * Consumers should treat the queue and its buffers as const. The modifiers
 * field tracks modifier changes in event order. Supply the starting modifier
 * state explicitly when resetting the queue each frame.
 */
typedef struct RgInputEventQueue
{
	RgInputEvent* events;
	size_t count;
	size_t capacity;

	char* text_buffer;
	size_t text_used;
	size_t text_capacity;

	size_t dropped_event_count;
	size_t dropped_text_byte_count;
	RgMod modifiers;
} RgInputEventQueue;

typedef struct RgInputState
{
	bool current_keyboard[RG_KEY_COUNT];
	bool previous_keyboard[RG_KEY_COUNT];

	int mouse_x;
	int mouse_y;
	int mouse_delta_x;
	int mouse_delta_y;
	f32 mouse_scroll_y;
	f32 mouse_scroll_x; // horizontal wheel / shift-wheel, positive = right
	bool current_mouse[RG_MOUSE_BUTTON_COUNT];
	bool previous_mouse[RG_MOUSE_BUTTON_COUNT];
	int relative_mouse_enabled;

	bool text_input_active;
	char text_input_buffer[RG_INPUT_TEXT_BUFFER_SIZE]; // every text event of the frame, concatenated
	bool has_text_input;
} RgInputState;

// =============================================================================
// PUBLIC API
// =============================================================================

/** Initialize input state. */
RGINLINE void rg_input_init(RgInputState* input);

/** Snapshot previous state and clear text, wheel and mouse deltas before the
 *  frame's input is fed. */
RGINLINE void rg_input_begin_frame(RgInputState* input);

// ---- feeders (called by a backend as its events arrive) ----

RGINLINE void rg_input_set_key(RgInputState* input, RgKey key, bool down);
/** Release every key (window lost focus). */
RGINLINE void rg_input_clear_keys(RgInputState* input);
RGINLINE void rg_input_set_mouse_button(RgInputState* input, int button, bool down);
/** Absolute position; the frame delta accumulates from successive calls. */
RGINLINE void rg_input_set_mouse_position(RgInputState* input, int x, int y);
/** Relative motion (relative mouse mode). */
RGINLINE void rg_input_add_mouse_delta(RgInputState* input, int dx, int dy);
RGINLINE void rg_input_add_wheel(RgInputState* input, f32 dx, f32 dy);
/** Append UTF-8 text to the frame's text buffer. Whole code points only: a
 *  sequence that does not fit is dropped entirely. Returns 1 when stored. */
RGINLINE int rg_input_push_text(RgInputState* input, const char* utf8);

// ---- ordered event queue ----

RGINLINE void rg_input_event_queue_init(RgInputEventQueue* queue,
                                        RgInputEvent* events,
                                        size_t event_capacity,
                                        char* text_buffer,
                                        size_t text_capacity);
/** Reset per-frame queue contents, telemetry, and initial modifiers. */
RGINLINE void rg_input_event_queue_reset(RgInputEventQueue* queue, RgMod initial_modifiers);
/** Append a zeroed event; NULL (and a dropped count) when full. */
RGINLINE RgInputEvent* rg_input_event_queue_push(RgInputEventQueue* queue);
/** Append a zeroed event whose text view points at a copy of `text`. */
RGINLINE RgInputEvent* rg_input_event_queue_push_text(RgInputEventQueue* queue, const char* text);

// ---- queries ----

RGINLINE int rg_input_is_key_down(const RgInputState* input, RgKey key);
RGINLINE int rg_input_is_key_pressed(const RgInputState* input, RgKey key);
RGINLINE int rg_input_is_key_released(const RgInputState* input, RgKey key);
RGINLINE int rg_input_is_mouse_button_down(const RgInputState* input, int button);
RGINLINE int rg_input_is_mouse_button_pressed(const RgInputState* input, int button);
RGINLINE int rg_input_is_mouse_button_released(const RgInputState* input, int button);
RGINLINE void rg_input_get_mouse_delta(const RgInputState* input, int* dx, int* dy);
RGINLINE void rg_input_get_mouse_position(const RgInputState* input, int* x, int* y);
/** Human-readable key label ("A", "Return", "Left Ctrl"); "" when unknown. */
RGINLINE const char* rg_key_name(RgKey key);

// =============================================================================
// IMPLEMENTATION
// =============================================================================

RGINLINE void rg_input_init(RgInputState* input)
{
	RG_INPUT_ASSERT(input != NULL);
	memset(input, 0, sizeof(*input));
}

// Copy one RG_KEY_COUNT key-state array to another.
RGINLINE void rg_input_copy_keys(bool* RG_RESTRICT dst, const bool* RG_RESTRICT src)
{
	const size_t bytes = sizeof(bool) * (size_t)RG_KEY_COUNT;
	u8* RG_RESTRICT out = (u8*)dst;
	const u8* RG_RESTRICT in = (const u8*)src;
	size_t offset = 0;
#if RG_INPUT_HAS_SSE2
	for (; offset + 16u <= bytes; offset += 16u)
	{
		_mm_storeu_si128((__m128i*)(out + offset),
		                 _mm_loadu_si128((const __m128i*)(in + offset)));
	}
#elif RG_INPUT_HAS_NEON
	for (; offset + 16u <= bytes; offset += 16u)
	{
		vst1q_u8(out + offset, vld1q_u8(in + offset));
	}
#else
	for (; offset + 8u <= bytes; offset += 8u)
	{
		u64 chunk;
		memcpy(&chunk, in + offset, sizeof(chunk));
		memcpy(out + offset, &chunk, sizeof(chunk));
	}
#endif
	if (offset < bytes) memcpy(out + offset, in + offset, bytes - offset);
}

RGINLINE void rg_input_begin_frame(RgInputState* input)
{
	RG_INPUT_ASSERT(input != NULL);

	rg_input_copy_keys(input->previous_keyboard, input->current_keyboard);
	memcpy(input->previous_mouse, input->current_mouse, sizeof(input->current_mouse));

	input->has_text_input = false;
	input->text_input_buffer[0] = '\0';
	input->mouse_scroll_x = 0.0f;
	input->mouse_scroll_y = 0.0f;
	input->mouse_delta_x = 0;
	input->mouse_delta_y = 0;
}

RGINLINE void rg_input_set_key(RgInputState* input, RgKey key, bool down)
{
	if ((unsigned)key < (unsigned)RG_KEY_COUNT) input->current_keyboard[key] = down;
}

RGINLINE void rg_input_clear_keys(RgInputState* input)
{
	memset(input->current_keyboard, 0, sizeof(input->current_keyboard));
}

RGINLINE void rg_input_set_mouse_button(RgInputState* input, int button, bool down)
{
	if (button >= 0 && button < RG_MOUSE_BUTTON_COUNT) input->current_mouse[button] = down;
}

RGINLINE void rg_input_set_mouse_position(RgInputState* input, int x, int y)
{
	input->mouse_delta_x += x - input->mouse_x;
	input->mouse_delta_y += y - input->mouse_y;
	input->mouse_x = x;
	input->mouse_y = y;
}

RGINLINE void rg_input_add_mouse_delta(RgInputState* input, int dx, int dy)
{
	input->mouse_delta_x += dx;
	input->mouse_delta_y += dy;
}

RGINLINE void rg_input_add_wheel(RgInputState* input, f32 dx, f32 dy)
{
	input->mouse_scroll_x += dx;
	input->mouse_scroll_y += dy;
}

RGINLINE int rg_input_push_text(RgInputState* input, const char* utf8)
{
	size_t used = strlen(input->text_input_buffer);
	size_t length = strlen(utf8);
	// Keep whole code points: back off to the last lead byte that fits.
	while (length && used + length + 1u > (size_t)RG_INPUT_TEXT_BUFFER_SIZE)
	{
		length--;
		while (length && ((u8)utf8[length] & 0xC0u) == 0x80u) length--;
	}
	if (!length) return 0;
	memcpy(input->text_input_buffer + used, utf8, length);
	input->text_input_buffer[used + length] = '\0';
	input->has_text_input = true;
	return 1;
}

RGINLINE void rg_input_event_queue_init(RgInputEventQueue* queue,
                                        RgInputEvent* events,
                                        size_t event_capacity,
                                        char* text_buffer,
                                        size_t text_capacity)
{
	RG_INPUT_ASSERT(queue != NULL);
	RG_INPUT_ASSERT(events != NULL || event_capacity == 0);
	RG_INPUT_ASSERT(text_buffer != NULL || text_capacity == 0);

	memset(queue, 0, sizeof(*queue));
	queue->events = events;
	queue->capacity = event_capacity;
	queue->text_buffer = text_buffer;
	queue->text_capacity = text_capacity;
	queue->modifiers = RG_MOD_NONE;

	if (text_buffer && text_capacity > 0)
	{
		text_buffer[0] = '\0';
	}
}

RGINLINE void rg_input_event_queue_reset(RgInputEventQueue* queue, RgMod initial_modifiers)
{
	RG_INPUT_ASSERT(queue != NULL);

	queue->count = 0;
	queue->text_used = 0;
	queue->dropped_event_count = 0;
	queue->dropped_text_byte_count = 0;
	queue->modifiers = initial_modifiers;

	if (queue->text_buffer && queue->text_capacity > 0)
	{
		queue->text_buffer[0] = '\0';
	}
}

RGINLINE RgInputEvent* rg_input_event_queue_push(RgInputEventQueue* queue)
{
	if (queue->count >= queue->capacity)
	{
		queue->dropped_event_count++;
		return NULL;
	}

	RgInputEvent* result = &queue->events[queue->count++];
	memset(result, 0, sizeof(*result));
	return result;
}

RGINLINE RgInputEvent* rg_input_event_queue_push_text(RgInputEventQueue* queue, const char* text)
{
	const size_t length = strlen(text);
	const size_t required = length + 1;

	if (queue->count >= queue->capacity)
	{
		queue->dropped_event_count++;
		return NULL;
	}

	if (required > queue->text_capacity - queue->text_used)
	{
		queue->dropped_event_count++;
		queue->dropped_text_byte_count += required;
		return NULL;
	}

	char* copied_text = queue->text_buffer + queue->text_used;
	memcpy(copied_text, text, required);
	queue->text_used += required;

	RgInputEvent* result = &queue->events[queue->count++];
	memset(result, 0, sizeof(*result));
	result->data.text.text.data = copied_text;
	result->data.text.text.length = length;
	return result;
}

RGINLINE int rg_input_is_key_down(const RgInputState* input, RgKey key)
{
	return input->current_keyboard[key] ? 1 : 0;
}

RGINLINE int rg_input_is_key_pressed(const RgInputState* input, RgKey key)
{
	return (input->current_keyboard[key] && !input->previous_keyboard[key]) ? 1 : 0;
}

RGINLINE int rg_input_is_key_released(const RgInputState* input, RgKey key)
{
	return (!input->current_keyboard[key] && input->previous_keyboard[key]) ? 1 : 0;
}

RGINLINE int rg_input_is_mouse_button_down(const RgInputState* input, int button)
{
	if (button < 0 || button >= RG_MOUSE_BUTTON_COUNT)
	{
		return 0;
	}
	return input->current_mouse[button] ? 1 : 0;
}

RGINLINE int rg_input_is_mouse_button_pressed(const RgInputState* input, int button)
{
	if (button < 0 || button >= RG_MOUSE_BUTTON_COUNT)
	{
		return 0;
	}
	return (input->current_mouse[button] && !input->previous_mouse[button]) ? 1 : 0;
}

RGINLINE int rg_input_is_mouse_button_released(const RgInputState* input, int button)
{
	if (button < 0 || button >= RG_MOUSE_BUTTON_COUNT)
	{
		return 0;
	}
	return (!input->current_mouse[button] && input->previous_mouse[button]) ? 1 : 0;
}

RGINLINE void rg_input_get_mouse_delta(const RgInputState* input, int* dx, int* dy)
{
	if (dx)
	{
		*dx = input->mouse_delta_x;
	}
	if (dy)
	{
		*dy = input->mouse_delta_y;
	}
}

RGINLINE void rg_input_get_mouse_position(const RgInputState* input, int* x, int* y)
{
	if (x)
	{
		*x = input->mouse_x;
	}
	if (y)
	{
		*y = input->mouse_y;
	}
}

RGINLINE const char* rg_key_name(RgKey key)
{
	switch (key)
	{
#define X(name, value, label) case RG_KEY_##name: return label;
		RG_KEY_LIST(X)
#undef X
		default: return "";
	}
}

#endif // RG_INPUT_CORE_H
