#pragma once



//NOTE: may need to find an optimal delay, or interpolate or sth if needed for performance, above 20 is too much
#define GAME_STATE_UPDATE_FRAME_DELAY 10

#define USE_HL_LERP 0

#define PRINT_SHADER_COMPILATION_INFO 0

#define PLAYER_Z_COORD_MIN 0.1f
#define PLAYER_Z_COORD_MULTIPLIER 0.1f
#define BULLET_Z_COORD_MIN PLAYER_Z_COORD_MULTIPLIER*(MAX_CLIENTS+2) // render above all players
#define BULLET_Z_COORD_MULTIPLIER 0.001f

#define PLAYER_COLLISION_MIN 0.95f
#define BULLET_COLLISION_MIN 0.93f