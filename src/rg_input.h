// rg_input - SDL3 adapter over rg_input_core
//
// Part of the Reverse Gravity (rg_) core libraries.
// Single-header C99 helper for keyboard, mouse, and text input state fed from
// SDL3. The state, queue and query functions live in rg_input_core.h (no SDL);
// this header adds sampling and SDL event translation and proves at compile
// time that RgKey / RgMod values equal SDL's, so SDL_SCANCODE_* and SDL_KMOD_*
// may be used anywhere an RgKey / RgMod is expected.
//
// USAGE:
//   #include "rg_input.h"
//
//   RgInputState input = {0};
//   rg_input_init(&input);
//
//   // Per frame:
//   rg_input_begin_frame(&input);
//   SDL_Event event;
//   while (SDL_PollEvent(&event))
//   {
//       rg_input_process_event(&input, &event);
//   }
//   rg_input_sample(&input); // consume input after SDL has pumped events
//
// OPTIONS: see rg_input_core.h (RG_INPUT_TEXT_BUFFER_SIZE, RG_INPUT_ASSERT, RG_INPUT_NO_SIMD).
//
// NOTES:
//   - All functions have internal linkage and work in unity builds.
//   - Begin before polling; sample once after polling, before gameplay.
//   - Neither helper pumps SDL events.
//   - Text events within one frame are concatenated in text_input_buffer.
//
// Author: Steven Wendel (superwendel)

#ifndef RG_INPUT_H
#define RG_INPUT_H

#include "rg_input_core.h"

#include <SDL3/SDL.h>

// Every RgKey / RgMod must equal its SDL3 counterpart.
// (Written as a difference of zero: MSVC C5287 fires on == between enum types even through casts.)
// Keys SDL3 has since removed are not asserted: 
typedef char rg_input_assert_key_UNKNOWN[((int)RG_KEY_UNKNOWN - (int)SDL_SCANCODE_UNKNOWN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_A[((int)RG_KEY_A - (int)SDL_SCANCODE_A) == 0 ? 1 : -1];
typedef char rg_input_assert_key_B[((int)RG_KEY_B - (int)SDL_SCANCODE_B) == 0 ? 1 : -1];
typedef char rg_input_assert_key_C[((int)RG_KEY_C - (int)SDL_SCANCODE_C) == 0 ? 1 : -1];
typedef char rg_input_assert_key_D[((int)RG_KEY_D - (int)SDL_SCANCODE_D) == 0 ? 1 : -1];
typedef char rg_input_assert_key_E[((int)RG_KEY_E - (int)SDL_SCANCODE_E) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F[((int)RG_KEY_F - (int)SDL_SCANCODE_F) == 0 ? 1 : -1];
typedef char rg_input_assert_key_G[((int)RG_KEY_G - (int)SDL_SCANCODE_G) == 0 ? 1 : -1];
typedef char rg_input_assert_key_H[((int)RG_KEY_H - (int)SDL_SCANCODE_H) == 0 ? 1 : -1];
typedef char rg_input_assert_key_I[((int)RG_KEY_I - (int)SDL_SCANCODE_I) == 0 ? 1 : -1];
typedef char rg_input_assert_key_J[((int)RG_KEY_J - (int)SDL_SCANCODE_J) == 0 ? 1 : -1];
typedef char rg_input_assert_key_K[((int)RG_KEY_K - (int)SDL_SCANCODE_K) == 0 ? 1 : -1];
typedef char rg_input_assert_key_L[((int)RG_KEY_L - (int)SDL_SCANCODE_L) == 0 ? 1 : -1];
typedef char rg_input_assert_key_M[((int)RG_KEY_M - (int)SDL_SCANCODE_M) == 0 ? 1 : -1];
typedef char rg_input_assert_key_N[((int)RG_KEY_N - (int)SDL_SCANCODE_N) == 0 ? 1 : -1];
typedef char rg_input_assert_key_O[((int)RG_KEY_O - (int)SDL_SCANCODE_O) == 0 ? 1 : -1];
typedef char rg_input_assert_key_P[((int)RG_KEY_P - (int)SDL_SCANCODE_P) == 0 ? 1 : -1];
typedef char rg_input_assert_key_Q[((int)RG_KEY_Q - (int)SDL_SCANCODE_Q) == 0 ? 1 : -1];
typedef char rg_input_assert_key_R[((int)RG_KEY_R - (int)SDL_SCANCODE_R) == 0 ? 1 : -1];
typedef char rg_input_assert_key_S[((int)RG_KEY_S - (int)SDL_SCANCODE_S) == 0 ? 1 : -1];
typedef char rg_input_assert_key_T[((int)RG_KEY_T - (int)SDL_SCANCODE_T) == 0 ? 1 : -1];
typedef char rg_input_assert_key_U[((int)RG_KEY_U - (int)SDL_SCANCODE_U) == 0 ? 1 : -1];
typedef char rg_input_assert_key_V[((int)RG_KEY_V - (int)SDL_SCANCODE_V) == 0 ? 1 : -1];
typedef char rg_input_assert_key_W[((int)RG_KEY_W - (int)SDL_SCANCODE_W) == 0 ? 1 : -1];
typedef char rg_input_assert_key_X[((int)RG_KEY_X - (int)SDL_SCANCODE_X) == 0 ? 1 : -1];
typedef char rg_input_assert_key_Y[((int)RG_KEY_Y - (int)SDL_SCANCODE_Y) == 0 ? 1 : -1];
typedef char rg_input_assert_key_Z[((int)RG_KEY_Z - (int)SDL_SCANCODE_Z) == 0 ? 1 : -1];
typedef char rg_input_assert_key_1[((int)RG_KEY_1 - (int)SDL_SCANCODE_1) == 0 ? 1 : -1];
typedef char rg_input_assert_key_2[((int)RG_KEY_2 - (int)SDL_SCANCODE_2) == 0 ? 1 : -1];
typedef char rg_input_assert_key_3[((int)RG_KEY_3 - (int)SDL_SCANCODE_3) == 0 ? 1 : -1];
typedef char rg_input_assert_key_4[((int)RG_KEY_4 - (int)SDL_SCANCODE_4) == 0 ? 1 : -1];
typedef char rg_input_assert_key_5[((int)RG_KEY_5 - (int)SDL_SCANCODE_5) == 0 ? 1 : -1];
typedef char rg_input_assert_key_6[((int)RG_KEY_6 - (int)SDL_SCANCODE_6) == 0 ? 1 : -1];
typedef char rg_input_assert_key_7[((int)RG_KEY_7 - (int)SDL_SCANCODE_7) == 0 ? 1 : -1];
typedef char rg_input_assert_key_8[((int)RG_KEY_8 - (int)SDL_SCANCODE_8) == 0 ? 1 : -1];
typedef char rg_input_assert_key_9[((int)RG_KEY_9 - (int)SDL_SCANCODE_9) == 0 ? 1 : -1];
typedef char rg_input_assert_key_0[((int)RG_KEY_0 - (int)SDL_SCANCODE_0) == 0 ? 1 : -1];
typedef char rg_input_assert_key_RETURN[((int)RG_KEY_RETURN - (int)SDL_SCANCODE_RETURN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_ESCAPE[((int)RG_KEY_ESCAPE - (int)SDL_SCANCODE_ESCAPE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_BACKSPACE[((int)RG_KEY_BACKSPACE - (int)SDL_SCANCODE_BACKSPACE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_TAB[((int)RG_KEY_TAB - (int)SDL_SCANCODE_TAB) == 0 ? 1 : -1];
typedef char rg_input_assert_key_SPACE[((int)RG_KEY_SPACE - (int)SDL_SCANCODE_SPACE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MINUS[((int)RG_KEY_MINUS - (int)SDL_SCANCODE_MINUS) == 0 ? 1 : -1];
typedef char rg_input_assert_key_EQUALS[((int)RG_KEY_EQUALS - (int)SDL_SCANCODE_EQUALS) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LEFTBRACKET[((int)RG_KEY_LEFTBRACKET - (int)SDL_SCANCODE_LEFTBRACKET) == 0 ? 1 : -1];
typedef char rg_input_assert_key_RIGHTBRACKET[((int)RG_KEY_RIGHTBRACKET - (int)SDL_SCANCODE_RIGHTBRACKET) == 0 ? 1 : -1];
typedef char rg_input_assert_key_BACKSLASH[((int)RG_KEY_BACKSLASH - (int)SDL_SCANCODE_BACKSLASH) == 0 ? 1 : -1];
typedef char rg_input_assert_key_NONUSHASH[((int)RG_KEY_NONUSHASH - (int)SDL_SCANCODE_NONUSHASH) == 0 ? 1 : -1];
typedef char rg_input_assert_key_SEMICOLON[((int)RG_KEY_SEMICOLON - (int)SDL_SCANCODE_SEMICOLON) == 0 ? 1 : -1];
typedef char rg_input_assert_key_APOSTROPHE[((int)RG_KEY_APOSTROPHE - (int)SDL_SCANCODE_APOSTROPHE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_GRAVE[((int)RG_KEY_GRAVE - (int)SDL_SCANCODE_GRAVE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_COMMA[((int)RG_KEY_COMMA - (int)SDL_SCANCODE_COMMA) == 0 ? 1 : -1];
typedef char rg_input_assert_key_PERIOD[((int)RG_KEY_PERIOD - (int)SDL_SCANCODE_PERIOD) == 0 ? 1 : -1];
typedef char rg_input_assert_key_SLASH[((int)RG_KEY_SLASH - (int)SDL_SCANCODE_SLASH) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CAPSLOCK[((int)RG_KEY_CAPSLOCK - (int)SDL_SCANCODE_CAPSLOCK) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F1[((int)RG_KEY_F1 - (int)SDL_SCANCODE_F1) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F2[((int)RG_KEY_F2 - (int)SDL_SCANCODE_F2) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F3[((int)RG_KEY_F3 - (int)SDL_SCANCODE_F3) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F4[((int)RG_KEY_F4 - (int)SDL_SCANCODE_F4) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F5[((int)RG_KEY_F5 - (int)SDL_SCANCODE_F5) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F6[((int)RG_KEY_F6 - (int)SDL_SCANCODE_F6) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F7[((int)RG_KEY_F7 - (int)SDL_SCANCODE_F7) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F8[((int)RG_KEY_F8 - (int)SDL_SCANCODE_F8) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F9[((int)RG_KEY_F9 - (int)SDL_SCANCODE_F9) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F10[((int)RG_KEY_F10 - (int)SDL_SCANCODE_F10) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F11[((int)RG_KEY_F11 - (int)SDL_SCANCODE_F11) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F12[((int)RG_KEY_F12 - (int)SDL_SCANCODE_F12) == 0 ? 1 : -1];
typedef char rg_input_assert_key_PRINTSCREEN[((int)RG_KEY_PRINTSCREEN - (int)SDL_SCANCODE_PRINTSCREEN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_SCROLLLOCK[((int)RG_KEY_SCROLLLOCK - (int)SDL_SCANCODE_SCROLLLOCK) == 0 ? 1 : -1];
typedef char rg_input_assert_key_PAUSE[((int)RG_KEY_PAUSE - (int)SDL_SCANCODE_PAUSE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_INSERT[((int)RG_KEY_INSERT - (int)SDL_SCANCODE_INSERT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_HOME[((int)RG_KEY_HOME - (int)SDL_SCANCODE_HOME) == 0 ? 1 : -1];
typedef char rg_input_assert_key_PAGEUP[((int)RG_KEY_PAGEUP - (int)SDL_SCANCODE_PAGEUP) == 0 ? 1 : -1];
typedef char rg_input_assert_key_DELETE[((int)RG_KEY_DELETE - (int)SDL_SCANCODE_DELETE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_END[((int)RG_KEY_END - (int)SDL_SCANCODE_END) == 0 ? 1 : -1];
typedef char rg_input_assert_key_PAGEDOWN[((int)RG_KEY_PAGEDOWN - (int)SDL_SCANCODE_PAGEDOWN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_RIGHT[((int)RG_KEY_RIGHT - (int)SDL_SCANCODE_RIGHT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LEFT[((int)RG_KEY_LEFT - (int)SDL_SCANCODE_LEFT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_DOWN[((int)RG_KEY_DOWN - (int)SDL_SCANCODE_DOWN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_UP[((int)RG_KEY_UP - (int)SDL_SCANCODE_UP) == 0 ? 1 : -1];
typedef char rg_input_assert_key_NUMLOCKCLEAR[((int)RG_KEY_NUMLOCKCLEAR - (int)SDL_SCANCODE_NUMLOCKCLEAR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_DIVIDE[((int)RG_KEY_KP_DIVIDE - (int)SDL_SCANCODE_KP_DIVIDE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_MULTIPLY[((int)RG_KEY_KP_MULTIPLY - (int)SDL_SCANCODE_KP_MULTIPLY) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_MINUS[((int)RG_KEY_KP_MINUS - (int)SDL_SCANCODE_KP_MINUS) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_PLUS[((int)RG_KEY_KP_PLUS - (int)SDL_SCANCODE_KP_PLUS) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_ENTER[((int)RG_KEY_KP_ENTER - (int)SDL_SCANCODE_KP_ENTER) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_1[((int)RG_KEY_KP_1 - (int)SDL_SCANCODE_KP_1) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_2[((int)RG_KEY_KP_2 - (int)SDL_SCANCODE_KP_2) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_3[((int)RG_KEY_KP_3 - (int)SDL_SCANCODE_KP_3) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_4[((int)RG_KEY_KP_4 - (int)SDL_SCANCODE_KP_4) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_5[((int)RG_KEY_KP_5 - (int)SDL_SCANCODE_KP_5) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_6[((int)RG_KEY_KP_6 - (int)SDL_SCANCODE_KP_6) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_7[((int)RG_KEY_KP_7 - (int)SDL_SCANCODE_KP_7) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_8[((int)RG_KEY_KP_8 - (int)SDL_SCANCODE_KP_8) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_9[((int)RG_KEY_KP_9 - (int)SDL_SCANCODE_KP_9) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_0[((int)RG_KEY_KP_0 - (int)SDL_SCANCODE_KP_0) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_PERIOD[((int)RG_KEY_KP_PERIOD - (int)SDL_SCANCODE_KP_PERIOD) == 0 ? 1 : -1];
typedef char rg_input_assert_key_NONUSBACKSLASH[((int)RG_KEY_NONUSBACKSLASH - (int)SDL_SCANCODE_NONUSBACKSLASH) == 0 ? 1 : -1];
typedef char rg_input_assert_key_APPLICATION[((int)RG_KEY_APPLICATION - (int)SDL_SCANCODE_APPLICATION) == 0 ? 1 : -1];
typedef char rg_input_assert_key_POWER[((int)RG_KEY_POWER - (int)SDL_SCANCODE_POWER) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_EQUALS[((int)RG_KEY_KP_EQUALS - (int)SDL_SCANCODE_KP_EQUALS) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F13[((int)RG_KEY_F13 - (int)SDL_SCANCODE_F13) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F14[((int)RG_KEY_F14 - (int)SDL_SCANCODE_F14) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F15[((int)RG_KEY_F15 - (int)SDL_SCANCODE_F15) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F16[((int)RG_KEY_F16 - (int)SDL_SCANCODE_F16) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F17[((int)RG_KEY_F17 - (int)SDL_SCANCODE_F17) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F18[((int)RG_KEY_F18 - (int)SDL_SCANCODE_F18) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F19[((int)RG_KEY_F19 - (int)SDL_SCANCODE_F19) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F20[((int)RG_KEY_F20 - (int)SDL_SCANCODE_F20) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F21[((int)RG_KEY_F21 - (int)SDL_SCANCODE_F21) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F22[((int)RG_KEY_F22 - (int)SDL_SCANCODE_F22) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F23[((int)RG_KEY_F23 - (int)SDL_SCANCODE_F23) == 0 ? 1 : -1];
typedef char rg_input_assert_key_F24[((int)RG_KEY_F24 - (int)SDL_SCANCODE_F24) == 0 ? 1 : -1];
typedef char rg_input_assert_key_EXECUTE[((int)RG_KEY_EXECUTE - (int)SDL_SCANCODE_EXECUTE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_HELP[((int)RG_KEY_HELP - (int)SDL_SCANCODE_HELP) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MENU[((int)RG_KEY_MENU - (int)SDL_SCANCODE_MENU) == 0 ? 1 : -1];
typedef char rg_input_assert_key_SELECT[((int)RG_KEY_SELECT - (int)SDL_SCANCODE_SELECT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_STOP[((int)RG_KEY_STOP - (int)SDL_SCANCODE_STOP) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AGAIN[((int)RG_KEY_AGAIN - (int)SDL_SCANCODE_AGAIN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_UNDO[((int)RG_KEY_UNDO - (int)SDL_SCANCODE_UNDO) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CUT[((int)RG_KEY_CUT - (int)SDL_SCANCODE_CUT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_COPY[((int)RG_KEY_COPY - (int)SDL_SCANCODE_COPY) == 0 ? 1 : -1];
typedef char rg_input_assert_key_PASTE[((int)RG_KEY_PASTE - (int)SDL_SCANCODE_PASTE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_FIND[((int)RG_KEY_FIND - (int)SDL_SCANCODE_FIND) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MUTE[((int)RG_KEY_MUTE - (int)SDL_SCANCODE_MUTE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_VOLUMEUP[((int)RG_KEY_VOLUMEUP - (int)SDL_SCANCODE_VOLUMEUP) == 0 ? 1 : -1];
typedef char rg_input_assert_key_VOLUMEDOWN[((int)RG_KEY_VOLUMEDOWN - (int)SDL_SCANCODE_VOLUMEDOWN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_COMMA[((int)RG_KEY_KP_COMMA - (int)SDL_SCANCODE_KP_COMMA) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_EQUALSAS400[((int)RG_KEY_KP_EQUALSAS400 - (int)SDL_SCANCODE_KP_EQUALSAS400) == 0 ? 1 : -1];
typedef char rg_input_assert_key_INTERNATIONAL1[((int)RG_KEY_INTERNATIONAL1 - (int)SDL_SCANCODE_INTERNATIONAL1) == 0 ? 1 : -1];
typedef char rg_input_assert_key_INTERNATIONAL2[((int)RG_KEY_INTERNATIONAL2 - (int)SDL_SCANCODE_INTERNATIONAL2) == 0 ? 1 : -1];
typedef char rg_input_assert_key_INTERNATIONAL3[((int)RG_KEY_INTERNATIONAL3 - (int)SDL_SCANCODE_INTERNATIONAL3) == 0 ? 1 : -1];
typedef char rg_input_assert_key_INTERNATIONAL4[((int)RG_KEY_INTERNATIONAL4 - (int)SDL_SCANCODE_INTERNATIONAL4) == 0 ? 1 : -1];
typedef char rg_input_assert_key_INTERNATIONAL5[((int)RG_KEY_INTERNATIONAL5 - (int)SDL_SCANCODE_INTERNATIONAL5) == 0 ? 1 : -1];
typedef char rg_input_assert_key_INTERNATIONAL6[((int)RG_KEY_INTERNATIONAL6 - (int)SDL_SCANCODE_INTERNATIONAL6) == 0 ? 1 : -1];
typedef char rg_input_assert_key_INTERNATIONAL7[((int)RG_KEY_INTERNATIONAL7 - (int)SDL_SCANCODE_INTERNATIONAL7) == 0 ? 1 : -1];
typedef char rg_input_assert_key_INTERNATIONAL8[((int)RG_KEY_INTERNATIONAL8 - (int)SDL_SCANCODE_INTERNATIONAL8) == 0 ? 1 : -1];
typedef char rg_input_assert_key_INTERNATIONAL9[((int)RG_KEY_INTERNATIONAL9 - (int)SDL_SCANCODE_INTERNATIONAL9) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LANG1[((int)RG_KEY_LANG1 - (int)SDL_SCANCODE_LANG1) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LANG2[((int)RG_KEY_LANG2 - (int)SDL_SCANCODE_LANG2) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LANG3[((int)RG_KEY_LANG3 - (int)SDL_SCANCODE_LANG3) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LANG4[((int)RG_KEY_LANG4 - (int)SDL_SCANCODE_LANG4) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LANG5[((int)RG_KEY_LANG5 - (int)SDL_SCANCODE_LANG5) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LANG6[((int)RG_KEY_LANG6 - (int)SDL_SCANCODE_LANG6) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LANG7[((int)RG_KEY_LANG7 - (int)SDL_SCANCODE_LANG7) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LANG8[((int)RG_KEY_LANG8 - (int)SDL_SCANCODE_LANG8) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LANG9[((int)RG_KEY_LANG9 - (int)SDL_SCANCODE_LANG9) == 0 ? 1 : -1];
typedef char rg_input_assert_key_ALTERASE[((int)RG_KEY_ALTERASE - (int)SDL_SCANCODE_ALTERASE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_SYSREQ[((int)RG_KEY_SYSREQ - (int)SDL_SCANCODE_SYSREQ) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CANCEL[((int)RG_KEY_CANCEL - (int)SDL_SCANCODE_CANCEL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CLEAR[((int)RG_KEY_CLEAR - (int)SDL_SCANCODE_CLEAR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_PRIOR[((int)RG_KEY_PRIOR - (int)SDL_SCANCODE_PRIOR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_RETURN2[((int)RG_KEY_RETURN2 - (int)SDL_SCANCODE_RETURN2) == 0 ? 1 : -1];
typedef char rg_input_assert_key_SEPARATOR[((int)RG_KEY_SEPARATOR - (int)SDL_SCANCODE_SEPARATOR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_OUT[((int)RG_KEY_OUT - (int)SDL_SCANCODE_OUT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_OPER[((int)RG_KEY_OPER - (int)SDL_SCANCODE_OPER) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CLEARAGAIN[((int)RG_KEY_CLEARAGAIN - (int)SDL_SCANCODE_CLEARAGAIN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CRSEL[((int)RG_KEY_CRSEL - (int)SDL_SCANCODE_CRSEL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_EXSEL[((int)RG_KEY_EXSEL - (int)SDL_SCANCODE_EXSEL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_00[((int)RG_KEY_KP_00 - (int)SDL_SCANCODE_KP_00) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_000[((int)RG_KEY_KP_000 - (int)SDL_SCANCODE_KP_000) == 0 ? 1 : -1];
typedef char rg_input_assert_key_THOUSANDSSEPARATOR[((int)RG_KEY_THOUSANDSSEPARATOR - (int)SDL_SCANCODE_THOUSANDSSEPARATOR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_DECIMALSEPARATOR[((int)RG_KEY_DECIMALSEPARATOR - (int)SDL_SCANCODE_DECIMALSEPARATOR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CURRENCYUNIT[((int)RG_KEY_CURRENCYUNIT - (int)SDL_SCANCODE_CURRENCYUNIT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CURRENCYSUBUNIT[((int)RG_KEY_CURRENCYSUBUNIT - (int)SDL_SCANCODE_CURRENCYSUBUNIT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_LEFTPAREN[((int)RG_KEY_KP_LEFTPAREN - (int)SDL_SCANCODE_KP_LEFTPAREN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_RIGHTPAREN[((int)RG_KEY_KP_RIGHTPAREN - (int)SDL_SCANCODE_KP_RIGHTPAREN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_LEFTBRACE[((int)RG_KEY_KP_LEFTBRACE - (int)SDL_SCANCODE_KP_LEFTBRACE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_RIGHTBRACE[((int)RG_KEY_KP_RIGHTBRACE - (int)SDL_SCANCODE_KP_RIGHTBRACE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_TAB[((int)RG_KEY_KP_TAB - (int)SDL_SCANCODE_KP_TAB) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_BACKSPACE[((int)RG_KEY_KP_BACKSPACE - (int)SDL_SCANCODE_KP_BACKSPACE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_A[((int)RG_KEY_KP_A - (int)SDL_SCANCODE_KP_A) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_B[((int)RG_KEY_KP_B - (int)SDL_SCANCODE_KP_B) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_C[((int)RG_KEY_KP_C - (int)SDL_SCANCODE_KP_C) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_D[((int)RG_KEY_KP_D - (int)SDL_SCANCODE_KP_D) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_E[((int)RG_KEY_KP_E - (int)SDL_SCANCODE_KP_E) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_F[((int)RG_KEY_KP_F - (int)SDL_SCANCODE_KP_F) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_XOR[((int)RG_KEY_KP_XOR - (int)SDL_SCANCODE_KP_XOR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_POWER[((int)RG_KEY_KP_POWER - (int)SDL_SCANCODE_KP_POWER) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_PERCENT[((int)RG_KEY_KP_PERCENT - (int)SDL_SCANCODE_KP_PERCENT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_LESS[((int)RG_KEY_KP_LESS - (int)SDL_SCANCODE_KP_LESS) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_GREATER[((int)RG_KEY_KP_GREATER - (int)SDL_SCANCODE_KP_GREATER) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_AMPERSAND[((int)RG_KEY_KP_AMPERSAND - (int)SDL_SCANCODE_KP_AMPERSAND) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_DBLAMPERSAND[((int)RG_KEY_KP_DBLAMPERSAND - (int)SDL_SCANCODE_KP_DBLAMPERSAND) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_VERTICALBAR[((int)RG_KEY_KP_VERTICALBAR - (int)SDL_SCANCODE_KP_VERTICALBAR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_DBLVERTICALBAR[((int)RG_KEY_KP_DBLVERTICALBAR - (int)SDL_SCANCODE_KP_DBLVERTICALBAR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_COLON[((int)RG_KEY_KP_COLON - (int)SDL_SCANCODE_KP_COLON) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_HASH[((int)RG_KEY_KP_HASH - (int)SDL_SCANCODE_KP_HASH) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_SPACE[((int)RG_KEY_KP_SPACE - (int)SDL_SCANCODE_KP_SPACE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_AT[((int)RG_KEY_KP_AT - (int)SDL_SCANCODE_KP_AT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_EXCLAM[((int)RG_KEY_KP_EXCLAM - (int)SDL_SCANCODE_KP_EXCLAM) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_MEMSTORE[((int)RG_KEY_KP_MEMSTORE - (int)SDL_SCANCODE_KP_MEMSTORE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_MEMRECALL[((int)RG_KEY_KP_MEMRECALL - (int)SDL_SCANCODE_KP_MEMRECALL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_MEMCLEAR[((int)RG_KEY_KP_MEMCLEAR - (int)SDL_SCANCODE_KP_MEMCLEAR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_MEMADD[((int)RG_KEY_KP_MEMADD - (int)SDL_SCANCODE_KP_MEMADD) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_MEMSUBTRACT[((int)RG_KEY_KP_MEMSUBTRACT - (int)SDL_SCANCODE_KP_MEMSUBTRACT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_MEMMULTIPLY[((int)RG_KEY_KP_MEMMULTIPLY - (int)SDL_SCANCODE_KP_MEMMULTIPLY) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_MEMDIVIDE[((int)RG_KEY_KP_MEMDIVIDE - (int)SDL_SCANCODE_KP_MEMDIVIDE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_PLUSMINUS[((int)RG_KEY_KP_PLUSMINUS - (int)SDL_SCANCODE_KP_PLUSMINUS) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_CLEAR[((int)RG_KEY_KP_CLEAR - (int)SDL_SCANCODE_KP_CLEAR) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_CLEARENTRY[((int)RG_KEY_KP_CLEARENTRY - (int)SDL_SCANCODE_KP_CLEARENTRY) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_BINARY[((int)RG_KEY_KP_BINARY - (int)SDL_SCANCODE_KP_BINARY) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_OCTAL[((int)RG_KEY_KP_OCTAL - (int)SDL_SCANCODE_KP_OCTAL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_DECIMAL[((int)RG_KEY_KP_DECIMAL - (int)SDL_SCANCODE_KP_DECIMAL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_KP_HEXADECIMAL[((int)RG_KEY_KP_HEXADECIMAL - (int)SDL_SCANCODE_KP_HEXADECIMAL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LCTRL[((int)RG_KEY_LCTRL - (int)SDL_SCANCODE_LCTRL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LSHIFT[((int)RG_KEY_LSHIFT - (int)SDL_SCANCODE_LSHIFT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LALT[((int)RG_KEY_LALT - (int)SDL_SCANCODE_LALT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_LGUI[((int)RG_KEY_LGUI - (int)SDL_SCANCODE_LGUI) == 0 ? 1 : -1];
typedef char rg_input_assert_key_RCTRL[((int)RG_KEY_RCTRL - (int)SDL_SCANCODE_RCTRL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_RSHIFT[((int)RG_KEY_RSHIFT - (int)SDL_SCANCODE_RSHIFT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_RALT[((int)RG_KEY_RALT - (int)SDL_SCANCODE_RALT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_RGUI[((int)RG_KEY_RGUI - (int)SDL_SCANCODE_RGUI) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MODE[((int)RG_KEY_MODE - (int)SDL_SCANCODE_MODE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_SLEEP[((int)RG_KEY_SLEEP - (int)SDL_SCANCODE_SLEEP) == 0 ? 1 : -1];
typedef char rg_input_assert_key_WAKE[((int)RG_KEY_WAKE - (int)SDL_SCANCODE_WAKE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CHANNEL_INCREMENT[((int)RG_KEY_CHANNEL_INCREMENT - (int)SDL_SCANCODE_CHANNEL_INCREMENT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CHANNEL_DECREMENT[((int)RG_KEY_CHANNEL_DECREMENT - (int)SDL_SCANCODE_CHANNEL_DECREMENT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_PLAY[((int)RG_KEY_MEDIA_PLAY - (int)SDL_SCANCODE_MEDIA_PLAY) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_PAUSE[((int)RG_KEY_MEDIA_PAUSE - (int)SDL_SCANCODE_MEDIA_PAUSE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_RECORD[((int)RG_KEY_MEDIA_RECORD - (int)SDL_SCANCODE_MEDIA_RECORD) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_FAST_FORWARD[((int)RG_KEY_MEDIA_FAST_FORWARD - (int)SDL_SCANCODE_MEDIA_FAST_FORWARD) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_REWIND[((int)RG_KEY_MEDIA_REWIND - (int)SDL_SCANCODE_MEDIA_REWIND) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_NEXT_TRACK[((int)RG_KEY_MEDIA_NEXT_TRACK - (int)SDL_SCANCODE_MEDIA_NEXT_TRACK) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_PREVIOUS_TRACK[((int)RG_KEY_MEDIA_PREVIOUS_TRACK - (int)SDL_SCANCODE_MEDIA_PREVIOUS_TRACK) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_STOP[((int)RG_KEY_MEDIA_STOP - (int)SDL_SCANCODE_MEDIA_STOP) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_EJECT[((int)RG_KEY_MEDIA_EJECT - (int)SDL_SCANCODE_MEDIA_EJECT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_PLAY_PAUSE[((int)RG_KEY_MEDIA_PLAY_PAUSE - (int)SDL_SCANCODE_MEDIA_PLAY_PAUSE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_MEDIA_SELECT[((int)RG_KEY_MEDIA_SELECT - (int)SDL_SCANCODE_MEDIA_SELECT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_NEW[((int)RG_KEY_AC_NEW - (int)SDL_SCANCODE_AC_NEW) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_OPEN[((int)RG_KEY_AC_OPEN - (int)SDL_SCANCODE_AC_OPEN) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_CLOSE[((int)RG_KEY_AC_CLOSE - (int)SDL_SCANCODE_AC_CLOSE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_EXIT[((int)RG_KEY_AC_EXIT - (int)SDL_SCANCODE_AC_EXIT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_SAVE[((int)RG_KEY_AC_SAVE - (int)SDL_SCANCODE_AC_SAVE) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_PRINT[((int)RG_KEY_AC_PRINT - (int)SDL_SCANCODE_AC_PRINT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_PROPERTIES[((int)RG_KEY_AC_PROPERTIES - (int)SDL_SCANCODE_AC_PROPERTIES) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_SEARCH[((int)RG_KEY_AC_SEARCH - (int)SDL_SCANCODE_AC_SEARCH) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_HOME[((int)RG_KEY_AC_HOME - (int)SDL_SCANCODE_AC_HOME) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_BACK[((int)RG_KEY_AC_BACK - (int)SDL_SCANCODE_AC_BACK) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_FORWARD[((int)RG_KEY_AC_FORWARD - (int)SDL_SCANCODE_AC_FORWARD) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_STOP[((int)RG_KEY_AC_STOP - (int)SDL_SCANCODE_AC_STOP) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_REFRESH[((int)RG_KEY_AC_REFRESH - (int)SDL_SCANCODE_AC_REFRESH) == 0 ? 1 : -1];
typedef char rg_input_assert_key_AC_BOOKMARKS[((int)RG_KEY_AC_BOOKMARKS - (int)SDL_SCANCODE_AC_BOOKMARKS) == 0 ? 1 : -1];
typedef char rg_input_assert_key_SOFTLEFT[((int)RG_KEY_SOFTLEFT - (int)SDL_SCANCODE_SOFTLEFT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_SOFTRIGHT[((int)RG_KEY_SOFTRIGHT - (int)SDL_SCANCODE_SOFTRIGHT) == 0 ? 1 : -1];
typedef char rg_input_assert_key_CALL[((int)RG_KEY_CALL - (int)SDL_SCANCODE_CALL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_ENDCALL[((int)RG_KEY_ENDCALL - (int)SDL_SCANCODE_ENDCALL) == 0 ? 1 : -1];
typedef char rg_input_assert_key_RESERVED[((int)RG_KEY_RESERVED - (int)SDL_SCANCODE_RESERVED) == 0 ? 1 : -1];
typedef char rg_input_assert_mod_NONE[RG_MOD_NONE == SDL_KMOD_NONE ? 1 : -1];
typedef char rg_input_assert_mod_LSHIFT[RG_MOD_LSHIFT == SDL_KMOD_LSHIFT ? 1 : -1];
typedef char rg_input_assert_mod_RSHIFT[RG_MOD_RSHIFT == SDL_KMOD_RSHIFT ? 1 : -1];
typedef char rg_input_assert_mod_LEVEL5[RG_MOD_LEVEL5 == SDL_KMOD_LEVEL5 ? 1 : -1];
typedef char rg_input_assert_mod_LCTRL[RG_MOD_LCTRL == SDL_KMOD_LCTRL ? 1 : -1];
typedef char rg_input_assert_mod_RCTRL[RG_MOD_RCTRL == SDL_KMOD_RCTRL ? 1 : -1];
typedef char rg_input_assert_mod_LALT[RG_MOD_LALT == SDL_KMOD_LALT ? 1 : -1];
typedef char rg_input_assert_mod_RALT[RG_MOD_RALT == SDL_KMOD_RALT ? 1 : -1];
typedef char rg_input_assert_mod_LGUI[RG_MOD_LGUI == SDL_KMOD_LGUI ? 1 : -1];
typedef char rg_input_assert_mod_RGUI[RG_MOD_RGUI == SDL_KMOD_RGUI ? 1 : -1];
typedef char rg_input_assert_mod_NUM[RG_MOD_NUM == SDL_KMOD_NUM ? 1 : -1];
typedef char rg_input_assert_mod_CAPS[RG_MOD_CAPS == SDL_KMOD_CAPS ? 1 : -1];
typedef char rg_input_assert_mod_MODE[RG_MOD_MODE == SDL_KMOD_MODE ? 1 : -1];
typedef char rg_input_assert_mod_SCROLL[RG_MOD_SCROLL == SDL_KMOD_SCROLL ? 1 : -1];
typedef char rg_input_assert_mod_CTRL[RG_MOD_CTRL == SDL_KMOD_CTRL ? 1 : -1];
typedef char rg_input_assert_mod_SHIFT[RG_MOD_SHIFT == SDL_KMOD_SHIFT ? 1 : -1];
typedef char rg_input_assert_mod_ALT[RG_MOD_ALT == SDL_KMOD_ALT ? 1 : -1];
typedef char rg_input_assert_mod_GUI[RG_MOD_GUI == SDL_KMOD_GUI ? 1 : -1];
typedef char rg_input_assert_key_count[((int)RG_KEY_COUNT - (int)SDL_SCANCODE_COUNT) == 0 ? 1 : -1];
typedef char rg_input_assert_wheel_flipped[((int)RG_WHEEL_FLIPPED - (int)SDL_MOUSEWHEEL_FLIPPED) == 0 ? 1 : -1];
typedef char rg_input_assert_keymod_size[sizeof(RgMod) == sizeof(SDL_Keymod) ? 1 : -1];

// =============================================================================
// PUBLIC API (SDL side)
// =============================================================================

/**
 * @brief Sample SDL keyboard/mouse state after polling, before gameplay
 * @param input Input state
 * @note Call once per frame, including empty frames. Does not pump events or
 *       clear text, wheel, previous state, or the ordered event queue.
 */
RGINLINE void rg_input_sample(RgInputState* input);

/**
 * @brief Process an SDL event and optionally append its normalized form
 * @param input Input state
 * @param event SDL event
 * @param queue Optional ordered event queue
 */
RGINLINE void rg_input_process_event_ex(RgInputState* input,
                                        const SDL_Event* event,
                                        RgInputEventQueue* queue);

/**
 * @brief Process SDL events that affect input state
 * @param input Input state
 * @param event SDL event
 */
RGINLINE void rg_input_process_event(RgInputState* input, const SDL_Event* event);

/**
 * @brief Enable or disable relative mouse mode for a window
 * @param input Input state
 * @param window Window handle (optional, uses keyboard focus if NULL)
 * @param enabled Non-zero to enable
 */
RGINLINE void rg_input_set_relative_mouse(RgInputState* input, SDL_Window* window, int enabled);

/**
 * @brief Enable or disable text input for a window
 * @param input Input state
 * @param window Window handle (optional, uses keyboard focus if NULL)
 * @param enabled Non-zero to enable
 */
RGINLINE void rg_input_set_text_input(RgInputState* input, SDL_Window* window, int enabled);

// =============================================================================
// IMPLEMENTATION
// =============================================================================

RGINLINE void rg_input_sample(RgInputState* input)
{
	RG_INPUT_ASSERT(input != NULL);

	const bool* keyboard = SDL_GetKeyboardState(NULL);
	rg_input_copy_keys(input->current_keyboard, keyboard);

	SDL_MouseButtonFlags mouse_state;
	if (input->relative_mouse_enabled)
	{
		f32 dx = 0.0f;
		f32 dy = 0.0f;
		mouse_state = SDL_GetRelativeMouseState(&dx, &dy);
		input->mouse_delta_x = (int)dx;
		input->mouse_delta_y = (int)dy;
	}
	else
	{
		f32 mx = 0.0f;
		f32 my = 0.0f;
		mouse_state = SDL_GetMouseState(&mx, &my);
		input->mouse_delta_x = (int)mx - input->mouse_x;
		input->mouse_delta_y = (int)my - input->mouse_y;
		input->mouse_x = (int)mx;
		input->mouse_y = (int)my;
	}

	input->current_mouse[RG_MOUSE_BUTTON_LEFT] = (mouse_state & SDL_BUTTON_LMASK) != 0;
	input->current_mouse[RG_MOUSE_BUTTON_MIDDLE] = (mouse_state & SDL_BUTTON_MMASK) != 0;
	input->current_mouse[RG_MOUSE_BUTTON_RIGHT] = (mouse_state & SDL_BUTTON_RMASK) != 0;
	input->current_mouse[RG_MOUSE_BUTTON_X1] = (mouse_state & SDL_BUTTON_X1MASK) != 0;
	input->current_mouse[RG_MOUSE_BUTTON_X2] = (mouse_state & SDL_BUTTON_X2MASK) != 0;
}

RGINLINE i8 rg_input_mouse_button_from_sdl(u8 button)
{
	switch (button)
	{
		case SDL_BUTTON_LEFT: return RG_MOUSE_BUTTON_LEFT;
		case SDL_BUTTON_MIDDLE: return RG_MOUSE_BUTTON_MIDDLE;
		case SDL_BUTTON_RIGHT: return RG_MOUSE_BUTTON_RIGHT;
		case SDL_BUTTON_X1: return RG_MOUSE_BUTTON_X1;
		case SDL_BUTTON_X2: return RG_MOUSE_BUTTON_X2;
		default: return -1;
	}
}

RGINLINE void rg_input_process_event_ex(RgInputState* input,
                                        const SDL_Event* event,
                                        RgInputEventQueue* queue)
{
	RG_INPUT_ASSERT(input != NULL);
	RG_INPUT_ASSERT(event != NULL);

	if (event->type == SDL_EVENT_TEXT_INPUT)
	{
		rg_input_push_text(input, event->text.text);
		input->has_text_input = true;
	}
	else if (event->type == SDL_EVENT_MOUSE_WHEEL)
	{
		input->mouse_scroll_x += event->wheel.x;
		input->mouse_scroll_y += event->wheel.y;
	}

	if (!queue)
	{
		return;
	}

	RgInputEvent* queued = NULL;
	switch (event->type)
	{
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		{
			queue->modifiers = (RgMod)event->key.mod;
			queued = rg_input_event_queue_push(queue);
			if (queued)
			{
				queued->kind = event->type == SDL_EVENT_KEY_DOWN
				                   ? RG_INPUT_EVENT_KEY_DOWN
				                   : RG_INPUT_EVENT_KEY_UP;
				queued->timestamp_ns = event->key.timestamp;
				queued->window_id = event->key.windowID;
				queued->modifiers = (RgMod)event->key.mod;
				queued->data.key.keyboard_id = event->key.which;
				queued->data.key.scancode = (RgKey)event->key.scancode;
				queued->data.key.keycode = event->key.key;
				queued->data.key.raw = event->key.raw;
				queued->data.key.repeat = event->key.repeat;
			}
		}
		break;

		case SDL_EVENT_TEXT_INPUT:
		{
			queued = rg_input_event_queue_push_text(queue, event->text.text);
			if (queued)
			{
				queued->kind = RG_INPUT_EVENT_TEXT_INPUT;
				queued->timestamp_ns = event->text.timestamp;
				queued->window_id = event->text.windowID;
				queued->modifiers = queue->modifiers;
			}
		}
		break;

		case SDL_EVENT_TEXT_EDITING:
		{
			queued = rg_input_event_queue_push_text(queue, event->edit.text);
			if (queued)
			{
				queued->kind = RG_INPUT_EVENT_TEXT_EDITING;
				queued->timestamp_ns = event->edit.timestamp;
				queued->window_id = event->edit.windowID;
				queued->modifiers = queue->modifiers;
				queued->data.text.start = event->edit.start;
				queued->data.text.length = event->edit.length;
			}
		}
		break;

		case SDL_EVENT_MOUSE_MOTION:
		{
			queued = rg_input_event_queue_push(queue);
			if (queued)
			{
				queued->kind = RG_INPUT_EVENT_MOUSE_MOVE;
				queued->timestamp_ns = event->motion.timestamp;
				queued->window_id = event->motion.windowID;
				queued->modifiers = queue->modifiers;
				queued->data.motion.mouse_id = event->motion.which;
				queued->data.motion.buttons = event->motion.state;
				queued->data.motion.x = event->motion.x;
				queued->data.motion.y = event->motion.y;
				queued->data.motion.dx = event->motion.xrel;
				queued->data.motion.dy = event->motion.yrel;
			}
		}
		break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			queued = rg_input_event_queue_push(queue);
			if (queued)
			{
				queued->kind = event->type == SDL_EVENT_MOUSE_BUTTON_DOWN
				                   ? RG_INPUT_EVENT_MOUSE_BUTTON_DOWN
				                   : RG_INPUT_EVENT_MOUSE_BUTTON_UP;
				queued->timestamp_ns = event->button.timestamp;
				queued->window_id = event->button.windowID;
				queued->modifiers = queue->modifiers;
				queued->data.button.mouse_id = event->button.which;
				queued->data.button.button = rg_input_mouse_button_from_sdl(
				    event->button.button);
				queued->data.button.clicks = event->button.clicks;
				queued->data.button.x = event->button.x;
				queued->data.button.y = event->button.y;
			}
		}
		break;

		case SDL_EVENT_MOUSE_WHEEL:
		{
			queued = rg_input_event_queue_push(queue);
			if (queued)
			{
				queued->kind = RG_INPUT_EVENT_MOUSE_WHEEL;
				queued->timestamp_ns = event->wheel.timestamp;
				queued->window_id = event->wheel.windowID;
				queued->modifiers = queue->modifiers;
				queued->data.wheel.mouse_id = event->wheel.which;
				queued->data.wheel.x = event->wheel.x;
				queued->data.wheel.y = event->wheel.y;
				queued->data.wheel.mouse_x = event->wheel.mouse_x;
				queued->data.wheel.mouse_y = event->wheel.mouse_y;
				queued->data.wheel.direction = (RgWheelDirection)event->wheel.direction;
			}
		}
		break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		case SDL_EVENT_WINDOW_FOCUS_LOST:
		{
			queued = rg_input_event_queue_push(queue);
			if (queued)
			{
				queued->kind = event->type == SDL_EVENT_WINDOW_FOCUS_GAINED
				                   ? RG_INPUT_EVENT_WINDOW_FOCUS_GAINED
				                   : RG_INPUT_EVENT_WINDOW_FOCUS_LOST;
				queued->timestamp_ns = event->window.timestamp;
				queued->window_id = event->window.windowID;
				queued->modifiers = queue->modifiers;
			}
			if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST)
			{
				queue->modifiers = RG_MOD_NONE;
			}
		}
		break;

		default: break;
	}
}

RGINLINE void rg_input_process_event(RgInputState* input, const SDL_Event* event)
{
	rg_input_process_event_ex(input, event, NULL);
}

RGINLINE void rg_input_set_relative_mouse(RgInputState* input, SDL_Window* window, int enabled)
{
	RG_INPUT_ASSERT(input != NULL);
	input->relative_mouse_enabled = enabled != 0;
	SDL_SetWindowRelativeMouseMode(window ? window : SDL_GetKeyboardFocus(), enabled != 0);
}

RGINLINE void rg_input_set_text_input(RgInputState* input, SDL_Window* window, int enabled)
{
	RG_INPUT_ASSERT(input != NULL);
	if (enabled)
	{
		SDL_StartTextInput(window ? window : SDL_GetKeyboardFocus());
	}
	else
	{
		SDL_StopTextInput(window ? window : SDL_GetKeyboardFocus());
	}
	input->text_input_active = enabled != 0;
}

#endif // RG_INPUT_H
