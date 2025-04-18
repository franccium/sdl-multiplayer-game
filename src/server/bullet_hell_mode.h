#pragma once
#include "common/common.h"

#define MAX_VORTEX_SPAWNERS 3

typedef struct VortexSpawner {
    float x;
    float y;
    float angle;
} VortexSpawner;

void init_vortex();
void add_vortex();
void remove_vortex();
void spawn();