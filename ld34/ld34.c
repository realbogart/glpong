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

#define VIEW_WIDTH      256
#define VIEW_HEIGHT		107

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

struct game;
typedef void(*player_state_t)(struct game*, float);

enum direction
{
	DIR_RIGHT = 0,
	DIR_LEFT,
	DIR_UP,
	DIR_DOWN
};

struct player{
	struct sprite			sprite;
	struct anim				anim_idle_left;
	struct anim				anim_idle_right;
	struct anim				anim_idle_up;
	struct anim				anim_idle_down;

	struct anim				anim_walk_left;
	struct anim				anim_walk_right;
	struct anim				anim_walk_up;
	struct anim				anim_walk_down;

	enum direction			dir;

	float					speed;
	player_state_t			state;
};

struct game {
	struct atlas			atlas;
	struct animatedsprites	*batcher;
	struct tiles			tiles;

	struct player			player;

	struct anim tiles_grass;
	//struct anim* tiles_data[NUM_TILES];
} *game = NULL;

/*
* PLAYER
*/
void player_walk(struct game* game, float dt);

void player_idle(struct game* game, float dt)
{
	int enter_walk = 0;
	if(key_pressed(GLFW_KEY_LEFT))
	{
		game->player.dir = DIR_LEFT;
		enter_walk = 1;
	}
	else if (key_pressed(GLFW_KEY_RIGHT))
	{
		game->player.dir = DIR_RIGHT;
		enter_walk = 1;
	}
	else if (key_pressed(GLFW_KEY_UP))
	{
		game->player.dir = DIR_UP;
		enter_walk = 1;
	}
	else if (key_pressed(GLFW_KEY_DOWN))
	{
		game->player.dir = DIR_DOWN;
		enter_walk = 1;
	}

	if (enter_walk)
	{
		game->player.state = &player_walk;
		return;
	}

	// Set correct idle animation
	switch (game->player.dir)
	{
	case DIR_LEFT:
		animatedsprites_switchanimation(&game->player, &game->player.anim_idle_left);
		break;
	case DIR_RIGHT:
		animatedsprites_switchanimation(&game->player, &game->player.anim_idle_right);
		break;
	case DIR_UP:
		animatedsprites_switchanimation(&game->player, &game->player.anim_idle_up);
		break;
	case DIR_DOWN:
		animatedsprites_switchanimation(&game->player, &game->player.anim_idle_down);
		break;
	}
}

void player_walk(struct game* game, float dt)
{
	// Check if player idle
	if (!key_down(GLFW_KEY_RIGHT) && 
		!key_down(GLFW_KEY_LEFT) && 
		!key_down(GLFW_KEY_UP) && 
		!key_down(GLFW_KEY_DOWN))
	{
		game->player.state = &player_idle;
		return;
	}

	vec2 pos;
	set2f(pos, game->player.sprite.position[0], game->player.sprite.position[1]);

	// Check if player change direction while moving
	if (key_down(GLFW_KEY_LEFT))
	{
		game->player.dir = DIR_LEFT;
		pos[0] -= game->player.speed * dt;
	}
	if (key_down(GLFW_KEY_RIGHT))
	{
		game->player.dir = DIR_RIGHT;
		pos[0] += game->player.speed * dt;
	}
	if (key_down(GLFW_KEY_UP))
	{
		game->player.dir = DIR_UP;
		pos[1] += game->player.speed * dt;
	}
	if (key_down(GLFW_KEY_DOWN))
	{
		game->player.dir = DIR_DOWN;
		pos[1] -= game->player.speed * dt;
	}

	// Move player and set correct walking animation
	switch (game->player.dir)
	{
	case DIR_LEFT:
	{
		animatedsprites_switchanimation(&game->player, &game->player.anim_walk_left);
	}
		break;
	case DIR_RIGHT:
	{
		animatedsprites_switchanimation(&game->player, &game->player.anim_walk_right);
	}
		break;
	case DIR_UP:
	{
		animatedsprites_switchanimation(&game->player, &game->player.anim_walk_up);
	}
		break;
	case DIR_DOWN:
	{
		animatedsprites_switchanimation(&game->player, &game->player.anim_walk_down);
	}
		break;
	}

	// TODO: Collision check

	set2f(game->player.sprite.position, pos[0], pos[1]);
}

void player_init(struct game* game)
{
	set3f(game->player.sprite.position, VIEW_WIDTH / 2.0f, VIEW_HEIGHT / 2.0f, 0);
	set2f(game->player.sprite.scale, 2.0f, 2.0f);

	game->player.speed = 0.05f;
	game->player.state = &player_idle;
	game->player.dir = DIR_LEFT;

	animatedsprites_setanim(&game->player.anim_idle_left, 1, 0, 2, 700.0f);
	animatedsprites_setanim(&game->player.anim_idle_right, 1, 2, 2, 700.0f);
	animatedsprites_setanim(&game->player.anim_idle_up, 1, 4, 2, 700.0f);
	animatedsprites_setanim(&game->player.anim_idle_down, 1, 6, 2, 700.0f);

	animatedsprites_setanim(&game->player.anim_walk_left, 1, 8, 2, 110.0f);
	animatedsprites_setanim(&game->player.anim_walk_right, 1, 10, 2, 110.0f);
	animatedsprites_setanim(&game->player.anim_walk_up, 1, 12, 2, 110.0f);
	animatedsprites_setanim(&game->player.anim_walk_down, 1, 14, 2, 110.0f);

	animatedsprites_playanimation(&game->player.sprite, &game->player.anim_idle_left);
	animatedsprites_add(game->batcher, &game->player.sprite);
}

void player_think(struct game* game, float dt)
{
	game->player.state(game, dt);
}



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

	player_think( game, dt );
	
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

struct anim* tiles_get_data_at(float x, float y,
	int tile_size, int grid_x_max, int grid_y_max)
{
	return &game->tiles_grass;
	//int grid_x = (int)floor(x / (float)tile_size);
	//int grid_y = (int)floor(y / (float)tile_size);
	//if (grid_x < 0 || grid_y < 0
	//	|| grid_x >= grid_x_max || grid_y >= grid_y_max) {
	//	return NULL;
	//}
	//return game->tiles_data[grid_y * grid_x_max + grid_x];
}


//void setup_tiles(struct game* game)
//{
//	animatedsprites_setanim(&game->tiles_grass, 1, 16, 1, 100.0f);
//
//	for (int i = 0; i < NUM_TILES; i++)
//	{
//		game->tiles_data[i] = &game->tiles_grass;
//	}
//}

void game_init()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	game->batcher = animatedsprites_create();

	//setup_tiles(game);
	animatedsprites_setanim(&game->tiles_grass, 1, 16, 1, 100.0f);
	tiles_init(&game->tiles, &tiles_get_data_at, 16, VIEW_WIDTH, VIEW_HEIGHT, 64, 64);

	player_init( game );
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
