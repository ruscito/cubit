// input.c

#include "input.h"
#include "cubit_types.h"
#include "backend.h"

#include <string.h>
#include <stdio.h>

struct key_state {
	char previous_state[INPUT_MAX_KEY];
	char current_state[INPUT_MAX_KEY];
};

struct mouse_state {
	char previous_state[MOUSE_BUTTON_TOTAL];
	char current_state[MOUSE_BUTTON_TOTAL];
	double x_position;
	double y_position;
	double x_scroll_offset;
	double y_scroll_offset;
	int32_t inside;
};

static struct key_state keyboard = {0};
static struct mouse_state mouse = {0};
static process_keyboard_func keyboard_callback = NULL;
static process_mouse_position_func mouse_position_callback = NULL;
static process_mouse_button_func mouse_button_callback = NULL;
static process_mouse_scroll_func mouse_scroll_callback = NULL;
static process_mouse_enter_func mouse_enter_callback = NULL;


/*******************************************************************************
 * 	INPUT UPDATE - Call once per frame before polling events
 *******************************************************************************/
void input_update(void) {
	memcpy(keyboard.previous_state, keyboard.current_state, INPUT_MAX_KEY);
	memcpy(mouse.previous_state, mouse.current_state, MOUSE_BUTTON_TOTAL);
	mouse.x_scroll_offset = 0.0;
	mouse.y_scroll_offset = 0.0;
}


/*******************************************************************************
 * 	KEYBOARD
 *******************************************************************************/
int32_t is_key_pressed(InputKeyboardKey key) {
	// Returns true the frame a key goes from not-pressed to pressed
	if (key > 0 && key < INPUT_MAX_KEY) {
		return (keyboard.current_state[key] == KEY_PRESS)
			&& (keyboard.previous_state[key] == KEY_RELEASE);
	}
	return 0;
}

int32_t is_key_released(InputKeyboardKey key) {
	// Returns true the frame a key goes from pressed to released
	if (key > 0 && key < INPUT_MAX_KEY) {
		return (keyboard.current_state[key] == KEY_RELEASE)
			&& (keyboard.previous_state[key] == KEY_PRESS || keyboard.previous_state[key] == KEY_REPEAT);
	}
	return 0;
}

int32_t is_key_down(InputKeyboardKey key) {
	// Returns true while key is held
	if (key > 0 && key < INPUT_MAX_KEY) {
		return keyboard.current_state[key] == KEY_PRESS ||
			   keyboard.current_state[key] == KEY_REPEAT;
	}
	return 0;
}

void register_key_callback(process_keyboard_func callback) {
	keyboard_callback = callback;
}

void input_process_keyboard(int32_t key, int32_t scancode, int32_t action, int32_t mods) {
	if (key > 0 && key < INPUT_MAX_KEY) {
		keyboard.current_state[key] = action;
	}
#ifdef INPUT_DEBUG
	printf("keyboard key=%d, action=%d\n", key, action);
#endif
	if (keyboard_callback) keyboard_callback(key, scancode, action, mods);
}


/*******************************************************************************
 * 	MOUSE POSITION
 *******************************************************************************/
void register_mouse_position_callback(process_mouse_position_func callback) {
	mouse_position_callback = callback;
}

void input_process_mouse_position(double x_pos, double y_pos) {
	mouse.x_position = x_pos;
	mouse.y_position = y_pos;
	if (mouse_position_callback) mouse_position_callback(x_pos, y_pos);
}

void get_mouse_position(double *x, double *y) {
	*x = mouse.x_position;
	*y = mouse.y_position;
}


/*******************************************************************************
 * 	MOUSE BUTTONS
 *******************************************************************************/
void register_mouse_button_callback(process_mouse_button_func callback) {
	mouse_button_callback = callback;
}

void input_process_mouse_button(int32_t button, int32_t action, int32_t mods) {
	if (button >= 0 && button < MOUSE_BUTTON_TOTAL) {
		mouse.current_state[button] = action;
	}
#ifdef INPUT_DEBUG
	printf("mouse button=%d, action=%d\n", button, action);
#endif
	if (mouse_button_callback) mouse_button_callback(button, action, mods);
}

int32_t is_mouse_button_pressed(InputMouseButton button) {
	// Returns true the frame a button goes from not-pressed to pressed
	if (button >= 0 && button < MOUSE_BUTTON_TOTAL) {
		return (mouse.current_state[button] == BTN_PRESS)
			&& (mouse.previous_state[button] == BTN_RELEASE);
	}
	return 0;
}

int32_t is_mouse_button_released(InputMouseButton button) {
	// Returns true the frame a button goes from pressed to released
	if (button >= 0 && button < MOUSE_BUTTON_TOTAL) {
		return (mouse.current_state[button] == BTN_RELEASE)
			&& (mouse.previous_state[button] == BTN_PRESS);
	}
	return 0;
}

int32_t is_mouse_button_down(InputMouseButton button) {
	// Returns true while button is held
	if (button >= 0 && button < MOUSE_BUTTON_TOTAL) {
		return mouse.current_state[button] == BTN_PRESS;
	}
	return 0;
}


/*******************************************************************************
 * 	MOUSE SCROLL
 *******************************************************************************/
void register_mouse_scroll_callback(process_mouse_scroll_func callback) {
	mouse_scroll_callback = callback;
}

void input_process_mouse_scroll(double x_offset, double y_offset) {
	mouse.x_scroll_offset = x_offset;
	mouse.y_scroll_offset = y_offset;
	if (mouse_scroll_callback) mouse_scroll_callback(x_offset, y_offset);
}

void get_mouse_scroll(double *x_offset, double *y_offset) {
	*x_offset = mouse.x_scroll_offset;
	*y_offset = mouse.y_scroll_offset;
}


/*******************************************************************************
 * 	MOUSE ENTER/LEAVE
 *******************************************************************************/
void register_mouse_enter_callback(process_mouse_enter_func callback) {
	mouse_enter_callback = callback;
}

void input_process_mouse_enter(int32_t entered) {
	mouse.inside = entered;
	if (mouse_enter_callback) mouse_enter_callback(entered);
}

int32_t is_mouse_inside(void) {
	return mouse.inside;
}


/*******************************************************************************
 * 	MOUSE MODE
 *******************************************************************************/
void set_mouse_mode(int32_t mode) {
	backend_set_mouse_mode(mode);
}
