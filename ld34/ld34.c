/**
 * Ludum dare 34 game!
 *
 * Author: Johan Yngman <johan.yngman@gmail.com
 * Date: 2015
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "math4.h"
#include "assets.h"
#include "game.h"
#include "core.h"
#include "core_reload.h"
#include "input.h"
#include "atlas.h"
#include "animatedsprites.h"
#include "top-down\tiles.h"

#define VIEW_WIDTH      1024
#define VIEW_HEIGHT		428

#define NUM_TILES		4096

struct game_settings settings = {
	.view_width			= VIEW_WIDTH,
	.view_height		= VIEW_HEIGHT,
	.window_width		= VIEW_WIDTH,
	.window_height		= VIEW_HEIGHT,
	.window_title		= "ld34",
	.sound_listener		= { VIEW_WIDTH / 2.0f, VIEW_HEIGHT / 2.0f, 0.0f },
	.sound_distance_max = 500.0f, // distance3f(vec3(0), sound_listener)
};

struct game {
	struct sprite			sprite;
	struct atlas			atlas;
	struct animatedsprites	*batcher;
	struct anim				anim_sprite;
	struct tiles			tiles;

	struct anim tiles_grass;
	struct anim* tiles_data[NUM_TILES];
} *game = NULL;

struct game_settings* game_get_settings()
{
	return &settings;
}

void game_init_memory(struct shared_memory *shared_memory, int reload)
{
	if(!reload) {
		memset(shared_memory->game_memory, 0, sizeof(struct game));
	}

	game = (struct game *) shared_memory->game_memory;
	core_global = shared_memory->core;
	assets = shared_memory->assets;
	vfs_global = shared_memory->vfs;
	input_global = shared_memory->input;
}

void game_think(struct core *core, struct graphics *g, float dt)
{
	vec2 offset;
	set2f(offset, 0.0f, 0.0f);

	tiles_think(&game->tiles, offset, &game->atlas, dt);
	animatedsprites_update(game->batcher, &game->atlas, dt);
	shader_uniforms_think(&assets->shaders.basic_shader, dt);
}

void game_render(struct core *core, struct graphics *g, float dt)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	mat4 transform;
	identity(transform);
	mat4 offset;
	mat4 final;

	set2f(offset, 0.0f, 0.0f);
	translate(offset, roundf(-offset[0]), roundf(-offset[1]), 0.0f);
	transpose(final, offset);

	tiles_render(&game->tiles, &assets->shaders.basic_shader, g, assets->textures.textures, transform);
	animatedsprites_render(game->batcher, &assets->shaders.basic_shader, g, assets->textures.textures, final);
}

void game_mousebutton_callback(struct core *core, GLFWwindow *window, int button, int action, int mods)
{
	if(action == GLFW_PRESS) {
		float x = 0, y = 0;
		input_view_get_cursor(window, &x, &y);
		console_debug("Click at %.0fx%.0f\n", x, y);
	}
}

void game_console_init(struct console *c)
{
	/* console_env_bind_1f(c, "print_fps", &(game->print_fps)); */
}

void setup_tiles()
{
	for (int i = 0; i < NUM_TILES; i++)
	{
		game->tiles_data[i] = &game->tiles_grass;
	}
}

void game_init()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	/* Create animated sprite batcher. */
	game->batcher = animatedsprites_create();

	/* Setup tiles */
	animatedsprites_setanim(&game->tiles_grass, 1, 1, 1, 100.0f);

	setup_tiles();
	tiles_init(&game->tiles, &game->tiles_data, 16, VIEW_WIDTH, VIEW_HEIGHT, 64, 64);

	/* Create sprite animation. */
	animatedsprites_setanim(&game->anim_sprite, 1, 0, 1, 300.0f);

	/* Create sprite. */
	set3f(game->sprite.position, VIEW_WIDTH/2.0f, VIEW_HEIGHT/2.0f, 0);
	set2f(game->sprite.scale, 10.0f, 10.0f);

	animatedsprites_playanimation(&game->sprite, &game->anim_sprite);
	animatedsprites_add(game->batcher, &game->sprite);
}

void game_key_callback(struct core *core, struct input *input, GLFWwindow *window, int key,
	int scancode, int action, int mods)
{
	if(action == GLFW_PRESS) {
		switch(key) {
			case GLFW_KEY_ESCAPE:
				glfwSetWindowShouldClose(window, 1);
				break;
		}
	}
}

void game_assets_load()
{
	assets_load();
	vfs_register_callback("textures.json", core_reload_atlas, &game->atlas);
}

void game_assets_release()
{
	assets_release();
	atlas_free(&game->atlas);
}

void game_fps_callback(struct frames *f)
{
	core_debug("FPS:% 5d, MS:% 3.1f/% 3.1f/% 3.1f\n", f->frames,
			f->frame_time_min, f->frame_time_avg, f->frame_time_max);
}
