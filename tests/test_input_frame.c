// Deterministic frame lifecycle tests: the fake poller updates device state.
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static bool keyboard_state[SDL_SCANCODE_COUNT];
static SDL_MouseButtonFlags mouse_buttons;
static float mouse_x, mouse_y, relative_x, relative_y;
static int keyboard_reads, mouse_reads, relative_reads, pumps;
static SDL_Event pending[8];
static size_t pending_count, pending_index;
static bool pending_key;

static const bool* test_keyboard(int* count)
{
	keyboard_reads++;
	if (count) *count = SDL_SCANCODE_COUNT;
	return keyboard_state;
}

static SDL_MouseButtonFlags test_mouse(float* x, float* y)
{
	mouse_reads++;
	if (x) *x = mouse_x;
	if (y) *y = mouse_y;
	return mouse_buttons;
}

static SDL_MouseButtonFlags test_relative(float* x, float* y)
{
	relative_reads++;
	if (x) *x = relative_x;
	if (y) *y = relative_y;
	relative_x = relative_y = 0.0f;
	return mouse_buttons;
}

static void test_pump(void)
{
	pumps++;
	keyboard_state[SDL_SCANCODE_A] = pending_key;
}

static bool test_poll(SDL_Event* event)
{
	if (pending_index == 0)
	{
		test_pump();
	}
	if (pending_index == pending_count)
	{
		pending_index++;
		return false;
	}
	*event = pending[pending_index++];
	return true;
}

#define SDL_GetKeyboardState test_keyboard
#define SDL_GetMouseState test_mouse
#define SDL_GetRelativeMouseState test_relative
#define SDL_PumpEvents test_pump
#define SDL_PollEvent test_poll
#include "../src/rg_input.h"

static int checks, failures;
#define CHECK(condition) do { checks++; if (!(condition)) { \
	printf("FAIL line %d: %s\n", __LINE__, #condition); failures++; } } while (0)

static void poll_frame(RgInputState* input, RgInputEventQueue* queue)
{
	pending_index = 0;
	SDL_Event event;
	while (test_poll(&event)) rg_input_process_event_ex(input, &event, queue);
}

static void test_frames(void)
{
	RgInputState input;
	rg_input_init(&input);
	pending_count = 2;
	memset(pending, 0, sizeof(pending));
	pending[0].type = SDL_EVENT_TEXT_INPUT;
	pending[0].text.text = "frame text";
	pending[1].type = SDL_EVENT_MOUSE_WHEEL;
	pending[1].wheel.y = 2.5f;
	pending_key = true;
	mouse_buttons = SDL_BUTTON_LMASK;
	mouse_x = 12.0f;
	mouse_y = -4.0f;
	rg_input_begin_frame(&input);
	CHECK(pumps == 0 && keyboard_reads == 0 && mouse_reads == 0);
	CHECK(!rg_input_is_key_down(&input, SDL_SCANCODE_A));
	poll_frame(&input, NULL);
	rg_input_sample(&input);
	CHECK(pumps == 1 && keyboard_reads == 1 && mouse_reads == 1 && relative_reads == 0);
	CHECK(rg_input_is_key_pressed(&input, SDL_SCANCODE_A));
	CHECK(rg_input_is_mouse_button_pressed(&input, RG_MOUSE_BUTTON_LEFT));
	CHECK(input.mouse_x == 12 && input.mouse_y == -4);
	CHECK(input.mouse_delta_x == 12 && input.mouse_delta_y == -4);
	CHECK(input.has_text_input && strcmp(input.text_input_buffer, "frame text") == 0);
	CHECK(input.mouse_scroll_y == 2.5f);

	pending_count = 0;
	rg_input_begin_frame(&input);
	CHECK(!input.has_text_input && input.text_input_buffer[0] == '\0');
	CHECK(input.mouse_scroll_y == 0.0f);
	poll_frame(&input, NULL);
	rg_input_sample(&input);
	CHECK(rg_input_is_key_down(&input, SDL_SCANCODE_A));
	CHECK(!rg_input_is_key_pressed(&input, SDL_SCANCODE_A));
	CHECK(!rg_input_is_mouse_button_pressed(&input, RG_MOUSE_BUTTON_LEFT));
	CHECK(input.mouse_delta_x == 0 && input.mouse_delta_y == 0);

	pending_key = false;
	mouse_buttons = 0;
	rg_input_begin_frame(&input);
	poll_frame(&input, NULL);
	rg_input_sample(&input);
	CHECK(rg_input_is_key_released(&input, SDL_SCANCODE_A));
	CHECK(rg_input_is_mouse_button_released(&input, RG_MOUSE_BUTTON_LEFT));

	input.relative_mouse_enabled = 1;
	relative_x = 5.0f;
	relative_y = -3.0f;
	mouse_buttons = SDL_BUTTON_RMASK;
	rg_input_begin_frame(&input);
	poll_frame(&input, NULL);
	rg_input_sample(&input);
	CHECK(input.mouse_delta_x == 5 && input.mouse_delta_y == -3);
	CHECK(input.mouse_x == 12 && input.mouse_y == -4);
	CHECK(rg_input_is_mouse_button_pressed(&input, RG_MOUSE_BUTTON_RIGHT));
	CHECK(relative_reads == 1 && mouse_reads == 3);
	rg_input_begin_frame(&input);
	poll_frame(&input, NULL);
	rg_input_sample(&input);
	CHECK(input.mouse_delta_x == 0 && input.mouse_delta_y == 0);
	CHECK(pumps == 5 && keyboard_reads == 5 && mouse_reads + relative_reads == 5);
}

static void test_frame_modifiers(void)
{
	RgInputState input;
	RgInputEvent storage[8];
	char text[64];
	RgInputEventQueue queue;
	rg_input_init(&input);
	rg_input_event_queue_init(&queue, storage, 8, text, sizeof(text));
	rg_input_event_queue_reset(&queue, SDL_KMOD_NONE);
	memset(pending, 0, sizeof(pending));
	pending_count = 5;
	pending[0].type = SDL_EVENT_MOUSE_MOTION;
	pending[1].type = SDL_EVENT_KEY_DOWN;
	pending[1].key.mod = SDL_KMOD_LSHIFT;
	pending[2].type = SDL_EVENT_TEXT_INPUT;
	pending[2].text.text = "A";
	pending[3].type = SDL_EVENT_KEY_UP;
	pending[3].key.mod = SDL_KMOD_NONE;
	pending[4].type = SDL_EVENT_TEXT_INPUT;
	pending[4].text.text = "a";
	rg_input_begin_frame(&input);
	poll_frame(&input, &queue);
	rg_input_sample(&input);
	CHECK(queue.count == 5);
	CHECK(storage[0].modifiers == SDL_KMOD_NONE);
	CHECK(storage[2].modifiers == SDL_KMOD_LSHIFT);
	CHECK(storage[4].modifiers == SDL_KMOD_NONE);
	CHECK(queue.modifiers == SDL_KMOD_NONE);
	CHECK(input.has_text_input && strcmp(input.text_input_buffer, "a") == 0);

	// Overflow must still carry the final modifier state to the following frame.
	queue.capacity = 1;
	rg_input_event_queue_reset(&queue, queue.modifiers);
	pending_count = 2;
	pending[0].type = SDL_EVENT_MOUSE_MOTION;
	pending[1].type = SDL_EVENT_KEY_DOWN;
	pending[1].key.mod = SDL_KMOD_LCTRL;
	poll_frame(&input, &queue);
	CHECK(queue.dropped_event_count == 1 && queue.modifiers == SDL_KMOD_LCTRL);
	rg_input_event_queue_reset(&queue, queue.modifiers);
	pending_count = 1;
	poll_frame(&input, &queue);
	CHECK(storage[0].modifiers == SDL_KMOD_LCTRL);
	pending[0].type = SDL_EVENT_WINDOW_FOCUS_LOST;
	poll_frame(&input, &queue);
	CHECK(queue.modifiers == SDL_KMOD_NONE);
}

int main(void)
{
	test_frames();
	test_frame_modifiers();
	printf("rg_input frame: %d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
