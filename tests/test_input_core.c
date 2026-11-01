// rg_input_core test suite (no SDL): feeders, frame lifecycle, text append,
// wheel accumulation, ordered queue, key names.
//
// Usage:
//   test_input_core.exe

#include "../src/rg_input_core.h"

#include <stdio.h>
#include <string.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(condition, message)                          \
	do {                                                         \
		if (!(condition))                                        \
		{                                                        \
			printf("  FAIL: %s (line %d)\n", message, __LINE__); \
			g_tests_failed++;                                    \
			return;                                              \
		}                                                        \
	} while (0)

#define TEST_PASS()       \
	do {                  \
		g_tests_passed++; \
	} while (0)

static void test_key_lifecycle(void)
{
	RgInputState input;
	rg_input_init(&input);

	rg_input_begin_frame(&input);
	rg_input_set_key(&input, RG_KEY_A, true);
	TEST_ASSERT(rg_input_is_key_down(&input, RG_KEY_A), "key down after set");
	TEST_ASSERT(rg_input_is_key_pressed(&input, RG_KEY_A), "pressed on the frame it went down");
	TEST_ASSERT(!rg_input_is_key_released(&input, RG_KEY_A), "not released");

	rg_input_begin_frame(&input);
	TEST_ASSERT(rg_input_is_key_down(&input, RG_KEY_A), "still down next frame");
	TEST_ASSERT(!rg_input_is_key_pressed(&input, RG_KEY_A), "pressed is edge-triggered");

	rg_input_set_key(&input, RG_KEY_A, false);
	TEST_ASSERT(rg_input_is_key_released(&input, RG_KEY_A), "released on the frame it went up");

	rg_input_set_key(&input, RG_KEY_LCTRL, true);
	rg_input_set_key(&input, RG_KEY_Z, true);
	rg_input_clear_keys(&input);
	TEST_ASSERT(!rg_input_is_key_down(&input, RG_KEY_LCTRL) && !rg_input_is_key_down(&input, RG_KEY_Z),
	            "clear releases everything");

	rg_input_set_key(&input, (RgKey)RG_KEY_COUNT, true); // out of range is ignored, not written
	rg_input_set_key(&input, (RgKey)-1, true);
	TEST_PASS();
}

static void test_mouse_and_wheel(void)
{
	RgInputState input;
	int dx, dy, x, y;
	rg_input_init(&input);
	rg_input_begin_frame(&input);

	rg_input_set_mouse_position(&input, 10, 20);
	rg_input_set_mouse_position(&input, 15, 18);
	rg_input_get_mouse_delta(&input, &dx, &dy);
	rg_input_get_mouse_position(&input, &x, &y);
	TEST_ASSERT(x == 15 && y == 18, "position is the last one set");
	TEST_ASSERT(dx == 15 && dy == 18, "delta accumulates from the frame start");

	rg_input_add_mouse_delta(&input, -3, 2);
	rg_input_get_mouse_delta(&input, &dx, &dy);
	TEST_ASSERT(dx == 12 && dy == 20, "relative motion adds to the delta");

	rg_input_add_wheel(&input, 0.5f, 1.0f);
	rg_input_add_wheel(&input, 0.25f, -0.5f);
	TEST_ASSERT(input.mouse_scroll_x == 0.75f && input.mouse_scroll_y == 0.5f, "wheel accumulates on both axes");

	rg_input_set_mouse_button(&input, RG_MOUSE_BUTTON_RIGHT, true);
	TEST_ASSERT(rg_input_is_mouse_button_pressed(&input, RG_MOUSE_BUTTON_RIGHT), "button pressed this frame");
	TEST_ASSERT(!rg_input_is_mouse_button_down(&input, RG_MOUSE_BUTTON_COUNT), "out of range button reads 0");

	rg_input_begin_frame(&input);
	rg_input_get_mouse_delta(&input, &dx, &dy);
	TEST_ASSERT(dx == 0 && dy == 0, "begin_frame clears the delta");
	TEST_ASSERT(input.mouse_scroll_x == 0.0f && input.mouse_scroll_y == 0.0f, "begin_frame clears the wheel");
	TEST_ASSERT(rg_input_is_mouse_button_down(&input, RG_MOUSE_BUTTON_RIGHT) &&
	                !rg_input_is_mouse_button_pressed(&input, RG_MOUSE_BUTTON_RIGHT),
	            "button stays down across frames, pressed is edge-triggered");
	rg_input_set_mouse_button(&input, RG_MOUSE_BUTTON_RIGHT, false);
	TEST_ASSERT(rg_input_is_mouse_button_released(&input, RG_MOUSE_BUTTON_RIGHT), "released this frame");
	TEST_PASS();
}

static void test_text_append(void)
{
	RgInputState input;
	char big[RG_INPUT_TEXT_BUFFER_SIZE + 8];
	rg_input_init(&input);
	rg_input_begin_frame(&input);

	TEST_ASSERT(rg_input_push_text(&input, "a"), "first text stored");
	TEST_ASSERT(rg_input_push_text(&input, "b"), "second text stored");
	TEST_ASSERT(rg_input_push_text(&input, "\xC3\xA9"), "third (2-byte) text stored");
	TEST_ASSERT(input.has_text_input && strcmp(input.text_input_buffer, "ab\xC3\xA9") == 0,
	            "text events of one frame are concatenated in order");

	// Fill to within 2 bytes of the end, then push a 3-byte code point: it
	// must be dropped whole rather than split.
	{
		size_t used = strlen(input.text_input_buffer);
		size_t room = (size_t)RG_INPUT_TEXT_BUFFER_SIZE - 1u - used; // bytes available
		memset(big, 'x', sizeof(big));
		big[room - 2u] = '\0';
		TEST_ASSERT(rg_input_push_text(&input, big), "fill to two bytes short");
		TEST_ASSERT(strlen(input.text_input_buffer) == (size_t)RG_INPUT_TEXT_BUFFER_SIZE - 3u, "two bytes left");
		TEST_ASSERT(!rg_input_push_text(&input, "\xE2\x82\xAC"), "3-byte code point does not fit: dropped");
		TEST_ASSERT(strlen(input.text_input_buffer) == (size_t)RG_INPUT_TEXT_BUFFER_SIZE - 3u, "buffer unchanged");
		TEST_ASSERT(rg_input_push_text(&input, "\xC3\xA9"), "2-byte code point fits exactly");
		TEST_ASSERT(strlen(input.text_input_buffer) == (size_t)RG_INPUT_TEXT_BUFFER_SIZE - 1u, "buffer full");
		TEST_ASSERT(!rg_input_push_text(&input, "z"), "nothing fits in a full buffer");
	}
	// A long mixed string is truncated at a code point boundary.
	rg_input_begin_frame(&input);
	TEST_ASSERT(!input.has_text_input && input.text_input_buffer[0] == '\0', "begin_frame clears text");
	memset(big, 'y', sizeof(big));
	memcpy(big + RG_INPUT_TEXT_BUFFER_SIZE - 3u, "\xE2\x82\xAC", 3); // 3-byte code point straddling the limit
	big[RG_INPUT_TEXT_BUFFER_SIZE] = '\0';
	TEST_ASSERT(rg_input_push_text(&input, big), "prefix stored");
	TEST_ASSERT(strlen(input.text_input_buffer) == (size_t)RG_INPUT_TEXT_BUFFER_SIZE - 3u,
	            "truncation backs off to the last whole code point");
	TEST_PASS();
}

static void test_queue(void)
{
	RgInputEvent events[2];
	char text_buffer[8];
	RgInputEventQueue queue;
	RgInputEvent* e;

	rg_input_event_queue_init(&queue, events, 2, text_buffer, sizeof(text_buffer));
	TEST_ASSERT(queue.modifiers == RG_MOD_NONE && queue.count == 0, "fresh queue");

	rg_input_event_queue_reset(&queue, RG_MOD_LCTRL);
	TEST_ASSERT(queue.modifiers == RG_MOD_LCTRL, "reset sets initial modifiers");

	e = rg_input_event_queue_push(&queue);
	TEST_ASSERT(e != NULL && e->kind == RG_INPUT_EVENT_NONE, "pushed event is zeroed");
	e->kind = RG_INPUT_EVENT_KEY_DOWN;
	e->data.key.scancode = RG_KEY_RETURN;
	e->modifiers = RG_MOD_LSHIFT;

	e = rg_input_event_queue_push_text(&queue, "hi");
	TEST_ASSERT(e != NULL && e->data.text.text.length == 2 && strcmp(e->data.text.text.data, "hi") == 0,
	            "text copied into the queue buffer");
	TEST_ASSERT(queue.text_used == 3, "text bytes include the NUL");

	TEST_ASSERT(rg_input_event_queue_push(&queue) == NULL && queue.dropped_event_count == 1, "overflow is counted");
	TEST_ASSERT(queue.count == 2, "count unchanged by the drop");

	rg_input_event_queue_reset(&queue, RG_MOD_NONE);
	e = rg_input_event_queue_push_text(&queue, "toolongtext");
	TEST_ASSERT(e == NULL && queue.dropped_event_count == 1 && queue.dropped_text_byte_count == 12,
	            "text that does not fit drops the event atomically");
	TEST_ASSERT(queue.count == 0 && queue.text_used == 0, "nothing stored on text overflow");
	TEST_PASS();
}

static void test_names_and_values(void)
{
	// Through arrays so the checks are not constant expressions (C4127).
	const int k[] = {RG_KEY_A, RG_KEY_Z, RG_KEY_1, RG_KEY_0, RG_KEY_RETURN, RG_KEY_ESCAPE, RG_KEY_SPACE,
	                 RG_KEY_LCTRL, RG_KEY_RGUI, RG_KEY_AC_BOOKMARKS, RG_KEY_ENDCALL, RG_KEY_COUNT};
	const unsigned m[] = {RG_MOD_LSHIFT, RG_MOD_RCTRL, RG_MOD_RGUI, RG_MOD_CTRL, RG_MOD_LCTRL};
	const size_t mod_size = sizeof(RgMod);
	TEST_ASSERT(k[0] == 4 && k[1] == 29 && k[2] == 30 && k[3] == 39, "HID usage ids for letters/digits");
	TEST_ASSERT(k[4] == 40 && k[5] == 41 && k[6] == 44, "editing keys");
	TEST_ASSERT(k[7] == 224 && k[8] == 231, "modifier keys");
	TEST_ASSERT(k[9] == 286 && k[10] == 290, "media/app keys");
	TEST_ASSERT(k[11] == 512, "array size");
	TEST_ASSERT(m[0] == 0x0001u && m[1] == 0x0080u && m[2] == 0x0800u, "modifier bits");
	TEST_ASSERT((m[3] & m[4]) && (m[3] & m[1]), "combined ctrl");
	TEST_ASSERT(mod_size == 2, "RgMod is 16 bits");

	TEST_ASSERT(strcmp(rg_key_name(RG_KEY_A), "A") == 0, "letter name");
	TEST_ASSERT(strcmp(rg_key_name(RG_KEY_7), "7") == 0, "digit name");
	TEST_ASSERT(strcmp(rg_key_name(RG_KEY_RETURN), "Return") == 0, "Return");
	TEST_ASSERT(strcmp(rg_key_name(RG_KEY_LCTRL), "Left Ctrl") == 0, "Left Ctrl");
	TEST_ASSERT(strcmp(rg_key_name(RG_KEY_PAGEUP), "PageUp") == 0, "PageUp");
	TEST_ASSERT(strcmp(rg_key_name(RG_KEY_KP_ENTER), "Keypad Enter") == 0, "Keypad Enter");
	TEST_ASSERT(strcmp(rg_key_name(RG_KEY_F12), "F12") == 0, "F12");
	TEST_ASSERT(strcmp(rg_key_name(RG_KEY_UNKNOWN), "") == 0, "unknown is empty");
	TEST_ASSERT(strcmp(rg_key_name((RgKey)300), "") == 0, "unassigned value is empty");
	TEST_ASSERT(strcmp(rg_key_name((RgKey)RG_KEY_COUNT), "") == 0, "count is not a key");

	// Every listed key has a distinct value below RG_KEY_COUNT.
	{
		static const int values[] = {
#define X(name, value, label) value,
			RG_KEY_LIST(X)
#undef X
		};
		bool seen[RG_KEY_COUNT];
		int duplicates = 0;
		size_t i;
		memset(seen, 0, sizeof(seen));
		for (i = 0; i < sizeof(values) / sizeof(values[0]); i++)
		{
			if (values[i] < 0 || values[i] >= RG_KEY_COUNT || seen[values[i]]) duplicates++;
			else seen[values[i]] = true;
		}
		const size_t named = sizeof(values) / sizeof(values[0]);
		TEST_ASSERT(duplicates == 0, "key values are unique and in range");
		TEST_ASSERT(named == 248, "248 named keys");
	}
	TEST_PASS();
}

int main(void)
{
	printf("rg_input_core tests\n");
	test_key_lifecycle();
	test_mouse_and_wheel();
	test_text_append();
	test_queue();
	test_names_and_values();
	printf("%d passed, %d failed\n", g_tests_passed, g_tests_failed);
	return g_tests_failed ? 1 : 0;
}
