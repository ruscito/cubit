// input.h

#ifndef INPUT_H_
#define INPUT_H_

#define INPUT_MAX_KEY 	512
#define KEY_RELEASE		0
#define KEY_PRESS		1
#define KEY_REPEAT		2
#define KEY_UNKNOWN	   -1

#define BTN_RELEASE		0
#define BTN_PRESS		1
#define BTN_UNKNOWN	   -1


#define MOUSE_NORMAL 	0
#define MOUSE_RAW		1


typedef enum {
    KEY_NULL            = 0,        // NULL no key
    KEY_APOSTROPHE      = 39,       // '
    KEY_SPACE           = 32,       // Space
    KEY_COMMA           = 44,       // ,
    KEY_MINUS           = 45,       // -
    KEY_PERIOD          = 46,       // .
    KEY_SLASH           = 47,       // /
    KEY_0	            = 48,       // 0
    KEY_1             	= 49,       // 1
    KEY_2               = 50,       // 2
    KEY_3   	        = 51,       // 3
    KEY_4	            = 52,       // 4
    KEY_5            	= 53,       // 5
    KEY_6             	= 54,       // 6
    KEY_7           	= 55,       // 7
    KEY_8           	= 56,       // 8
    KEY_9            	= 57,       // 9
    KEY_SEMICOLON  		= 59,       // ;
    KEY_EQUAL           = 61,       // =
    KEY_A               = 65,       // A/a
    KEY_B               = 66,       // B/b
    KEY_C               = 67,       // C/c
    KEY_D               = 68,       // D/d
    KEY_E               = 69,       // E/e
    KEY_F               = 70,       // F/f
    KEY_G               = 71,       // G/g
    KEY_H               = 72,       // H/h
    KEY_I               = 73,       // I/i
    KEY_J               = 74,       // J/j
    KEY_K               = 75,       // K/k
    KEY_L               = 76,       // L/l
    KEY_M               = 77,       // M/m
    KEY_N               = 78,       // N/n
    KEY_O               = 79,       // O/o
    KEY_P               = 80,       // P/p
    KEY_Q               = 81,       // Q/q
    KEY_R               = 82,       // R/r
    KEY_S               = 83,       // S/s
    KEY_T               = 84,       // T/t
    KEY_U               = 85,       // U/u
    KEY_V               = 86,       // V/v
    KEY_W               = 87,       // W/w
    KEY_X               = 88,       // X/x
    KEY_Y               = 89,       // Y/y
    KEY_Z               = 90,       // Z/z
    KEY_LEFT_BRACKET    = 91,       // [
    KEY_BACKSLASH       = 92,       // \ backslash
    KEY_RIGHT_BRACKET   = 93,       // ]
    KEY_GRAVE           = 96,       // `
    KEY_ESC             = 256,      // Esc
    KEY_ENTER           = 257,      // Enter
    KEY_TAB             = 258,      // Tab
    KEY_BACKSPACE       = 259,      // Backspace
    KEY_INSERT          = 260,      // Ins
    KEY_DELETE          = 261,      // Del
    KEY_RIGHT           = 262,      // Cursor right
    KEY_LEFT            = 263,      // Cursor left
    KEY_DOWN            = 264,      // Cursor down
    KEY_UP              = 265,      // Cursor up
    KEY_PAGE_UP         = 266,      // Page up
    KEY_PAGE_DOWN       = 267,      // Page down
    KEY_HOME            = 268,      // Home
    KEY_END             = 269,      // End
    KEY_CAPS_LOCK       = 280,      // Caps lock
    KEY_SCROLL_LOCK     = 281,      // Scroll down
    KEY_NUM_LOCK        = 282,      // Num lock
    KEY_PRINT_SCREEN    = 283,      // Print screen
    KEY_PAUSE           = 284,      // Pause
    KEY_F1              = 290,      // F1
    KEY_F2              = 291,      // F2
    KEY_F3              = 292,      // F3
    KEY_F4              = 293,      // F4
    KEY_F5              = 294,      // F5
    KEY_F6              = 295,      // F6
    KEY_F7              = 296,      // F7
    KEY_F8              = 297,      // F8
    KEY_F9              = 298,      // F9
    KEY_F10             = 299,      // F10
    KEY_F11             = 300,      // F11
    KEY_F12             = 301,      // F12
    KEY_LEFT_SHIFT      = 340,      // Shift left
    KEY_LEFT_CONTROL    = 341,      // Control left
    KEY_LEFT_ALT        = 342,      // Alt left
    KEY_LEFT_SUPER      = 343,      // Super left
    KEY_RIGHT_SHIFT     = 344,      // Shift right
    KEY_RIGHT_CONTROL   = 345,      // Control right
    KEY_RIGHT_ALT       = 346,      // Alt right
    KEY_RIGHT_SUPER     = 347,      // Super right
    KEY_KB_MENU         = 348,      // KB menu
    KEY_KP_0            = 320,      // Keypad 0
    KEY_KP_1            = 321,      // Keypad 1
    KEY_KP_2            = 322,      // Keypad 2
    KEY_KP_3            = 323,      // Keypad 3
    KEY_KP_4            = 324,      // Keypad 4
    KEY_KP_5            = 325,      // Keypad 5
    KEY_KP_6            = 326,      // Keypad 6
    KEY_KP_7            = 327,      // Keypad 7
    KEY_KP_8            = 328,      // Keypad 8
    KEY_KP_9            = 329,      // Keypad 9
    KEY_KP_DECIMAL      = 330,      // Keypad .
    KEY_KP_DIVIDE       = 331,      // Keypad /
    KEY_KP_MULTIPLY     = 332,      // Keypad *
    KEY_KP_SUBTRACT     = 333,      // Keypad -
    KEY_KP_ADD          = 334,      // Keypad +
    KEY_KP_ENTER        = 335,      // Keypad Enter
    KEY_KP_EQUAL        = 336,      // Keypad =
} InputKeyboardKey;

typedef enum {
    MOUSE_BUTTON_LEFT   		= 0,        // Button left
    MOUSE_BUTTON_RIGHT  		= 1,        // Button right
    MOUSE_BUTTON_MIDDLE 		= 2,        // Button middle
    MOUSE_BUTTON_SIDE   		= 3,        // Button side
    MOUSE_BUTTON_EXTRA  		= 4,        // Button extra
    MOUSE_BUTTON_FORWARD		= 5,        // Button forward
    MOUSE_BUTTON_BACK   		= 6,        // Button back
    MOUSE_BUTTON_TOTAL			= 7,
} InputMouseButton;



#endif // INPUT_H_
