#include "common.h" 

extern Player players[MAX_CLIENTS];
extern Player players_last[MAX_CLIENTS];
extern PlayerStaticData player_data[MAX_CLIENTS];
extern char is_update_locked;
extern char is_player_initialized;

extern Player local_player;
extern PlayerStaticData local_player_data;

#define BULLETS_DEFAULT_CAPACITY 20
#define BULLETS_DEFAULT_CAPACITY2 200
extern Bullet* bullets;
extern Bullet* bullets_last;
extern int existing_bullets;
extern int bullet_capacity;

extern void load_player_sprite(int sprite_index);
extern char should_update_sprites;

extern char can_shoot;
extern float shoot_timer;
#define SHOOT_COOLDOWN 0.1f;