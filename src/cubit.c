// cubit.c

#include "backend.h"
#include "shadow.h"
#include "camera2d.h"
#include "object2d.h"
#include "batch2d.h"
#include "material2d.h"
#include "cubit.h"

#include <assert.h>
#include <stddef.h>


/*
 * External funcion user provided
 */
extern void application_init(void);
extern void application_update(double dt);
extern void application_shutdown(void);
extern void application_config(app_config_t *config);
extern void input_update(void);
extern void application_fixed_update(double dt);

double application_fps(void) {
    return renderer_fps();
}

void fixed_update(double dt){
    application_fixed_update(dt);
}


// Public immediate-mode 2D sprite API.
//
// Thin wrapper around batch2d_push_immediate(): the only added value is
// validating the contract (non-NULL pointers in the config struct) and
// hiding batch2d as an engine-internal module — the game never includes
// batch2d.h.
//
// See sprite_submit_t in cubit_types.h for the full field documentation.
void sprite_submit(const sprite_submit_t* cfg) {
    assert(cfg              != NULL && "sprite_submit: cfg is NULL");
    assert(cfg->camera      != NULL && "sprite_submit: cfg->camera is NULL — use camera2d_get_ui() if unsure");
    assert(cfg->material    != NULL && "sprite_submit: cfg->material is NULL — use material2d_get_default() if unsure");

    batch2d_push_immediate(
        cfg->camera,
        cfg->layer,
        cfg->material,
        cfg->transform,
        cfg->uv_rect,
        cfg->color
    );
}


// ---- UI Primitives ----
//
// Thin helpers on top of sprite_submit() that produce common shapes (filled
// rect, bordered rect, progress bar) using the engine's 1x1 white default
// texture (provided by material2d_get_default()). Each call is one or more
// immediate sprites — no retained state, no resources to manage.
//
// Coordinate convention matches text_draw():
//   - (x, y) is the top-left corner of the rect
//   - the batch2d quad is centered on origin, so we offset by (w/2, h/2)
//     internally to convert top-left placement into center-pivot transform


// Helper used by all three primitives below. Submits one filled rectangle as
// an immediate sprite at the given coordinates, with the white default
// material so the per-instance color shows through unmodified.
static void ui_submit_rect(camera2d_t* cam,
                           float x, float y, float w, float h,
                           color_t color, int32_t layer)
{
    mat4 t = mat4_translate(x + w * 0.5f, y + h * 0.5f, 0.0f);
    mat4 s = mat4_scale(w, h, 1.0f);
    mat4 transform = mat4_multiply(t, s);

    sprite_submit(&(sprite_submit_t){
        .camera    = cam,
        .material  = material2d_get_default(),
        .transform = transform,
        .uv_rect   = (vec4){0.0f, 0.0f, 1.0f, 1.0f},
        .color     = color,
        .layer     = layer,
    });
}


// Draws a solid-color rectangle at (x, y) with the given size.
// One sprite per call. Stretchable to any size — the white texture is
// uniform so there is no scaling artifact.
void ui_rect_filled(camera2d_t* cam,
                    float x, float y, float w, float h,
                    color_t color, int32_t layer)
{
    ui_submit_rect(cam, x, y, w, h, color, layer);
}


// Draws a rectangle with a solid fill and a uniform-width border.
// Implemented as five sprites: one for the inner fill, four for the border
// strips (top, bottom, left, right). The border draws on the same layer as
// the fill, but is submitted after — submission order is the tiebreaker so
// the border ends up visually on top.
//
// border_width is in virtual units. If border_width >= min(w, h) / 2, the
// "fill" disappears and you see only the border color — degenerate but not
// incorrect.
void ui_rect_bordered(camera2d_t* cam,
                      float x, float y, float w, float h,
                      color_t fill_color, color_t border_color,
                      float border_width, int32_t layer)
{
    // Inner fill, inset by border_width on every side.
    float ix = x + border_width;
    float iy = y + border_width;
    float iw = w - border_width * 2.0f;
    float ih = h - border_width * 2.0f;
    if (iw > 0.0f && ih > 0.0f) {
        ui_submit_rect(cam, ix, iy, iw, ih, fill_color, layer);
    }

    // Four border strips. Top and bottom span full width; left and right
    // are inset vertically so the corners belong to top/bottom only — no
    // double overdraw on alpha-blended borders.
    ui_submit_rect(cam, x, y,
                   w, border_width, border_color, layer);                       // top
    ui_submit_rect(cam, x, y + h - border_width,
                   w, border_width, border_color, layer);                       // bottom
    ui_submit_rect(cam, x, y + border_width,
                   border_width, h - border_width * 2.0f, border_color, layer); // left
    ui_submit_rect(cam, x + w - border_width, y + border_width,
                   border_width, h - border_width * 2.0f, border_color, layer); // right
}


// Draws a horizontal progress bar: a background rect with a foreground rect
// overlaid, the foreground's width scaled by fill_pct.
//
// fill_pct is clamped to [0, 1] internally — out-of-range values won't crash
// or overflow, they just clamp to empty/full. The fill grows from the left.
void ui_progress_bar(camera2d_t* cam,
                     float x, float y, float w, float h,
                     float fill_pct,
                     color_t bg_color, color_t fill_color,
                     int32_t layer)
{
    if (fill_pct < 0.0f) fill_pct = 0.0f;
    if (fill_pct > 1.0f) fill_pct = 1.0f;

    // Background spans the full width.
    ui_submit_rect(cam, x, y, w, h, bg_color, layer);

    // Foreground only renders if there is something to show — avoids a
    // zero-width sprite that would still cost a batch record.
    float fw = w * fill_pct;
    if (fw > 0.0f) {
        ui_submit_rect(cam, x, y, fw, h, fill_color, layer);
    }
}


// ---- Hit-test primitives ----

// Pure geometric test. Projects the given screen-logical point into the
// camera's virtual coordinate space, then runs an axis-aligned rect test.
//
// Why this is the foundation: it accepts an arbitrary point, not "the mouse
// right now". This makes it usable for touch input, multi-touch (call it
// once per finger), simulated input in tests, AI agents probing the UI,
// replay systems, and any future input source we haven't thought of yet.
// The mouse-specific helper is built on top of this — never the other way
// around.
//
// The window size is read internally because it is engine-owned state, not
// game-supplied. The point however is a parameter: the caller decides what
// it represents.
//
// Coordinate spaces, to avoid confusion:
//   - (x, y, w, h) is in the camera's virtual units (top-left origin)
//   - (point_x_screen, point_y_screen) is in window-logical pixels — the
//     same space GLFW reports the cursor in, regardless of DPI/Retina.
bool ui_point_in_rect(camera2d_t* cam,
                      float x, float y, float w, float h,
                      float point_x_screen, float point_y_screen)
{
    assert(cam != NULL && "ui_point_in_rect: camera is NULL");

    int32_t win_w, win_h;
    backend_get_window_size(&win_w, &win_h);

    vec2 vp = camera2d_screen_to_virtual(
        cam, point_x_screen, point_y_screen,
        (float)win_w, (float)win_h);

    return vp.x >= x && vp.x <= x + w
        && vp.y >= y && vp.y <= y + h;
}


// Convenience wrapper: reads live mouse position and delegates to the pure
// primitive. Use this for the 90% case of mouse-driven UI.
bool ui_rect_contains_mouse(camera2d_t* cam,
                            float x, float y, float w, float h)
{
    double mx, my;
    get_mouse_position(&mx, &my);
    return ui_point_in_rect(cam, x, y, w, h, (float)mx, (float)my);
}


int main(void) {
	app_config_t cfg =  {
		.width = 800,
		.height = 450,
		.title = "GAME ENGINE",
		.fps = 60,
        .shadow_atlas_size = DEFAULT_SHADOW_ATLAS_SIZE,
        .shadow_tile_size = DEFAULT_SHADOW_TILE_SIZE,
        .ui_virtual_resolution = {1920.0f, 1080.0f}
	};

	application_config(&cfg);
	gfx_context *gc = gfx_context_init(&cfg);
	renderer_init(gc);
    shadow_atlas_init(cfg.shadow_atlas_size, cfg.shadow_tile_size);
    camera2d_init(cfg.ui_virtual_resolution);
    object2d_init();
	application_init();
    renderer_loop_setup();
	while (!renderer_should_close()){
		renderer_begin_frame();
        renderer_fixed_update();
		input_update();
		renderer_process_input();
		application_update(renderer_dt());
		renderer_draw();
		renderer_end_frame();
	}
	application_shutdown();
    object2d_shutdown();
    camera2d_shutdown();
    shadow_atlas_shutdown();
	gfx_context_shutdown(gc);
	renderer_shutdown();
}
