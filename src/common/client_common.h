#include "common.h" 
#include <pthread.h>

extern Player players[MAX_CLIENTS];
extern Player players_last[MAX_CLIENTS];
extern PlayerStaticData player_data[MAX_CLIENTS];
extern char is_update_locked;
extern char is_player_initialized;

extern Player local_player;
//extern Player* local_player;
extern PlayerStaticData local_player_data;

#define BULLETS_DEFAULT_CAPACITY 256
extern Bullet* bullets;
extern Bullet* bullets_render;
extern Bullet* bullets_last;
extern int existing_bullets;
extern int bullet_capacity;

extern void load_player_sprite(int sprite_index);
extern char should_update_sprites;

extern float shoot_timer;
extern float respawn_timer;
extern char is_dead;
#define SHOOT_COOLDOWN 0.2f
#define RESPAWN_COOLDOWN 3.0f

extern pthread_mutex_t client_bullets_mutex;

#include <stdatomic.h>
extern _Atomic(Bullet*) atomic_bullets_net;     // Network thread writes here
extern  _Atomic(Bullet*) atomic_bullets_render;  // Render thread reads here
extern  _Atomic size_t atomic_existing_bullets;