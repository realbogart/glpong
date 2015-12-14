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
#include "sound.h"

#define VIEW_WIDTH      256
#define VIEW_HEIGHT		107

#define ROOM_SIZE		256
#define NUM_ROOMS		5
#define MAX_MONSTERS	1024
#define MAX_ITEMS		16
#define MAX_PROJECTILES	32

#define ROOMS_FILE_PATH	"C:/programmering/ld34/glpong/ld34/assets/allrooms.world"
//#define ROOMS_FILE_PATH	"assets/allrooms.world"

struct game_settings settings = {
	.view_width			= VIEW_WIDTH,
	.view_height		= VIEW_HEIGHT,
	.window_width		= VIEW_WIDTH,
	.window_height		= VIEW_HEIGHT,
	.window_title		= "ld34",
	.sound_listener		= { 0.0f, 0.0f, 0.0f },
	.sound_distance_max = 500.0f, // distance3f(vec3(0), sound_listener)
};

typedef void(*player_state_t)(float);
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
	TILE_STONEFLOOR,
	TILE_CLEAR_EXTRA,
	TILE_SWITCH_ROOM,
	TILE_KEYHOLE,
	TILE_WIN
};

enum extra_data
{
	EXTRA_DATA_NONE = 0,
	EXTRA_DATA_MONSTER,
	EXTRA_DATA_ITEM,
	EXTRA_DATA_DOOR
};

struct monster
{
	struct sprite	sprite;
	
	enum direction	dir;

	monster_state_t	state;

	float			speed;
	int				alive;

	struct anim		anim_walk_left;
	struct anim		anim_walk_right;
	struct anim		anim_walk_up;
	struct anim		anim_walk_down;
	struct anim		anim_die;
};

enum item_type
{
	ITEM_KEY = 0,
	ITEM_WEAPON
};

struct item
{
	enum item_type	type;
	struct sprite	sprite;
	struct anim		anim;
	int	room_id;
};

struct player{
	struct sprite			sprite;
	struct sprite			sprite_arms;
	struct sprite			sprite_item;

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

	struct anim				anim_item_left;
	struct anim				anim_item_right;
	struct anim				anim_item_up;
	struct anim				anim_item_down;

	struct anim				anim_item;

	struct anim				anim_die;

	int						alive;
	struct item*			item;

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

struct door
{
	int to_room_index;
	float x;
	float y;
};

struct projectile
{
	struct sprite sprite;
	struct anim anim;
	enum direction direction;
};

struct world_items
{
	int			num_items;
	struct item	items[MAX_ITEMS];
	struct projectile	projectiles[MAX_PROJECTILES];
	int					current_projectile;
} world_items;

struct game {
	struct basic_sprite		sprite_killscreen;
	struct basic_sprite		sprite_win;

	struct anim				anim_debug;

	struct atlas			atlas;
	struct animatedsprites	*batcher;
	struct animatedsprites	*batcher_statics;

	struct drawable			drawable_monster;
	struct drawable			drawable_player;

	struct tiles			tiles_back;
	struct tiles			tiles_front;

	struct player			player;

	vec3					sound_position;
	vec2					camera;

	struct room				rooms[NUM_ROOMS];
	struct door				doors[16];

	int						room_index;

	enum tile_type			edit_current_type;
	float					edit_current_door;

	int						win;

	struct anim anim_empty;

	struct anim tiles_grass;
	struct anim tiles_cobble;
	struct anim tiles_grain;
	struct anim tiles_wood;
	struct anim tiles_stonefloor;
	struct anim tiles_keyhole;
	struct anim tiles_win;
	struct anim tiles_wall_top;
	struct anim tiles_wall_mid;
	struct anim tiles_wall_bottom;
	struct anim tiles_switch_room;

} *game = NULL;

struct room_tile* get_room_tile_at(float x, float y, int tile_size, int grid_x_max, int grid_y_max, int* tile_index_out);
void room_edit_place_front(float x, float y);
void room_edit_place_back(float x, float y);
void refresh_sprites();

int is_tile_collision(float x, float y)
{
	x += VIEW_WIDTH / 2;
	y += VIEW_HEIGHT / 2;

	int index = -1;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);

	if (index != -1)
	{
		if (tile->type_back != TILE_STONEFLOOR && tile->type_back != TILE_SWITCH_ROOM && tile->type_back != TILE_WIN)
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

int is_switch_room(float x, float y, int* door_id_out)
{
	x += VIEW_WIDTH / 2;
	y += VIEW_HEIGHT / 2;

	int index = -1;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);

	if (index != -1)
	{
		if (tile->type_back == TILE_SWITCH_ROOM)
		{
			*door_id_out = tile->extra_data[1];
			return 1;
		}
		else
		{
			return 0;
		}
	}

	return 0;
}

void player_walk(float dt);
void player_set_arms();

void try_unlock(float x, float y)
{
	x += VIEW_WIDTH / 2;
	y += VIEW_HEIGHT / 2;

	int index = -1;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);

	if (index != -1)
	{
		if (tile->type_back == TILE_KEYHOLE)
		{
			if (game->player.item && game->player.item->type == ITEM_KEY)
			{
				tile->tile_back = &game->tiles_stonefloor;
				tile->type_back = TILE_STONEFLOOR;
				game->player.item = 0;
				player_set_arms();

				sound_buf_play(&core_global->sound, assets->sounds.unlock, game->sound_position);
			}
		}
	}
}

void player_win(float dt)
{

}

void try_win(float x, float y)
{
	x += VIEW_WIDTH / 2;
	y += VIEW_HEIGHT / 2;

	int index = -1;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);

	if (index != -1)
	{
		if (tile->type_back == TILE_WIN)
		{
			game->player.state = &player_win;
			game->win = 1;
		}
	}
}

void monster_die(struct monster* monster, float dt)
{
	animatedsprites_switchanimation(&monster->sprite, &monster->anim_die);
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

	if (is_tile_collision(check_forward[0], check_forward[1]))
	{
		int r = !is_tile_collision(check_right[0], check_right[1]);
		int l = !is_tile_collision(check_left[0], check_left[1]);

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
		else if (!is_tile_collision(check_back[0], check_back[1]))
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
		set3f(monster->sprite.position, pos[0], pos[1], 0.3f);
	}

	// Check for player collision
	if (collide_circlef(monster->sprite.position[0], monster->sprite.position[1], 6.0f, 
						game->player.sprite.position[0], game->player.sprite.position[1] - 6.0f, 6.0f))
	{
		if (game->player.alive)
		{
			sound_buf_play(&core_global->sound, assets->sounds.die, game->sound_position);
		}
			
		game->player.alive = 0;
	}
}

void game_add_item(enum item_type type, int room_id, float x, float y)
{
	if (world_items.num_items >= MAX_ITEMS)
	{
		return;
	}

	struct item* item = &world_items.items[world_items.num_items];
	
	item->type = type;
	item->room_id = room_id;
	set3f(item->sprite.position, x, y, 0.2f);
	set2f(item->sprite.scale, 0.3f, 0.3f);

	world_items.num_items++;

	switch (type)
	{
		case ITEM_KEY:
		{
			animatedsprites_setanim(&item->anim, 1, atlas_frame_index(&game->atlas, "item_key"), 2, 100.0f);
			animatedsprites_playanimation(&item->sprite, &item->anim);
		}
		break;
		case ITEM_WEAPON:
		{
			animatedsprites_setanim(&item->anim, 1, atlas_frame_index(&game->atlas, "item_weapon"), 2, 100.0f);
			animatedsprites_playanimation(&item->sprite, &item->anim);
		}
		break;
		default:
		{
			animatedsprites_playanimation(&item->sprite, &game->anim_empty);
		}
		break;
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

	set3f(monster->sprite.position, x, y, 0.3f);
	set2f(monster->sprite.scale, 0.8f, 0.8f);

	monster->dir = DIR_LEFT;
	monster->speed = 0.08f;
	monster->state = &monster_hunt;
	monster->alive = 1;

	room->num_monsters++;

	animatedsprites_setanim(&monster->anim_walk_left, 1, atlas_frame_index(&game->atlas, "monster_walk_left"), 2, 100.0f);
	animatedsprites_setanim(&monster->anim_walk_right, 1, atlas_frame_index(&game->atlas, "monster_walk_right"), 2, 100.0f);
	animatedsprites_setanim(&monster->anim_walk_up, 1, atlas_frame_index(&game->atlas, "monster_walk_up"), 2, 100.0f);
	animatedsprites_setanim(&monster->anim_walk_down, 1, atlas_frame_index(&game->atlas, "monster_walk_down"), 2, 100.0f);
	animatedsprites_setanim(&monster->anim_die, 0, atlas_frame_index(&game->atlas, "monster_die"), 5, 40.0f);
}

void room_setup_tile(int room_index, int tile_index, enum tile_type type, int back)
{
	struct anim** tile = back ? &game->rooms[room_index].tiles[tile_index].tile_back : 
								&game->rooms[room_index].tiles[tile_index].tile_front;

	if (type != TILE_CLEAR_EXTRA)
	{
		if (back)
		{
			game->rooms[room_index].tiles[tile_index].type_back = type;
		}
		else
		{
			game->rooms[room_index].tiles[tile_index].type_front = type;
		}
	}
	else
	{
		for (int i = 0; i < 8; i++)
		{
			game->rooms[room_index].tiles[tile_index].extra_data[i] = 0;
		}
	}

	switch (type)
	{
		case TILE_NONE:
		{
			*tile = 0;
		}
		break;
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
		case TILE_KEYHOLE:
		{
			*tile = &game->tiles_keyhole;
		}
			break;
		case TILE_WIN:
		{
			*tile = &game->tiles_win;
		}
			break;
		case TILE_SWITCH_ROOM:
		{
			*tile = &game->tiles_switch_room;
		}
			break;
	}
}

void rooms_save()
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
		case EXTRA_DATA_ITEM:
		{
			enum item_type type = tile->extra_data[i + 1];
			float x = *((float*)&tile->extra_data[i + 2]);
			float y = *((float*)&tile->extra_data[i + 3]);
			i += 3;

			game_add_item(type, room_index, x, y);
		}
			break;
		}
	}
}

void rooms_load()
{
	FILE *fp;
	fp = stb_fopen(ROOMS_FILE_PATH, "rb");

	for (int i = 0; i < NUM_ROOMS; i++)
	{
		for (int j = 0; j < ROOM_SIZE; j++)
		{
			enum tile_type type_back;
			enum tile_type type_front;

			fread(&type_back, sizeof(int), 1, fp);
			fread(&type_front, sizeof(int), 1, fp);

			fread(game->rooms[i].tiles[j].extra_data, sizeof(int), 8, fp);

			room_setup_tile(i, j, type_back, 1);
			room_setup_tile(i, j, type_front, 0);
			room_setup_extra_data(i, j);
		}
	}
	fclose(fp);
}

void rooms_init()
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

struct item* player_pickup_item()
{
	for (int i = 0; i < world_items.num_items; i++)
	{
		struct item* item = &world_items.items[i];

		if (collide_circlef(item->sprite.position[0], item->sprite.position[1], 6.0f,
			game->player.sprite.position[0], game->player.sprite.position[1] - 6.0f, 6.0f))
		{
			item->sprite.position[0] = -99999.0f;
			return item;
		}
	}

	return 0;
}

void player_drop_item(struct item* item)
{
	set2f(item->sprite.scale, 0.3f, 0.3f);
	set2f(item->sprite.position, game->player.sprite.position[0], game->player.sprite.position[1] - 6.0f);
	item->room_id = game->room_index;
	refresh_sprites();
}

void player_try_pickup_drop()
{
	// Check if pickup or drop
	if (key_pressed(GLFW_KEY_Z))
	{
		if (game->player.item)
		{
			player_drop_item(game->player.item);
			sound_buf_play(&core_global->sound, assets->sounds.drop, game->sound_position);
			game->player.item = 0;
		}
		else
		{
			struct item* item = player_pickup_item();

			if (item)
			{
				sound_buf_play(&core_global->sound, assets->sounds.pickup, game->sound_position);
				game->player.item = item;
			}
		}

		player_set_arms();
	}
}

void player_use_item()
{
	struct item* item = game->player.item;

	if (game->player.item)
	{
		if (item->type == ITEM_WEAPON)
		{
			vec2 offset;
			if (game->player.dir == DIR_LEFT)
			{
				set2f(offset, game->player.sprite.position[0] - 8.0f, game->player.sprite.position[1] + 2.0f);
			}
			else if (game->player.dir == DIR_RIGHT)
			{
				set2f(offset, game->player.sprite.position[0] + 8.0f, game->player.sprite.position[1] + 2.0f);
			}
			else if (game->player.dir == DIR_UP)
			{
				set2f(offset, game->player.sprite.position[0] + 3.0f, game->player.sprite.position[1] + 8.0f);
			}
			else if (game->player.dir == DIR_DOWN)
			{
				set2f(offset, game->player.sprite.position[0] - 3.0f, game->player.sprite.position[1] - 2.0f);
			}

			struct projectile* projectile = &world_items.projectiles[world_items.current_projectile];
			set2f(projectile->sprite.position, offset[0], offset[1]);
			projectile->direction = game->player.dir;

			sound_buf_play(&core_global->sound, assets->sounds.laser, game->sound_position);

			world_items.current_projectile++;

			if (world_items.current_projectile >= MAX_PROJECTILES)
			{
				world_items.current_projectile = 0;
			}
		}
	}
}

void player_die(float dt)
{
	animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_die);
}

void refresh_sprites()
{
	animatedsprites_clear(game->batcher);

	for (int i = 0; i < world_items.num_items; i++)
	{
		if (world_items.items[i].room_id == game->room_index)
		{
			animatedsprites_add(game->batcher, &world_items.items[i].sprite);
		}
	}

	animatedsprites_add(game->batcher, &game->player.sprite);
	animatedsprites_add(game->batcher, &game->player.sprite_arms);
	animatedsprites_add(game->batcher, &game->player.sprite_item);

	for (int i = 0; i < game->rooms[game->room_index].num_monsters; i++)
	{
		animatedsprites_add(game->batcher, &game->rooms[game->room_index].monsters[i].sprite);
	}

	for (int i = 0; i < MAX_PROJECTILES; i++)
	{
		animatedsprites_add(game->batcher, &world_items.projectiles[i].sprite);
	}
}

void switch_room(int room_index, float x, float y)
{
	game->room_index = room_index;
	set2f(game->player.sprite.position, x, y);
	set2f(game->camera, x, y);
	refresh_sprites();
}

void player_idle(float dt)
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

	if (key_pressed(GLFW_KEY_X))
	{
		player_use_item();
	}

	player_try_pickup_drop();

	// Set correct idle animation
	switch (game->player.dir)
	{
		case DIR_LEFT:
		{
			animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_idle_left);
			animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_idle_left);
			animatedsprites_switchanimation(&game->player.sprite_item, &game->player.anim_item_left);
		}
		break;
		case DIR_RIGHT:
		{
			animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_idle_right);
			animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_idle_right);
			animatedsprites_switchanimation(&game->player.sprite_item, &game->player.anim_item_right);
		}
		break;
		case DIR_UP:
		{
			animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_idle_up);
			animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_idle_up);
			animatedsprites_switchanimation(&game->player.sprite_item, &game->player.anim_item_up);
		}
		break;
		case DIR_DOWN:
		{
			animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_idle_down);
			animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_idle_down);
			animatedsprites_switchanimation(&game->player.sprite_item, &game->player.anim_item_down);
		}
		break;
	}
}

void player_walk(float dt)
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

	if (key_pressed(GLFW_KEY_X))
	{
		player_use_item();
	}

	player_try_pickup_drop();

	// Move player and set correct walking animation
	switch (game->player.dir)
	{
	case DIR_LEFT:
	{
		animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_walk_left);
		animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_walk_left);
		animatedsprites_switchanimation(&game->player.sprite_item, &game->player.anim_item_left);
	}
		break;
	case DIR_RIGHT:
	{
		animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_walk_right);
		animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_walk_right);
		animatedsprites_switchanimation(&game->player.sprite_item, &game->player.anim_item_right);
	}
		break;
	case DIR_UP:
	{
		animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_walk_up);
		animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_walk_up);
		animatedsprites_switchanimation(&game->player.sprite_item, &game->player.anim_item_up);
	}
		break;
	case DIR_DOWN:
	{
		animatedsprites_switchanimation(&game->player.sprite, &game->player.anim_walk_down);
		animatedsprites_switchanimation(&game->player.sprite_arms, &game->player.anim_arms_walk_down);
		animatedsprites_switchanimation(&game->player.sprite_item, &game->player.anim_item_down);
	}
		break;
	}

	try_win(pos[0] - 4.0f, game->player.sprite.position[1] - 6.0f);

	try_unlock(pos[0] - 4.0f, game->player.sprite.position[1] - 6.0f);
	try_unlock(pos[0] + 4.0f, game->player.sprite.position[1] - 6.0f);
	try_unlock(pos[0] - 4.0f, game->player.sprite.position[1] - 8.0);
	try_unlock(pos[0] + 4.0f, game->player.sprite.position[1] - 8.0f);

	// X check
	if (!is_tile_collision(pos[0] - 4.0f, game->player.sprite.position[1] - 6.0f) &&
		!is_tile_collision(pos[0] + 4.0f, game->player.sprite.position[1] - 6.0f) &&
		!is_tile_collision(pos[0] - 4.0f, game->player.sprite.position[1] - 8.0f) &&
		!is_tile_collision(pos[0] + 4.0f, game->player.sprite.position[1] - 8.0f) )
	{
		set2f(game->player.sprite.position, pos[0], game->player.sprite.position[1]);
	}
	
	// Y check
	if (!is_tile_collision(game->player.sprite.position[0] - 4.0f, pos[1] - 6.0f) &&
		!is_tile_collision(game->player.sprite.position[0] + 4.0f, pos[1] - 6.0f) &&
		!is_tile_collision(game->player.sprite.position[0] - 4.0f, pos[1] - 8.0f) &&
		!is_tile_collision(game->player.sprite.position[0] + 4.0f, pos[1] - 8.0f))
	{
		set2f(game->player.sprite.position, game->player.sprite.position[0], pos[1]);
	}

	int door_id = -1;
	if (is_switch_room(game->player.sprite.position[0], game->player.sprite.position[1] - 6.0f, &door_id))
	{
		struct door* door = &game->doors[door_id];
		switch_room(door->to_room_index, door->x, door->y);
	}
}

void player_set_arms()
{
	if (game->player.item)
	{
		animatedsprites_setanim(&game->player.anim_arms_idle_left, 1, atlas_frame_index(&game->atlas, "arms_left_hold"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_idle_right, 1, atlas_frame_index(&game->atlas, "arms_right_hold"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_idle_up, 1, atlas_frame_index(&game->atlas, "arms_up_hold"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_idle_down, 1, atlas_frame_index(&game->atlas, "arms_down_hold"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_left, 1, atlas_frame_index(&game->atlas, "arms_left_hold"), 2, 150.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_right, 1, atlas_frame_index(&game->atlas, "arms_right_hold"), 2, 150.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_up, 1, atlas_frame_index(&game->atlas, "arms_up_hold"), 2, 150.0f);
		animatedsprites_setanim(&game->player.anim_arms_walk_down, 1, atlas_frame_index(&game->atlas, "arms_down_hold"), 2, 150.0f);

		if (game->player.item->type == ITEM_KEY)
		{
			animatedsprites_setanim(&game->player.anim_item_left, 1, atlas_frame_index(&game->atlas, "key_left"), 1, 700.0f);
			animatedsprites_setanim(&game->player.anim_item_right, 1, atlas_frame_index(&game->atlas, "key_right"), 1, 700.0f);
			animatedsprites_setanim(&game->player.anim_item_up, 1, atlas_frame_index(&game->atlas, "key_up"), 1, 700.0f);
			animatedsprites_setanim(&game->player.anim_item_down, 1, atlas_frame_index(&game->atlas, "key_down"), 1, 700.0f);
		}
		else if (game->player.item->type == ITEM_WEAPON)
		{
			animatedsprites_setanim(&game->player.anim_item_left, 1, atlas_frame_index(&game->atlas, "weapon_left"), 1, 700.0f);
			animatedsprites_setanim(&game->player.anim_item_right, 1, atlas_frame_index(&game->atlas, "weapon_right"), 1, 700.0f);
			animatedsprites_setanim(&game->player.anim_item_up, 1, atlas_frame_index(&game->atlas, "weapon_up"), 1, 700.0f);
			animatedsprites_setanim(&game->player.anim_item_down, 1, atlas_frame_index(&game->atlas, "weapon_down"), 1, 700.0f);
		}
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

		animatedsprites_setanim(&game->player.anim_item_left, 1, atlas_frame_index(&game->atlas, "empty"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_item_right, 1, atlas_frame_index(&game->atlas, "empty"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_item_up, 1, atlas_frame_index(&game->atlas, "empty"), 2, 700.0f);
		animatedsprites_setanim(&game->player.anim_item_down, 1, atlas_frame_index(&game->atlas, "empty"), 2, 700.0f);
	}

	game->player.sprite.anim = 0;
	game->player.sprite_arms.anim = 0;
	game->player.sprite_item.anim = 0;
}

void player_init()
{
	set2f(game->player.sprite.scale, 1.3f, 1.3f);
	set2f(game->player.sprite_arms.scale, 1.3f, 1.3f);
	set2f(game->player.sprite_item.scale, 1.3f, 1.3f);

	//game->player.sprite_arms.position[2] = 0.8f;
	//game->player.sprite_item.position[2] = 0.9f;

	game->player.speed = 0.04f;
	game->player.state = &player_idle;
	game->player.dir = DIR_DOWN;
	game->player.item = 0;
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

	player_set_arms();
}

void player_think(float dt)
{
	if (key_pressed(GLFW_KEY_P))
	{
		game->player.alive = 0;
	}

	if (!game->player.alive)
	{
		game->player.state = &player_die;
		game->player.sprite_arms.position[0] = -99999.0f;
		game->player.sprite_item.position[0] = -99999.0f;

		// TODO: Check for restart
		if (key_pressed(GLFW_KEY_X))
		{
			game_init();
		}
	}
	else
	{
		set2f(game->player.sprite_arms.position, game->player.sprite.position[0], game->player.sprite.position[1]);
		set2f(game->player.sprite_item.position, game->player.sprite.position[0], game->player.sprite.position[1]);
	}

	game->player.state(dt);
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
	room_setup_tile(game->room_index, index, game->edit_current_type, 1);
}

void room_edit_place_front(float x, float y)
{
	int index = 0;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);
	room_setup_tile(game->room_index, index, game->edit_current_type, 0);
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

void room_edit_place_item(float x, float y, enum item_type type)
{
	int index = 0;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);

	vec2 offset;
	set2f(offset, x - VIEW_WIDTH / 2, y - VIEW_HEIGHT / 2);

	tile->extra_data[0] = EXTRA_DATA_ITEM;
	tile->extra_data[1] = type;
	*((float*)&tile->extra_data[2]) = offset[0];
	*((float*)&tile->extra_data[3]) = offset[1];

	game_add_item(type, game->room_index, offset[0], offset[1]);
}

void room_edit_place_door(float x, float y)
{
	int index = 0;
	struct room_tile* tile = get_room_tile_at(x, y, 16, 16, 16, &index);

	tile->type_back = TILE_SWITCH_ROOM;
	tile->tile_back = &game->tiles_switch_room;
	tile->extra_data[0] = EXTRA_DATA_DOOR;

	struct door* door = &game->doors[(int)game->edit_current_door];
	tile->extra_data[1] = (int)game->edit_current_door;
	*((float*)&tile->extra_data[2]) = door->x;
	*((float*)&tile->extra_data[3]) = door->y;
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

	if (key_pressed(GLFW_KEY_W))
	{
		room_edit_place_item(x, y, ITEM_WEAPON);
	}
	if (key_pressed(GLFW_KEY_K))
	{
		room_edit_place_item(x, y, ITEM_KEY);
	}

	if (key_pressed(GLFW_KEY_INSERT))
	{
		room_edit_place_door(x, y);
	}


	// Room save/load logic
	if (key_pressed(GLFW_KEY_F9))
	{
		rooms_save();
	}

	if (key_pressed(GLFW_KEY_F4))
	{
		switch_room(0, 0.0f, 0.0f);
	}
	if (key_pressed(GLFW_KEY_F5))
	{
		switch_room(1, 0.0f, 0.0f);
	}
	if (key_pressed(GLFW_KEY_F6))
	{
		switch_room(2, 0.0f, 0.0f);
	}
	if (key_pressed(GLFW_KEY_F7))
	{
		switch_room(3, 0.0f, 0.0f);
	}
	if (key_pressed(GLFW_KEY_F8))
	{
		switch_room(4, 0.0f, 0.0f);
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
		game->edit_current_type = TILE_KEYHOLE;
	}
	if (key_pressed(GLFW_KEY_KP_9))
	{
		game->edit_current_type = TILE_CLEAR_EXTRA;
	}
	if (key_pressed(GLFW_KEY_Y))
	{
		game->edit_current_type = TILE_WIN;
	}
}

void projectiles_think(float dt)
{
	for (int i = 0; i < MAX_PROJECTILES; i++)
	{
		struct projectile* projectile = &world_items.projectiles[i];

		float speed = 0.15f;

		switch (projectile->direction)
		{
		case DIR_LEFT:
		{
			projectile->sprite.position[0] -= speed * dt;
		}
			break;
		case DIR_RIGHT:
		{
			projectile->sprite.position[0] += speed * dt;
		}
			break;
		case DIR_UP:
		{
			projectile->sprite.position[1] += speed * dt;
		}
			break;
		case DIR_DOWN:
		{
			projectile->sprite.position[1] -= speed * dt;
		}
			break;
		}

		struct room* room = &game->rooms[game->room_index];

		for (int j = 0; j < room->num_monsters; j++)
		{
			if (room->monsters[j].alive)
			{
				if (collide_circlef(room->monsters[j].sprite.position[0], room->monsters[j].sprite.position[1], 6.0f,
					projectile->sprite.position[0], projectile->sprite.position[1], 1.0f))
				{
					room->monsters[j].alive = 0;

					sound_buf_play(&core_global->sound, assets->sounds.enemy_hit, game->sound_position);

					set3f(projectile->sprite.position, -9999.0f, -9999.0f, 0.2f);
					projectile->direction = DIR_LEFT;
					break;
				}
			}
		}
	}
}

void monsters_think(float dt)
{
	struct room* room = &game->rooms[game->room_index];

	for (int i = 0; i < room->num_monsters; i++)
	{
		if (!room->monsters[i].alive)
		{
			room->monsters[i].state = &monster_die;
		}

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
	if (fabs(buffer_data_b[2] - buffer_data_a[2]) > 0.01f)
	{
		return buffer_data_a[2] - buffer_data_b[2];
	}

	return buffer_data_b[1] - buffer_data_a[1];
}

void game_think(struct core *core, struct graphics *g, float dt)
{
	//room_edit(dt);

	vec2 offset;
	set2f(offset, 0.0f, 0.0f);

	player_think(dt);
	monsters_think(dt);

	lerp2f( game->camera, game->player.sprite.position, 0.02f * dt);

	animatedsprites_update(game->batcher, &game->atlas, dt);
	animatedsprites_update(game->batcher_statics, &game->atlas, dt);

	projectiles_think(dt);

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

	if (!game->player.alive)
	{
		sprite_render(&game->sprite_killscreen, &assets->shaders.basic_shader, g);
	}

	if (game->win)
	{
		sprite_render(&game->sprite_win, &assets->shaders.basic_shader, g);
	}
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
	console_env_bind_1f(c, "room_id", &(game->edit_current_door));
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

void doors_init()
{
	game->doors[0].to_room_index = 1;
	game->doors[0].x = 100.0f;
	game->doors[0].y = 100.0f;

	game->doors[1].to_room_index = 0;
	game->doors[1].x = -96.0f;
	game->doors[1].y = 25.0f;

	game->doors[2].to_room_index = 2;
	game->doors[2].x = -90.0f;
	game->doors[2].y = 160.0f;

	game->doors[3].to_room_index = 0;
	game->doors[3].x = -7.0f;
	game->doors[3].y = -30.0f;

	game->doors[4].to_room_index = 3;
	game->doors[4].x = -100.0f;
	game->doors[4].y = 180.0f;

	game->doors[5].to_room_index = 0;
	game->doors[5].x = 110.0f;
	game->doors[5].y = 75.0f;
}

void projectiles_init()
{
	world_items.current_projectile = 0;

	for (int i = 0; i < MAX_PROJECTILES; i++)
	{
		animatedsprites_setanim(&world_items.projectiles[i].anim, 1, atlas_frame_index(&game->atlas, "projectile"), 1, 10.0f);
		animatedsprites_playanimation(&world_items.projectiles[i].sprite, &world_items.projectiles[i].anim);

		world_items.projectiles[i].direction = DIR_LEFT;
		set2f(world_items.projectiles[i].sprite.scale, 0.6f, 0.6f);
		set3f(world_items.projectiles[i].sprite.position, -9999.0f, -9999.0f, 0.5f);
		animatedsprites_add(game->batcher, &world_items.projectiles[i].sprite);
	}
}

void game_init()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	game->batcher = animatedsprites_create();
	game->batcher_statics = animatedsprites_create();

	game->edit_current_door = 0;
	animatedsprites_setanim(&game->tiles_grass, 1, 16, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_grain, 1, 17, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_wood, 1, 18, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_cobble, 1, 19, 1, 100.0f);

	animatedsprites_setanim(&game->tiles_wall_top, 1, 21, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_wall_mid, 20, 22, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_wall_bottom, 1, 23, 1, 100.0f);
	animatedsprites_setanim(&game->tiles_stonefloor, 1, 24, 1, 100.0f);
	animatedsprites_setanim(&game->anim_debug, 1, 25, 1, 100.0f);

	animatedsprites_setanim(&game->tiles_win, 1, atlas_frame_index(&game->atlas, "win"), 1, 100.0f);
	animatedsprites_setanim(&game->tiles_keyhole, 1, atlas_frame_index(&game->atlas, "keyhole"), 1, 100.0f);
	animatedsprites_setanim(&game->tiles_switch_room, 1, atlas_frame_index(&game->atlas, "switch_room"), 1, 100.0f);
	animatedsprites_setanim(&game->anim_empty, 0, atlas_frame_index(&game->atlas, "empty"), 1, 100.0f);

	tiles_init(&game->tiles_back, &tiles_get_back_data_at, 16, VIEW_WIDTH, VIEW_HEIGHT, 16, 16);
	tiles_init(&game->tiles_front, &tiles_get_front_data_at, 16, VIEW_WIDTH, VIEW_HEIGHT, 16, 16);

	set3f( game->sound_position, 0.0f, 0.0f, 0.0f);
	game->win = 0;
	game->room_index = 0;
	game->edit_current_type = TILE_NONE;
	world_items.num_items = 0;

	player_init();

	vec4 c;
	set4f(c, 1.0f, 1.0f, 1.0f, 1.0f);
	sprite_init(&game->sprite_killscreen, 0, VIEW_WIDTH / 2.0f, VIEW_HEIGHT / 2.0f, 0.0f, 256.0f, 107.0f, c, 0.0f, &assets->textures.killscreen);
	sprite_init(&game->sprite_win, 0, VIEW_WIDTH / 2.0f, VIEW_HEIGHT / 2.0f, 0.0f, 256.0f, 107.0f, c, 0.0f, &assets->textures.win);

	rooms_init();
	rooms_load();

	monsters_init();
	doors_init();
	projectiles_init();

	//struct door* door = &game->doors[2];
	//switch_room(door->to_room_index, door->x, door->y);

	switch_room(0, -69.0f, -12.0f);
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
