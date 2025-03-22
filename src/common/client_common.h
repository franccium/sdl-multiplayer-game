#pragma once

#include "common.h" 

Player players[MAX_CLIENTS];
PlayerStaticData player_data[MAX_CLIENTS];

static Player local_player = {INVALID_PLAYER_ID, 100, 100};
static PlayerStaticData local_player_data = {INVALID_PLAYER_ID, 0};
