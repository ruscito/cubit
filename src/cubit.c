// cubit.c

#include "backend.h"
#include "shadow.h"


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

int main(void) {
	app_config_t cfg =  {
		.width = 800,
		.height = 450,
		.title = "GAME ENGINE",
		.fps = 60,
        .shadow_atlas_size = DEFAULT_SHADOW_ATLAS_SIZE,
        .shadow_tile_size = DEFAULT_SHADOW_TILE_SIZE
	};

	application_config(&cfg);
	gfx_context *gc = gfx_context_init(&cfg);
	renderer_init(gc);
    shadow_atlas_init(cfg.shadow_atlas_size, cfg.shadow_tile_size);
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
    shadow_atlas_shutdown();
	gfx_context_shutdown(gc);
	renderer_shutdown();
}
