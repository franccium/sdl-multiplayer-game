#include "common.h" 

extern Player players[MAX_CLIENTS];
extern Player players_last[MAX_CLIENTS];
extern PlayerStaticData player_data[MAX_CLIENTS];
extern char is_update_locked;
extern char is_player_initialized;

extern Player local_player;
extern PlayerStaticData local_player_data;

void load_player_sprite(int sprite_index);