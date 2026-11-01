# rg_input

`rg_input.h` maintains immediate keyboard, mouse, wheel, and text-input state
from SDL3 events. It can also normalize those events into a caller-owned,
ordered queue for systems that need exact event order or IME data.

## Immediate state

Initialize one `RgInputState`. Begin the frame, pass every polled SDL event to
`rg_input_process_event`, then sample SDL before consuming gameplay input:

```c
#include "rg_input.h"

RgInputState input;
rg_input_init(&input);

while (running)
{
	rg_input_begin_frame(&input);

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		rg_input_process_event(&input, &event);
	}

	rg_input_sample(&input);
	if (rg_input_is_key_pressed(&input, SDL_SCANCODE_ESCAPE))
	{
		running = 0;
	}
}
```

`rg_input_begin_frame` snapshots the previous key and mouse-button state and
clears text and wheel fields. Polling lets SDL pump this frame's events.
`rg_input_sample` then reads current keyboard/buttons and mouse movement;
it preserves the text and wheel data collected during polling. Neither helper
pumps events. Call each once per frame, even when there are no events, on SDL's
main thread. Query immediate input after sampling.

Immediate state is a snapshot: a key pressed and released within one frame
may appear released throughout. Use the ordered queue to retain both edges.

Immediate queries are grouped by device:

| Input | Functions and fields |
| --- | --- |
| Keyboard | `rg_input_is_key_down`, `rg_input_is_key_pressed`, `rg_input_is_key_released` |
| Mouse buttons | `rg_input_is_mouse_button_down`, `rg_input_is_mouse_button_pressed`, `rg_input_is_mouse_button_released` |
| Mouse motion | `rg_input_get_mouse_position`, `rg_input_get_mouse_delta` |
| Mouse wheel | `input.mouse_scroll_y` |
| Text | `input.has_text_input`, `input.text_input_buffer` |

The five zero-based `RG_MOUSE_BUTTON_*` constants map SDL's left, middle,
right, X1, and X2 buttons. Invalid mouse-button indices return zero.

## Ordered event queue

Use `RgInputEventQueue` when order, timestamps, device/window IDs, modifiers,
raw floating-point mouse coordinates, or IME editing data matter. The queue
never allocates: the caller supplies event and UTF-8 text storage.

```c
RgInputState input;
RgInputEvent events[256];
char event_text[4096];
RgInputEventQueue queue;

rg_input_init(&input);
rg_input_event_queue_init(&queue,
                          events, RG_ARRAY_COUNT(events),
                          event_text, sizeof(event_text));
// Seed before the first event stream is pumped; carry the final state thereafter.
rg_input_event_queue_reset(&queue, SDL_GetModState());

while (running)
{
	rg_input_begin_frame(&input);
	rg_input_event_queue_reset(&queue, queue.modifiers);

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		rg_input_process_event_ex(&input, &event, &queue);
		if (event.type == SDL_EVENT_QUIT)
			running = 0;
	}

	rg_input_sample(&input);
	for (size_t i = 0; i < queue.count; ++i)
	{
		const RgInputEvent* queued = &queue.events[i];
		if (queued->kind == RG_INPUT_EVENT_KEY_DOWN)
		{
			// Handle key presses in event order.
		}
	}
}
```

Reset the queue once per frame before processing events. The initial modifier
argument describes state at the beginning of that event stream; key events
then update the queue's modifier snapshot in order.

The queue normalizes key up/down, text input and editing, mouse motion,
mouse-button up/down, wheel, and window focus events. Unsupported SDL events
are ignored. Text strings are copied into `text_buffer`; their views remain
valid until that storage is reset or reused.

If event storage is full, `dropped_event_count` increases. If a text event
cannot fit, both `dropped_event_count` and `dropped_text_byte_count` increase
and the event is omitted. Inspect these counters before resetting the queue
when overflow must be visible.

## Relative mouse and text input

```c
rg_input_set_relative_mouse(&input, window, 1);
rg_input_set_text_input(&input, window, 1);
```

Pass a window explicitly or null to use SDL's keyboard-focus window. Relative
mode changes `rg_input_sample` to read accumulated relative deltas. Text-input
mode controls SDL's text-input session; `text_input_buffer` holds the latest
text event truncated to `RG_INPUT_TEXT_BUFFER_SIZE`, while the ordered queue can
retain multiple text and editing events up to caller-provided capacity.

## Configuration and lifetime

Define options before including the header:

```c
#define RG_INPUT_TEXT_BUFFER_SIZE 64
#define RG_INPUT_ASSERT(condition) my_assert(condition)
#define RG_INPUT_NO_SIMD // copy key state without SSE2/NEON
#include "rg_input.h"
```

`rg_input_begin_frame` and `rg_input_sample` each copy the 512-entry key-state
array. SDL's array is only 2-byte aligned and compilers lower a `memcpy` of it
to a misaligned `rep movs`, so the header copies with an explicit 16-byte loop
using SSE2 on x86 (always available on x86-64) or NEON on AArch64, and an 8-byte
loop otherwise. `RG_INPUT_NO_SIMD` selects the portable loop everywhere.

`RgInputState`, queue storage, and queued text are caller-owned. Keep them alive
while processing and consuming a frame. The header has normal include guards
and all functions have internal linkage. A shared input state or queue requires
caller synchronization, and SDL's event and window thread requirements still
apply.

## Performance

On Linux x86-64 (Intel Core i3-1005G1, GCC 13.3, SDL3, `./build.sh bench_input`),
`rg_input_begin_frame` plus `rg_input_sample` with real SDL getters costs about
42 ns per frame with 1,024 tracked states and 28 ns with a single state, median
of seven samples and three process runs on one pinned core. The keyboard
snapshot copies use 16-byte SSE2 moves (NEON on AArch64); `RG_INPUT_NO_SIMD`
selects a portable loop that costs a few nanoseconds more.

## Backend-neutral core (`rg_input_core.h`)

`rg_input.h` is an SDL3 adapter over `rg_input_core.h`, which has no SDL or
platform dependency. The core owns `RgInputState`, `RgInputEventQueue` and
every query; it names keys with `RgKey` (`RG_KEY_*`, USB HID usage ids equal
to SDL3 scancodes) and modifiers with `RgMod` (`RG_MOD_*`, equal to
`SDL_KMOD_*`). `rg_input.h` static-asserts every value against SDL, so SDL
names and RG names are interchangeable in code that includes the adapter.

A windowing backend that is not SDL (a Win32 message loop, for example) feeds
the state directly:

```c
#include "rg_input_core.h"

rg_input_begin_frame(&input);                 // once per frame, before feeding
rg_input_set_key(&input, RG_KEY_A, true);     // WM_KEYDOWN
rg_input_set_mouse_position(&input, x, y);    // WM_MOUSEMOVE; delta accumulates
rg_input_add_wheel(&input, dx, dy);           // WM_MOUSEHWHEEL / WM_MOUSEWHEEL
rg_input_push_text(&input, utf8);             // WM_CHAR; appends, whole code points only
rg_input_clear_keys(&input);                  // WM_KILLFOCUS
```

Ordered events are produced with `rg_input_event_queue_push` and
`rg_input_event_queue_push_text`, filling the same `RgInputEvent` fields the
SDL adapter fills. `rg_key_name` returns a label for shortcut hints
("Left Ctrl", "PageUp").

Differences from the SDL-only header this replaces: `text_input_buffer` holds
every text event of the frame concatenated (the old behavior kept only the
last one), `mouse_scroll_x` accumulates horizontal wheel motion, and
`rg_input_begin_frame` clears the mouse delta (the SDL sampler overwrites it
anyway).
