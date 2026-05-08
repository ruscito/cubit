// game2d.c
//
// Example application showcasing the Cubit Engine's 2D pipeline (Stage 22).
//
// What this demonstrates:
//   - object2d_t (retained sprites): the player and projectiles
//   - sprite_submit() (immediate sprites): muzzle flash effect
//   - text_draw():                       HUD score and FPS
//   - ui_rect_filled / bordered:         HP bar, button background
//   - ui_progress_bar:                   HP fill animation
//   - ui_rect_contains_mouse:            interactive button
//   - camera2d_screen_to_virtual:        cursor position in UI space
//   - layers:                            background < entities < UI < text
//
// Game logic is intentionally minimal — this is an architecture example,
// not a finished game. Movement is keyboard-driven, projectiles are spawned
// on space press, the "enemy" is a static red rect that blinks when the
// mouse hovers over it.

#include "cubit.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


// ---- Configuration ----

#define VIRTUAL_W  1920.0f
#define VIRTUAL_H  1080.0f

#define PLAYER_SIZE      80.0f
#define PLAYER_SPEED     500.0f      // virtual units per second
#define PROJECTILE_SIZE  16.0f
#define PROJECTILE_SPEED 900.0f
#define MAX_PROJECTILES  64
#define MUZZLE_FLASH_TIME 0.08f      // seconds


// ---- Layer constants ----
//
// Higher = drawn on top. Spread out so we have room to insert in-between
// layers later without renumbering.

#define LAYER_BG          0
#define LAYER_ENEMY      10
#define LAYER_PROJECTILE 20
#define LAYER_PLAYER     30
#define LAYER_FX         40
#define LAYER_UI_BG      50
#define LAYER_UI_FG      60


// ---- Game state ----

typedef struct {
	object2d_t* obj;
	bool        active;
} projectile_t;


static struct {
	object2d_t*  player;
	object2d_t*  enemy_static;       // a sprite-style "target" (red rect)

	projectile_t projectiles[MAX_PROJECTILES];

	// Muzzle flash: a one-frame sprite triggered after a shot.
	// We store the world position and a countdown timer; while >0 we draw
	// it as an immediate sprite each frame.
	vec2  flash_pos;
	float flash_timer;

	// Game stats
	int   score;
	float player_hp;
	float player_hp_max;

	// "Reset" button hover state (for visual feedback)
	bool reset_button_hovered;

	// Edge-detection state (the engine exposes "is_pressed" but not
	// "is_just_pressed", so we track previous-frame state ourselves to
	// detect rising edges for shoot/click).
	bool prev_space_down;
	bool prev_mouse_left_down;
} g;


// Forward declaration — defined below.
static void draw_hud(camera2d_t* ui_cam);


// ---- Helpers ----

// Spawn a projectile from a free slot in the pool. Returns NULL if the pool
// is full — the game silently drops the shot.
static projectile_t* spawn_projectile(vec2 origin, vec2 velocity) {
	for (int i = 0; i < MAX_PROJECTILES; i++) {
		if (!g.projectiles[i].active) {
			object2d_t* o = object2d_new();
			if (!o) return NULL;

			object2d_set_position(o, origin);
			object2d_set_size(o, (vec2){PROJECTILE_SIZE, PROJECTILE_SIZE});
			object2d_set_color(o, COLOR_YELLOW);
			object2d_set_layer(o, LAYER_PROJECTILE);

			g.projectiles[i].obj = o;
			g.projectiles[i].active = true;
			// Stash velocity in the unused rotation field?  Better: keep it
			// in a parallel array. For brevity we skip per-projectile velocity
			// tracking — they all move straight up at PROJECTILE_SPEED.
			(void)velocity;
			return &g.projectiles[i];
		}
	}
	return NULL;
}


static void destroy_projectile(projectile_t* p) {
	if (!p->active) return;
	object2d_destroy(p->obj);
	p->obj = NULL;
	p->active = false;
}


// AABB overlap on two rects in the same coordinate space.
// We don't have a 2D collision system yet (deferred to a future stage), so
// we hand-roll the check here using object2d_get_position/size.
static bool rects_overlap(object2d_t* a, object2d_t* b) {
	vec2 ap = object2d_get_position(a);
	vec2 as = object2d_get_size(a);
	vec2 bp = object2d_get_position(b);
	vec2 bs = object2d_get_size(b);

	// position is the center of the sprite (Stage 22 convention)
	float a_left   = ap.x - as.x * 0.5f;
	float a_right  = ap.x + as.x * 0.5f;
	float a_top    = ap.y - as.y * 0.5f;
	float a_bottom = ap.y + as.y * 0.5f;

	float b_left   = bp.x - bs.x * 0.5f;
	float b_right  = bp.x + bs.x * 0.5f;
	float b_top    = bp.y - bs.y * 0.5f;
	float b_bottom = bp.y + bs.y * 0.5f;

	return a_left < b_right && a_right > b_left
	    && a_top < b_bottom && a_bottom > b_top;
}


// ---- Engine callbacks ----

void application_config(app_config_t* cfg) {
	cfg->width  = 1280;
	cfg->height = 720;
	cfg->title  = "Cubit 2D Example";
	cfg->fps    = 60;
	cfg->ui_virtual_resolution = (vec2){VIRTUAL_W, VIRTUAL_H};
}


void application_init(void) {
	memset(&g, 0, sizeof(g));

	// Player — a green square in the lower-middle of the screen
	g.player = object2d_new();
	object2d_set_position(g.player, (vec2){VIRTUAL_W * 0.5f, VIRTUAL_H - 200.0f});
	object2d_set_size(g.player, (vec2){PLAYER_SIZE, PLAYER_SIZE});
	object2d_set_color(g.player, CUBIT_GREEN);
	object2d_set_layer(g.player, LAYER_PLAYER);

	// Enemy — a static red square the player can shoot at
	g.enemy_static = object2d_new();
	object2d_set_position(g.enemy_static, (vec2){VIRTUAL_W * 0.5f, 200.0f});
	object2d_set_size(g.enemy_static, (vec2){120.0f, 120.0f});
	object2d_set_color(g.enemy_static, CUBIT_RED);
	object2d_set_layer(g.enemy_static, LAYER_ENEMY);

	// Stats
	g.score         = 0;
	g.player_hp     = 100.0f;
	g.player_hp_max = 100.0f;
	g.flash_timer   = 0.0f;
}


void application_fixed_update(double dt) {
	// No fixed-step logic in this example. Movement and shooting run from
	// application_update for simplicity.
	(void)dt;
}


void application_update(double dt) {
	float dts = (float)dt;

	// ---- Player movement ----
	vec2 ppos = object2d_get_position(g.player);

	if (is_key_pressed(KEY_A) || is_key_pressed(KEY_LEFT))  ppos.x -= PLAYER_SPEED * dts;
	if (is_key_pressed(KEY_D) || is_key_pressed(KEY_RIGHT)) ppos.x += PLAYER_SPEED * dts;
	if (is_key_pressed(KEY_W) || is_key_pressed(KEY_UP))    ppos.y -= PLAYER_SPEED * dts;
	if (is_key_pressed(KEY_S) || is_key_pressed(KEY_DOWN))  ppos.y += PLAYER_SPEED * dts;

	// Clamp to screen
	float half = PLAYER_SIZE * 0.5f;
	if (ppos.x < half)             ppos.x = half;
	if (ppos.x > VIRTUAL_W - half) ppos.x = VIRTUAL_W - half;
	if (ppos.y < half)             ppos.y = half;
	if (ppos.y > VIRTUAL_H - half) ppos.y = VIRTUAL_H - half;

	object2d_set_position(g.player, ppos);

	// ---- Shooting (rising-edge detection on Space) ----
	bool space_down = is_key_pressed(KEY_SPACE);
	bool space_just_pressed = space_down && !g.prev_space_down;
	g.prev_space_down = space_down;

	if (space_just_pressed) {
		spawn_projectile(ppos, (vec2){0.0f, -PROJECTILE_SPEED});
		g.flash_pos   = ppos;
		g.flash_timer = MUZZLE_FLASH_TIME;
	}

	// ---- Update projectiles ----
	for (int i = 0; i < MAX_PROJECTILES; i++) {
		projectile_t* p = &g.projectiles[i];
		if (!p->active) continue;

		// Move up
		vec2 pos = object2d_get_position(p->obj);
		pos.y -= PROJECTILE_SPEED * dts;
		object2d_set_position(p->obj, pos);

		// Off-screen cleanup
		if (pos.y < -PROJECTILE_SIZE) {
			destroy_projectile(p);
			continue;
		}

		// Hit-test against enemy
		if (rects_overlap(p->obj, g.enemy_static)) {
			g.score += 10;
			destroy_projectile(p);
		}
	}

	// ---- Muzzle flash timer ----
	if (g.flash_timer > 0.0f) g.flash_timer -= dts;

	// ---- HP slowly regenerating, and decrease if mouse is over enemy ----
	// (Just to make the HP bar move visibly during the demo.)
	camera2d_t* ui_cam = camera2d_get_ui();
	bool mouse_on_enemy = ui_rect_contains_mouse(ui_cam,
		object2d_get_position(g.enemy_static).x - 60,
		object2d_get_position(g.enemy_static).y - 60,
		120, 120);

	if (mouse_on_enemy) {
		g.player_hp -= 30.0f * dts;
	} else {
		g.player_hp += 10.0f * dts;
	}
	if (g.player_hp < 0.0f)             g.player_hp = 0.0f;
	if (g.player_hp > g.player_hp_max)  g.player_hp = g.player_hp_max;

	// ---- Reset button hit-test ----
	g.reset_button_hovered = ui_rect_contains_mouse(ui_cam,
		VIRTUAL_W - 220, 50, 170, 60);

	// Rising-edge detection on left mouse button
	bool mouse_left_down = is_mouse_button_pressed(MOUSE_BUTTON_LEFT);
	bool mouse_left_just_pressed = mouse_left_down && !g.prev_mouse_left_down;
	g.prev_mouse_left_down = mouse_left_down;

	if (g.reset_button_hovered && mouse_left_just_pressed) {
		g.score = 0;
		g.player_hp = g.player_hp_max;
	}

	// ---- Draw HUD (immediate-mode 2D — runs every frame) ----
	draw_hud(ui_cam);

	// ---- Muzzle flash effect (immediate sprite, lives one frame) ----
	if (g.flash_timer > 0.0f) {
		// Bright yellow square fading out as the timer drains.
		float t = g.flash_timer / MUZZLE_FLASH_TIME;
		color_t flash_color = {1.0f, 1.0f, 0.5f, t};
		float size = 80.0f + (1.0f - t) * 60.0f; // grows as it fades

		mat4 transform = mat4_multiply(
			mat4_translate(g.flash_pos.x, g.flash_pos.y - 20.0f, 0.0f),
			mat4_scale(size, size, 1.0f));

		sprite_submit(&(sprite_submit_t){
			.camera    = ui_cam,
			.material  = material2d_get_default(),
			.transform = transform,
			.uv_rect   = (vec4){0, 0, 1, 1},
			.color     = flash_color,
			.layer     = LAYER_FX,
		});
	}
}


// Renders the heads-up display: HP bar, score text, FPS counter, reset
// button. All immediate-mode — re-submitted every frame.
static void draw_hud(camera2d_t* ui_cam) {
	// ---- HP bar (top-left) ----
	// Border + background + fill, three layers stacked at LAYER_UI_BG.
	ui_rect_bordered(ui_cam,
		50, 50, 300, 40,
		(color_t){0.1f, 0.1f, 0.1f, 0.9f},   // dark fill
		COLOR_WHITE,                          // white border
		3.0f,                                 // 3-unit border
		LAYER_UI_BG);

	// Progress bar drawn slightly inset, on top of the border.
	float hp_pct = g.player_hp / g.player_hp_max;
	color_t hp_color = (hp_pct > 0.3f)
		? (color_t){0.2f, 0.8f, 0.2f, 1.0f}
		: (color_t){0.9f, 0.2f, 0.2f, 1.0f};

	ui_progress_bar(ui_cam,
		56, 56, 288, 28,
		hp_pct,
		(color_t){0.2f, 0.0f, 0.0f, 1.0f},  // dark red bg
		hp_color,
		LAYER_UI_FG);

	// HP label
	char hp_text[32];
	sprintf(hp_text, "HP: %.0f/%.0f", g.player_hp, g.player_hp_max);
	text_draw(hp_text, strlen(hp_text), 1.0f, 60, 100, COLOR_WHITE);

	// ---- Score ----
	char score_text[32];
	sprintf(score_text, "Score: %d", g.score);
	text_draw(score_text, strlen(score_text), 1.5f, 50, 150, CUBIT_YELLOW);

	// ---- FPS counter (top-left, smaller) ----
	char fps_text[32];
	sprintf(fps_text, "FPS: %.0f", application_fps());
	text_draw(fps_text, strlen(fps_text), 0.8f, 50, 220, CUBIT_GRAY);

	// ---- Reset button (top-right) ----
	color_t btn_bg = g.reset_button_hovered
		? (color_t){0.4f, 0.4f, 0.7f, 1.0f}
		: (color_t){0.2f, 0.2f, 0.4f, 1.0f};

	ui_rect_bordered(ui_cam,
		VIRTUAL_W - 220, 50, 170, 60,
		btn_bg, COLOR_WHITE, 2.0f, LAYER_UI_BG);

	text_draw("RESET", 5, 1.2f,
		(uint32_t)(VIRTUAL_W - 175), 70, COLOR_WHITE);

	// ---- Instructions (bottom-left) ----
	text_draw("WASD/Arrows: move", 17, 0.7f, 50, VIRTUAL_H - 100, CUBIT_GRAY);
	text_draw("Space: shoot",      12, 0.7f, 50, VIRTUAL_H - 70,  CUBIT_GRAY);
	text_draw("Click RESET to clear score", 26, 0.7f, 50, VIRTUAL_H - 40, CUBIT_GRAY);
}


void application_shutdown(void) {
	// Engine destroys the pool on object2d_shutdown(), but cleanly destroying
	// active sprites here is good practice.
	for (int i = 0; i < MAX_PROJECTILES; i++) {
		if (g.projectiles[i].active) destroy_projectile(&g.projectiles[i]);
	}
	if (g.player)       object2d_destroy(g.player);
	if (g.enemy_static) object2d_destroy(g.enemy_static);
}
