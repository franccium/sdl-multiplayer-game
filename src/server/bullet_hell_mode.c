#include "bullet_hell_mode.h"

int spawner_count = 1;
VortexSpawner spawners[MAX_VORTEX_SPAWNERS];
float vortex_angle = 0.0f;
float vortex_angle_step = 0.3f;

void add_bullet(Player* shooter, vec2 direction);

void init_vortex() {
    spawners[0].x = WINDOW_WIDTH / 2;
    spawners[0].y = WINDOW_HEIGHT / 2;
    spawners[0].angle = 0.f;
}

void add_vortex() {
    if(spawner_count >= MAX_VORTEX_SPAWNERS) {
        printf("Max amount of vortex spawners reached!\n");
        return;
    }

    spawners[spawner_count].x = MIN_SPAWN_X + rand() % (WINDOW_WIDTH - MIN_SPAWN_X);
    spawners[spawner_count].y = MIN_SPAWN_Y + rand() % (WINDOW_WIDTH - MIN_SPAWN_Y);
    ++spawner_count;
}

void remove_vortex() {
    if(spawner_count <= 0) {
        printf("There aren't any spawners to remove!\n");
        return;
    }
    --spawner_count;
}

void spawn() {
    printf( "resped");
    for(int i = 0; i < spawner_count; ++i) {
        Player server_shooter = {.x = spawners[i].x, .y = spawners[i].y, .id = 6};
        vec2 dir = {cosf(spawners[i].angle), sinf(spawners[i].angle)};
        add_bullet(&server_shooter, dir);
        spawners[i].angle += vortex_angle_step;
    }
}