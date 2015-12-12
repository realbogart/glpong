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

#define ROOM_SIZE		256
#define NUM_ROOMS		5

#define ROOMS_FILE_PATH	"C:/programmering/ld34/glpong/ld34/assets/allrooms.world"

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

enum tile_type
{
	TILE_NONE = 0,
	TILE_WOOD,
	TILE_GRAIN,
	TILE_COBBLE,
	TILE_WALL_TOP,
	TILE_WALL_MID,
	TILE_WALL_BOTTOM
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

struct room_tile
{
	enum tile_type	type_back;
	enum tile_type	type_front;

	int				extra_data[8];

	struct anim*	tile_back;
	struct anim*	tile_front;
};

struct room
{
	struct room_tile tiles[ROOM_SIZE];
};

struct game {
	struct atlas			atlas;
	struct animatedsprites	*batcher;
	struct tiles			tiles_back;
	struct tiles			tiles_front;

	struct player			player;

	vec2					camera;

	struct room				rooms[NUM_ROOMS];

	int						room_index;

	int						edit_current_room;
	enum tile_type			edit_current_type;

	struct anim tiles_grass;
	struct anim tiles_cobble;
	struct anim tiles_grain;
	struct anim tiles_wood;
	struct anim tiles_wall_top;
	struct anim tiles_wall_mid;
	struct anim tiles_wall_bottom;
} *game = NULL;

void room_setup_tile(struct game* game, int room_index, int tile_index, enum tile_type type, int back)
{
	struct anim** tile = back ? &game->rooms[room_index].tiles[tile_index].tile_back : 
								&game->rooms[room_index].tiles[tile_index].tile_front;

	if (back)
	{
		game->rooms[room_index].tiles[tile_index].type_back = type;
	}
	else
	{
		game->rooms[room_index].tiles[tile_index].type_front = type;
	}

	switch (type)
	{
		case TILE_NONE:
			*tile = 0;
			break;
		case TILE_WOOD:
			*tile = &game->tiles_wood;
			break;
		case TILE_GRAIN:
			*tile = &game->tiles_grain;
			break;
		case TILE_COBBLE:
			*tile = &game->tiles_cobble;
			break;
		case TILE_WALL_TOP:
			*tile = &game->tiles_wall_top;
			break;
		case TILE_WALL_MID:
			*tile = &game->tiles_wall_mid;
			break;
		case TILE_WALL_BOTTOM:
			*tile = &game->tiles_wall_bottom;
			break;
	}
}

void rooms_save(struct game* game)
{
	FILE *fp;
	fp = fopen(ROOMS_FILE_PATH, "wb+");

	for (int i = 0; i < NUM_ROOMS; i++)
	{
		for (int j = 0; j < ROOM_SIZE; j++)
		{
			fwrite(&game->rooms[i].tiles[j].type_back, sizeof(int), 1, fp);
			fwrite(&game->rooms[i].tiles[j].type_front, sizeof(int), 1, fp);
			fwrite(&game->rooms[i].tiles[j].extra_data, sizeof(int), 8, fp);
		}
	}

	fclose(fp);
}

void rooms_load(struct game* game)
{
	FILE *fp;
	fp = fopen(ROOMS_FILE_PATH, "rb");

	for (int i = 0; i < NUM_ROOMS; i++)
	{
		for (int j = 0; j < ROOM_SIZE; j++)
		{
			enum tile_type type;

			fread(&type, sizeof(int), 1, fp);
			room_setup_tile(game, i, j, type, 1);

			fread(&type, sizeof(int), 1, fp);
			room_setup_tile(game, i, j, type, 0);

			fread(game->rooms[i].tiles[j].extra_data, sizeof(int), 8, fp);
		}
	}
	fclose(fp);
}

void rooms_init(struct game* game)
{
	for (int i = 0; i < NUM_ROOMS; i++)
	{
		for (int j = 0; j < ROOM_SIZE; j++)
		{
			for (int k = 0; k < 8; k++)
			{
				game->rooms[i].tiles[j].extra_data[8] = 0;
			}
	
			game->rooms[i].tiles[j].type_back = TILE_NONE;
			game->rooms[i].tiles[j].type_front = TILE_NONE;
		}
	}
}

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

	game->player.speed = 0.04f;
	game->player.state = &player_idle;
	game->player.dir = DIR_LEFT;

	animatedsprites_setanim(&game->player.anim_idle_left, 1, 0, 2, 700.0f);
	animatedsprites_setanim(&game->player.anim_idle_right, 1, 2, 2, 700.0f);
	animatedsprites_setanim(&game->player.anim_idle_up, 1, 4, 2, 700.0f);
	animatedsprites_setanim(&game->player.anim_idle_down, 1, 6, 2, 700.0f);

	animatedsprites_setanim(&game->player.anim_walk_left, 1, 8, 2, 150.0f);
	animatedsprites_setanim(&game->player.anim_walk_right, 1, 10, 2, 150.0f);
	animatedsprites_setanim(&game->player.anim_walk_up, 1, 12, 2, 150.0f);
	animatedsprites_setanim(&game->player.anim_walk_down, 1, 14, 2, 150.0f);

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

void room_edit()
{
	// Room save/load logic
	if (key_pressed(GLFW_KEY_F9))
	{
		rooms_save(game);
	}

	if (key_pressed(GLFW_KEY_F4))
	{
		game->edit_current_room = 0;
	}
	if (key_pressed(GLFW_KEY_F5))
	{
		game->edit_current_room = 1;
	}
	if (key_pressed(GLFW_KEY_F6))
	{
		game->edit_current_room = 2;
	}
	if (key_pressed(GLFW_KEY_F7))
	{
		game->edit_current_room = 3;
	}
	if (key_pressed(GLFW_KEY_F8))
	{
		game->edit_current_room = 4;
	}

	if (key_pressed(GLFW_KEY_KP_0))
	{
		game->edit_current_type = TILE_NONE;
	}
	if (key_pressed(GLFW_KEY_KP_1))
	{
		game->edit_current_type = TILE_WALL_BOTTOM;
	}
	if (key_pressed(GLFW_KEY_KP_2))
	{
		game->edit_current_type = TILE_WALL_MID;
	}
	if (key_pressed(GLFW_KEY_KP_3))
	{
		game->edit_current_type = TILE_WALL_TOP;
	}
	if (key_pressed(GLFW_KEY_KP_4))
	{
		game->edit_current_type = TILE_WOOD;
	}
	if (key_pressed(GLFW_KEY_KP_5))
	{
		game->edit_current_type = TILE_GRAIN;
	}
	if (key_pressed(GLFW_KEY_KP_6))
	{
		game->edit_current_type = TILE_WOOD;
	}
	if (key_pressed(GLFW_KEY_KP_7))
	{
	}
	if (key_pressed(GLFW_KEY_KP_8))
	{

	}
	if (key_pressed(GLFW_KEY_KP_9))
	{

	}
}

void game_think(struct core *core, struct graphics *g, float dt)
{
	room_edit();

	vec2 offset;
	set2f(offset, 0.0f, 0.0f);

	player_think( game, dt );
	
	float dx = (game->player.sprite.position[0] - game->camera[0]) / 100.0f;
	float dy = (game->player.sprite.position[1] - game->camera[1]) / 100.0f;
	
	game->camera[0] += dx;
	game->camera[1] += dy;

	animatedsprites_update(game->batcher, &game->atlas, dt);
	tiles_think(&game->tiles_back, game->camera, &game->atlas, dt);
	tiles_think(&game->tiles_front, game->camera, &game->atlas, dt);
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
	translate(offset, -game->camera[0] + VIEW_WIDTH / 2, -game->camera[1] + VIEW_HEIGHT / 2, 0.0f);
	transpose(final, offset);

	tiles_render(&game->tiles_back, &assets->shaders.basic_shader, g, assets->textures.textures, transform);
	animatedsprites_render(game->batcher, &assets->shaders.basic_shader, g, assets->textures.textures, final);
	tiles_render(&game->tiles_front, &assets->shaders.basic_shader, g, assets->textures.textures, transform);
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

struct anim* tiles_get_back_data_at(float x, float y,
	int tile_size, int grid_x_max, int grid_y_max)
{
	int grid_x = (int)floor(x / (float)tile_size);
	int grid_y = (int)floor(y / (float)tile_size);

	if (grid_x < 0 || grid_y < 0 || grid_x >= grid_x_max || grid_y >= grid_y_max) 
	{
		return 0;
	}

	int tile_index = grid_y * grid_x_max + grid_x;
	return game->rooms[game->room_index].tiles[tile_index].tile_back;
}

struct anim* tiles_get_front_data_at(float x, float y,
	int tile_size, int grid_x_max, int grid_y_max)
{
	int grid_x = (int)floor(x / (float)tile_size);
	int grid_y = (int)floor(y / (float)tile_size);

	if (grid_x < 0 || grid_y < 0 || grid_x >= grid_x_max || grid_y >= grid_y_max)
	{
		return 0;
	}

	int tile_index = grid_y * grid_x_max + grid_x;
	return game->rooms[game->room_index].tiles[tile_index].tile_front;
}

void game_init()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	game->batcher = animatedsprites_create();

	animatedsprites_setanim(&game->tiles_grass, 1, 16, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_grain, 1, 17, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_wood, 1, 18, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_cobble, 1, 19, 1, 100.0f);

	animatedsprites_setanim(&game->tiles_wall_top, 1, 21, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_wall_mid, 1, 22, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_wall_bottom, 1, 23, 1, 100.0f);

	tiles_init(&game->tiles_back, &tiles_get_back_data_at, 16, VIEW_WIDTH, VIEW_HEIGHT, 16, 16);
	tiles_init(&game->tiles_front, &tiles_get_front_data_at, 16, VIEW_WIDTH, VIEW_HEIGHT, 16, 16);

	game->edit_current_room = 0;
	game->edit_current_type = TILE_NONE;

	player_init(game);
	rooms_load(game);
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
