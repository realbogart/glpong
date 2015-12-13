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
#include "collide.h"
#include "drawable.h"
#include "GLFW\glfw3.h"

#define VIEW_WIDTH      256
#define VIEW_HEIGHT		107

#define ROOM_SIZE		256
#define NUM_ROOMS		5
#define MAX_MONSTERS	1024

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
typedef void(*monster_state_t)(struct monster* monster, float);

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
	TILE_WALL_BOTTOM,
	TILE_STONEFLOOR
};

enum extra_data
{
	EXTRA_DATA_NONE = 0,
	EXTRA_DATA_MONSTER,
	EXTRA_DATA_KEY,
	EXTRA_DATA_DOOR
};

struct monster
{
	struct sprite	sprite;
	
	enum direction	dir;

	monster_state_t	state;

	float			speed;

	struct anim		anim_walk_left;
	struct anim		anim_walk_right;
	struct anim		anim_walk_up;
	struct anim		anim_walk_down;
};

struct item
{
	struct sprite			sprite;
};

struct player{
	struct sprite			sprite;
	struct sprite			sprite_arms;

	struct anim				anim_idle_left;
	struct anim				anim_idle_right;
	struct anim				anim_idle_up;
	struct anim				anim_idle_down;
	struct anim				anim_walk_left;
	struct anim				anim_walk_right;
	struct anim				anim_walk_up;
	struct anim				anim_walk_down;

	struct anim				anim_arms_idle_left;
	struct anim				anim_arms_idle_right;
	struct anim				anim_arms_idle_up;
	struct anim				anim_arms_idle_down;
	struct anim				anim_arms_walk_left;
	struct anim				anim_arms_walk_right;
	struct anim				anim_arms_walk_up;
	struct anim				anim_arms_walk_down;
	struct anim				anim_die;

	int						alive;
	int						holding_item;

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
	struct room_tile	tiles[ROOM_SIZE];
	struct monster		monsters[MAX_MONSTERS];
	int					num_monsters;
};

struct game {
	struct sprite			debug_pos;
	struct sprite			sprite_killscreen;

	struct anim				anim_debug;

	struct atlas			atlas;
	struct animatedsprites	*batcher;
	struct animatedsprites	*batcher_statics;

	struct drawable			drawable_monster;
	struct drawable			drawable_player;

	struct tiles			tiles_back;
	struct tiles			tiles_front;

	struct player			player;

	vec2					camera;

	struct room				rooms[NUM_ROOMS];

	int						room_index;

	enum tile_type			edit_current_type;

	struct anim anim_empty;

	struct anim tiles_grass;
	struct anim tiles_cobble;
	struct anim tiles_grain;
	struct anim tiles_wood;
	struct anim tiles_stonefloor;
	struct anim tiles_wall_top;
	struct anim tiles_wall_mid;
	struct anim tiles_wall_bottom;
} *game = NULL;

struct room_tile* get_room_tile_at(float x, float y, int tile_size, int grid_x_max, int grid_y_max, int* tile_index_out);
void room_edit_place_front(float x, float y);
void room_edit_place_back(float x, float y);

int is_tile_collision(struct game* game, float x, float y)
{
	x += VIEW_WIDTH / 2;
	y += VIEW_HEIGHT / 2;

	int index = -1;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);

	if (index != -1)
	{
		if (tile->type_back != TILE_STONEFLOOR)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	return 1;
}

void monster_hunt(struct monster* monster, float dt)
{
	vec2 pos;
	set2f(pos, monster->sprite.position[0], monster->sprite.position[1]);

	vec2 check_forward;
	vec2 check_right;
	vec2 check_left;
	vec2 check_back;

	float check_dist = 8.0f;

	set2f(check_forward, monster->sprite.position[0], monster->sprite.position[1]);
	set2f(check_left, monster->sprite.position[0], monster->sprite.position[1]);
	set2f(check_right, monster->sprite.position[0], monster->sprite.position[1]);
	set2f(check_back, monster->sprite.position[0], monster->sprite.position[1]);

	enum new_dir right = -1;
	enum new_dir left = -1;
	enum new_dir back = -1;

	switch (monster->dir)
	{
	case DIR_LEFT:
	{
		animatedsprites_switchanimation(&monster->sprite, &monster->anim_walk_left);
		pos[0] -= monster->speed * dt;

		right = DIR_UP;
		left = DIR_DOWN;
		back = DIR_RIGHT;

		check_forward[0] -= check_dist;
		check_back[0] += check_dist;
		check_right[1] += check_dist;
		check_left[1] -= check_dist;
	}
		break;
	case DIR_RIGHT:
	{
		animatedsprites_switchanimation(&monster->sprite, &monster->anim_walk_right);
		pos[0] += monster->speed * dt;

		right = DIR_DOWN;
		left = DIR_UP;
		back = DIR_LEFT;

		check_forward[0] += check_dist;
		check_back[0] -= check_dist;
		check_right[1] -= check_dist;
		check_left[1] += check_dist;
	}
		break;
	case DIR_UP:
	{
		animatedsprites_switchanimation(&monster->sprite, &monster->anim_walk_up);
		pos[1] += monster->speed * dt;

		right = DIR_RIGHT;
		left = DIR_LEFT;
		back = DIR_DOWN;

		check_forward[1] += check_dist;
		check_back[1] -= check_dist;
		check_right[0] += check_dist;
		check_left[0] -= check_dist;
	}
		break;
	case DIR_DOWN:
	{
		animatedsprites_switchanimation(&monster->sprite, &monster->anim_walk_down);
		pos[1] -= monster->speed * dt;

		right = DIR_LEFT;
		left = DIR_RIGHT;
		back = DIR_UP;

		check_forward[1] -= check_dist;
		check_back[1] += check_dist;
		check_right[0] -= check_dist;
		check_left[0] += check_dist;
	}
		break;
	}

	int path_free = 1;

	if (is_tile_collision(game, check_forward[0], check_forward[1]))
	{
		int r = !is_tile_collision(game, check_right[0], check_right[1]);
		int l = !is_tile_collision(game, check_left[0], check_left[1]);

		if (r && l)
		{
			if (rand() % 2 == 0)
			{
				monster->dir = right;
			}
			else
			{
				monster->dir = left;
			}
		}
		else if (r)
		{
			monster->dir = right;
		}
		else if (l)
		{
			monster->dir = left;
		}
		else if (!is_tile_collision(game, check_back[0], check_back[1]))
		{
			monster->dir = back;
		}
		else
		{
			path_free = 0;
		}
	}

	if (path_free)
	{
		set2f(monster->sprite.position, pos[0], pos[1]);
	}

	drawable_new_circle_outlinef(&game->drawable_monster, monster->sprite.position[0], monster->sprite.position[1], 6.0f, 32, &assets->shaders.basic_shader);
	drawable_new_circle_outlinef(&game->drawable_player, game->player.sprite.position[0], game->player.sprite.position[1] - 6.0f, 6.0f, 32, &assets->shaders.basic_shader);

	// Check for player collision
	if (collide_circlef(monster->sprite.position[0], monster->sprite.position[1], 6.0f, 
						game->player.sprite.position[0], game->player.sprite.position[1] - 6.0f, 6.0f))
	{
		game->player.alive = 0;
	}
}

void room_add_monster(int room_index, float x, float y)
{
	struct room* room = &game->rooms[room_index];

	if (room->num_monsters >= MAX_MONSTERS)
	{
		return;
	}

	struct monster* monster = &room->monsters[room->num_monsters];

	set2f(monster->sprite.position, x, y);
	set2f(monster->sprite.scale, 0.8f, 0.8f);

	monster->dir = DIR_LEFT;
	monster->speed = 0.08f;
	monster->state = &monster_hunt;
	room->num_monsters++;

	animatedsprites_setanim(&monster->anim_walk_left, 1, atlas_frame_index(&game->atlas, "monster_walk_left"), 2, 100.0f);
	animatedsprites_setanim(&monster->anim_walk_right, 1, atlas_frame_index(&game->atlas, "monster_walk_right"), 2, 100.0f);
	animatedsprites_setanim(&monster->anim_walk_up, 1, atlas_frame_index(&game->atlas, "monster_walk_up"), 2, 100.0f);
	animatedsprites_setanim(&monster->anim_walk_down, 1, atlas_frame_index(&game->atlas, "monster_walk_down"), 2, 100.0f);

	animatedsprites_add(game->batcher, &monster->sprite);
}

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
		{
			*tile = &game->tiles_wall_mid;
		}
			break;
		case TILE_WALL_BOTTOM:
		{
			*tile = &game->tiles_wall_bottom;
		}
			break;
		case TILE_STONEFLOOR:
		{
			*tile = &game->tiles_stonefloor;
		}
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

void room_setup_extra_data(int room_index, int tile_index)
{
	struct room_tile* tile = &game->rooms[room_index].tiles[tile_index];

	for (int i = 0; i < 8; i++)
	{
		switch (tile->extra_data[i])
		{
		case EXTRA_DATA_MONSTER:
		{
			float x = *((float*)&tile->extra_data[i + 1]);
			float y = *((float*)&tile->extra_data[i + 2]);
			i += 2;

			room_add_monster(room_index, x, y);
		}
			break;
		}
	}
}

void rooms_load(struct game* game)
{
	FILE *fp;
	fp = fopen(ROOMS_FILE_PATH, "rb");

	for (int i = 0; i < NUM_ROOMS; i++)
	{
		for (int j = 0; j < ROOM_SIZE; j++)
		{
			enum tile_type type_back;
			enum tile_type type_front;

			fread(&type_back, sizeof(int), 1, fp);
			fread(&type_front, sizeof(int), 1, fp);

			fread(game->rooms[i].tiles[j].extra_data, sizeof(int), 8, fp);

			room_setup_tile(game, i, j, type_back, 1);
			room_setup_tile(game, i, j, type_front, 0);
			room_setup_extra_data(i, j);
		}
	}
	fclose(fp);
}

void rooms_init(struct game* game)
{
	for (int i = 0; i < NUM_ROOMS; i++)
	{
		game->rooms[i].num_monsters = 0;

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
void player_set_arms(int hold);

void player_try_pickup_drop()
{
	// Check if pickup or drop
	if (key_pressed(GLFW_KEY_Z))
	{
		game->player.holding_item = game->player.holding_item ? 0 : 1;
		player_set_arms(game->player.holding_item);
	}
}

void player_die(struct game* game, float dt)
{
	animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_die);
}

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

	player_try_pickup_drop();

	// Set correct idle animation
	switch (game->player.dir)
	{
		case DIR_LEFT:
		{
			animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_idle_left);
			animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_idle_left);
		}
		break;
		case DIR_RIGHT:
		{
			animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_idle_right);
			animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_idle_right);
		}
		break;
		case DIR_UP:
		{
			animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_idle_up);
			animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_idle_up);
		}
		break;
		case DIR_DOWN:
		{
			animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_idle_down);
			animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_idle_down);
		}
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

	player_try_pickup_drop();

	// Move player and set correct walking animation
	switch (game->player.dir)
	{
	case DIR_LEFT:
	{
		animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_walk_left);
		animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_walk_left);
	}
		break;
	case DIR_RIGHT:
	{
		animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_walk_right);
		animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_walk_right);
	}
		break;
	case DIR_UP:
	{
		animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_walk_up);
		animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_walk_up);
	}
		break;
	case DIR_DOWN:
	{
		animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_walk_down);
		animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_walk_down);
	}
		break;
	}

	set2f(game->debug_pos.position, game->player.sprite.position[0], game->player.sprite.position[1] - 8.0f);

	// X check
	if (!is_tile_collision(game, pos[0] - 4.0f, game->player.sprite.position[1] - 6.0f) &&
		!is_tile_collision(game, pos[0] + 4.0f, game->player.sprite.position[1] - 6.0f) &&
		!is_tile_collision(game, pos[0] - 4.0f, game->player.sprite.position[1] - 8.0f) &&
		!is_tile_collision(game, pos[0] + 4.0f, game->player.sprite.position[1] - 8.0f) )
	{
		set2f(game->player.sprite.position, pos[0], game->player.sprite.position[1]);
	}
	
	// Y check
	if (!is_tile_collision(game, game->player.sprite.position[0] - 4.0f, pos[1] - 6.0f) &&
		!is_tile_collision(game, game->player.sprite.position[0] + 4.0f, pos[1] - 6.0f) &&
		!is_tile_collision(game, game->player.sprite.position[0] - 4.0f, pos[1] - 8.0f) &&
		!is_tile_collision(game, game->player.sprite.position[0] + 4.0f, pos[1] - 8.0f))
	{
		set2f(game->player.sprite.position, game->player.sprite.position[0], pos[1]);
	}
}

void player_set_arms(int hold)
{
	if (hold)
	{
		animatedsprites_setanim(&game->player.anim_arms_idle_left, 1, atlas_frame_index(&game->atlas, "arms_left_hold"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_idle_right, 1, atlas_frame_index(&game->atlas, "arms_right_hold"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_idle_up, 1, atlas_frame_index(&game->atlas, "arms_up_hold"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_idle_down, 1, atlas_frame_index(&game->atlas, "arms_down_hold"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_left, 1, atlas_frame_index(&game->atlas, "arms_left_hold"), 2, 150.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_right, 1, atlas_frame_index(&game->atlas, "arms_right_hold"), 2, 150.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_up, 1, atlas_frame_index(&game->atlas, "arms_up_hold"), 2, 150.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_down, 1, atlas_frame_index(&game->atlas, "arms_down_hold"), 2, 150.0f);
	}
	else
	{
		animatedsprites_setanim(&game->player.anim_arms_idle_left, 1, atlas_frame_index(&game->atlas, "arms_left"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_idle_right, 1, atlas_frame_index(&game->atlas, "arms_right"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_idle_up, 1, atlas_frame_index(&game->atlas, "arms_up"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_idle_down, 1, atlas_frame_index(&game->atlas, "arms_down"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_left, 1, atlas_frame_index(&game->atlas, "arms_left"), 2, 150.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_right, 1, atlas_frame_index(&game->atlas, "arms_right"), 2, 150.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_up, 1, atlas_frame_index(&game->atlas, "arms_up"), 2, 150.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_down, 1, atlas_frame_index(&game->atlas, "arms_down"), 2, 150.0f);
	}

	game->player.sprite.anim = 0;
	game->player.sprite_arms.anim = 0;
}

void player_init(struct game* game)
{
	set3f(game->player.sprite.position, -69.0f, -12.0f, 0);
	set3f(game->camera, -69.0f, -12.0f, 0);

	set2f(game->player.sprite.scale, 1.3f, 1.3f);
	set2f(game->player.sprite_arms.scale, 1.3f, 1.3f);

	game->player.speed = 0.04f;
	game->player.state = &player_idle;
	game->player.dir = DIR_DOWN;
	game->player.holding_item = 0;
	game->player.alive = 1;

	animatedsprites_setanim(&game->player.anim_idle_left, 1, 0, 2, 700.0f);
	animatedsprites_setanim(&game->player.anim_idle_right, 1, 2, 2, 700.0f);
	animatedsprites_setanim(&game->player.anim_idle_up, 1, 4, 2, 700.0f);
	animatedsprites_setanim(&game->player.anim_idle_down, 1, 6, 2, 700.0f);
	animatedsprites_setanim(&game->player.anim_walk_left, 1, 8, 2, 150.0f);
	animatedsprites_setanim(&game->player.anim_walk_right, 1, 10, 2, 150.0f);
	animatedsprites_setanim(&game->player.anim_walk_up, 1, 12, 2, 150.0f);
	animatedsprites_setanim(&game->player.anim_walk_down, 1, 14, 2, 150.0f);
	animatedsprites_setanim(&game->player.anim_die, 0, atlas_frame_index(&game->atlas, "player_die"), 8, 40.0f);

	player_set_arms(0);

	animatedsprites_playanimation(&game->debug_pos, &game->anim_debug);

	animatedsprites_add(game->batcher, &game->player.sprite);
	animatedsprites_add(game->batcher, &game->player.sprite_arms);

	//animatedsprites_add(game->batcher, &game->debug_pos);
}

void player_think(struct game* game, float dt)
{
	if (key_pressed(GLFW_KEY_P))
	{
		game->player.alive = 0;
	}

	if (!game->player.alive)
	{
		game->player.state = &player_die;
		game->player.sprite_arms.position[0] = -99999.0f;

		// TODO: Check for restart
		if (key_pressed(GLFW_KEY_ENTER))
		{
			game_init();
		}
	}
	else
	{
		set2f(game->player.sprite_arms.position, game->player.sprite.position[0], game->player.sprite.position[1]);
	}

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

struct room_tile* get_room_tile_at(float x, float y, int tile_size, int grid_x_max, int grid_y_max, int* tile_index_out)
{
	int grid_x = (int)floor(x / (float)tile_size);
	int grid_y = (int)floor(y / (float)tile_size);

	if (grid_x < 0 || grid_y < 0 || grid_x >= grid_x_max || grid_y >= grid_y_max)
	{
		return 0;
	}

	*tile_index_out = grid_y * grid_x_max + grid_x;
	return &game->rooms[game->room_index].tiles[*tile_index_out];
}

void room_edit_place_back(float x, float y)
{
	int index = 0;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);
	room_setup_tile(game, game->room_index, index, game->edit_current_type, 1);
}

void room_edit_place_front(float x, float y)
{
	int index = 0;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);
	room_setup_tile(game, game->room_index, index, game->edit_current_type, 0);
}

void room_edit_place_monster(float x, float y)
{
	int index = 0;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);

	vec2 offset;
	set2f(offset, x - VIEW_WIDTH / 2, y - VIEW_HEIGHT / 2);

	tile->extra_data[0] = EXTRA_DATA_MONSTER;
	*((float*)&tile->extra_data[1]) = offset[0];
	*((float*)&tile->extra_data[2]) = offset[1];

	room_add_monster(game->room_index, offset[0], offset[1]);
}

void room_edit(float dt)
{
	int left_mouse = glfwGetMouseButton(core_global->graphics.window, GLFW_MOUSE_BUTTON_LEFT);
	int right_mouse = glfwGetMouseButton(core_global->graphics.window, GLFW_MOUSE_BUTTON_RIGHT);

	float x = 0, y = 0;
	input_view_get_cursor(core_global->graphics.window, &x, &y);

	x += game->camera[0];
	y += game->camera[1];

	if (left_mouse == GLFW_PRESS)
	{
		room_edit_place_back(x, y);
	}
	if (right_mouse == GLFW_PRESS)
	{
		room_edit_place_front(x, y);
	}
	if (key_pressed(GLFW_KEY_KP_ADD))
	{
		room_edit_place_monster(x, y);
	}

	// Room save/load logic
	if (key_pressed(GLFW_KEY_F9))
	{
		rooms_save(game);
	}

	if (key_pressed(GLFW_KEY_F4))
	{
		game->room_index = 0;
	}
	if (key_pressed(GLFW_KEY_F5))
	{
		game->room_index = 1;
	}
	if (key_pressed(GLFW_KEY_F6))
	{
		game->room_index = 2;
	}
	if (key_pressed(GLFW_KEY_F7))
	{
		game->room_index = 3;
	}
	if (key_pressed(GLFW_KEY_F8))
	{
		game->room_index = 4;
	}

	if (key_pressed(GLFW_KEY_KP_0))
	{
		game->edit_current_type = TILE_NONE;
	}
	if (key_pressed(GLFW_KEY_KP_1))
	{
		game->edit_current_type = TILE_WALL_TOP;
	}
	if (key_pressed(GLFW_KEY_KP_2))
	{
		game->edit_current_type = TILE_WALL_MID;
	}
	if (key_pressed(GLFW_KEY_KP_3))
	{
		game->edit_current_type = TILE_WALL_BOTTOM;
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
		game->edit_current_type = TILE_STONEFLOOR;
	}
	if (key_pressed(GLFW_KEY_KP_8))
	{

	}
	if (key_pressed(GLFW_KEY_KP_9))
	{

	}
}

void shader_think(struct game* game)
{

}

void monsters_think(float dt)
{
	struct room* room = &game->rooms[game->room_index];

	for (int i = 0; i < room->num_monsters; i++)
	{
		room->monsters[i].state(&room->monsters[i], dt);
	}
}

void monsters_init()
{
	struct room* room = &game->rooms[game->room_index];

	for (int i = 0; i < room->num_monsters; i++)
	{
		room->monsters[i].state = &monster_hunt;
	}
}

int sort_y(GLfloat* buffer_data_a, GLfloat* buffer_data_b)
{
	return buffer_data_b[1] - buffer_data_a[1];
}

void game_think(struct core *core, struct graphics *g, float dt)
{
	room_edit(dt);

	vec2 offset;
	set2f(offset, 0.0f, 0.0f);

	player_think(game, dt);
	monsters_think(dt);

	float dx = (game->player.sprite.position[0] - game->camera[0]) / 100.0f;
	float dy = (game->player.sprite.position[1] - game->camera[1]) / 100.0f;
	
	game->camera[0] += dx;
	game->camera[1] += dy;

	animatedsprites_update(game->batcher, &game->atlas, dt);
	animatedsprites_update(game->batcher_statics, &game->atlas, dt);

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

	animatedsprites_sort(game->batcher, sort_y);;

	tiles_render(&game->tiles_back, &assets->shaders.basic_shader, g, assets->textures.textures, transform);
	animatedsprites_render(game->batcher, &assets->shaders.basic_shader, g, assets->textures.textures, final);
	tiles_render(&game->tiles_front, &assets->shaders.basic_shader, g, assets->textures.textures, transform);

	vec4 c;
	set4f(c, 1.0f, 1.0f, 1.0f, 1.0f);
	drawable_render(&game->drawable_player, &assets->shaders.basic_shader, g, &core->textures.none, c, final);
	drawable_render(&game->drawable_monster, &assets->shaders.basic_shader, g, &core->textures.none, c, final);

	animatedsprites_render(game->batcher_statics, &assets->shaders.basic_shader, g, assets->textures.killscreen, transform);
}

void game_mousebutton_callback(struct core *core, GLFWwindow *window, int button, int action, int mods)
{
	if (action == GLFW_PRESS) {
		float x = 0, y = 0;
		input_view_get_cursor(window, &x, &y);
		
		x += game->camera[0];
		y += game->camera[1];

		console_debug("Click at %.0fx%.0f\n", x, y);
	}
}

void game_console_init(struct console *c)
{
	/* console_env_bind_1f(c, "print_fps", &(game->print_fps)); */
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
	game->batcher_statics = animatedsprites_create();

	animatedsprites_setanim(&game->tiles_grass, 1, 16, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_grain, 1, 17, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_wood, 1, 18, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_cobble, 1, 19, 1, 100.0f);

	animatedsprites_setanim(&game->tiles_wall_top, 1, 21, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_wall_mid, 20, 22, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_wall_bottom, 1, 23, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_stonefloor, 1, 24, 1, 100.0f);
	animatedsprites_setanim(&game->anim_debug, 1, 25, 1, 100.0f);

	animatedsprites_setanim(&game->anim_empty, 0, atlas_frame_index(&game->atlas, "anim_empty"), 1, 100.0f);

	tiles_init(&game->tiles_back, &tiles_get_back_data_at, 16, VIEW_WIDTH, VIEW_HEIGHT, 16, 16);
	tiles_init(&game->tiles_front, &tiles_get_front_data_at, 16, VIEW_WIDTH, VIEW_HEIGHT, 16, 16);

	game->room_index = 0;
	game->edit_current_type = TILE_NONE;

	player_init(game);

	set2f(game->sprite_killscreen.scale, 1.0f, 1.0f);
	set2f(game->sprite_killscreen.position, 0.0f, 0.0f);
	animatedsprites_add(game->batcher_statics, &game->sprite_killscreen);

	set2f(game->debug_pos.scale, 0.2f, 0.2f);
	set2f(game->debug_pos.position, game->player.sprite.position[0], game->player.sprite.position[1]);

	rooms_init(game);
	rooms_load(game);

	monsters_init();
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
