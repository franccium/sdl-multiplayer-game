#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client/client.h"
#include "game_window.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define PLAYER_SPRITE_COUNT 5
Player players_interpolated[MAX_CLIENTS];

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

#define PLAYER_SPRITE_WIDTH 128.0f
#define PLAYER_SPRITE_HEIGHT 64.0f


float speed = 0.0f;
float rotation_speed = 0.00005f;
float max_speed = 50.0f;
Uint32 previous_time;

SDL_Texture* player_sprites_map[MAX_CLIENTS];
const char* player_sprite_files[MAX_CLIENTS] = {
    "../resources/sprites/boat-01.bmp",
    "../resources/sprites/boat-02.bmp",
    "../resources/sprites/boat-03.bmp",
    "../resources/sprites/boat-04.bmp",
    "../resources/sprites/boat-05.bmp"
};


SDL_Texture* load_texture(SDL_Renderer* renderer, const char* file_path) {
    SDL_Texture* texture = NULL;
    SDL_Surface* surface = SDL_LoadBMP(file_path);
    if (!surface) {
        printf("Failed to load image: %s\n", SDL_GetError());
        return NULL;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        return NULL;
    }

    return texture;
}

float to_degrees(float radians) {
    return radians * (180.f / PI);
}

void render_players() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (players[i].id != INVALID_PLAYER_ID) {
            SDL_FRect dstRect = { players_interpolated[i].x, players_interpolated[i].y, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT };
            SDL_RenderTextureRotated(renderer, player_sprites_map[i % PLAYER_SPRITE_COUNT], NULL, 
                &dstRect, to_degrees(players_interpolated[i].rotation), NULL, SDL_FLIP_NONE);
        }
    }

    SDL_RenderPresent(renderer);
}


SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) == false) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }

    if (SDL_CreateWindowAndRenderer("Multiplayer Game Client", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer) == false) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create window/renderer!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }



    // Initial player data setup
    // Load all player sprites
    for (int i = 0; i < PLAYER_SPRITE_COUNT; i++) {
        SDL_Surface *surface = SDL_LoadBMP(player_sprite_files[i]);
        if (!surface) {
            fprintf(stderr, "Could not load BMP image %s: %s\n", player_sprite_files[i], SDL_GetError());
            continue;
        }

        player_sprites_map[i] = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);
        if (!player_sprites_map[i]) {
            fprintf(stderr, "Could not create texture from surface %s: %s\n", player_sprite_files[i], SDL_GetError());
        }
    }

    connect_to_server();

    previous_time = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float lerp_halflife(float a, float b, float t, float p, float dt) {
    float hl = -t/log2f(p);
    return lerp(a, b, 1-exp2f(-dt/hl));
}

void update_players(float dt) {
    float precision = 1/100;

    for(int i = 0; i < MAX_CLIENTS; ++i) {
        if(players[i].id == INVALID_PLAYER_ID) continue;
        float t = dt / GAME_STATE_UPDATE_FRAME_DELAY;
        players_interpolated[i].id = players[i].id; //todo em
        /*
        players_interpolated[i].x = players_last[i].x + (players[i].x - players_last[i].x) * t;
        players_interpolated[i].y = players_last[i].y + (players[i].y - players_last[i].y) * t;
        players_interpolated[i].rotation = players_last[i].rotation + (players[i].rotation - players_last[i].rotation) * t;*/
#if USE_HL_LERP
        players_interpolated[i].x = lerp_halflife(players_last[i].x, players[i].x, t, precision, dt);
        players_interpolated[i].y = lerp_halflife(players_last[i].y, players[i].y, t, precision, dt);
        players_interpolated[i].rotation = lerp_halflife(players_last[i].rotation, players[i].rotation, t, precision, dt);
#else
        players_interpolated[i].x = lerp(players_last[i].x, players[i].x, t);
        players_interpolated[i].y = lerp(players_last[i].y, players[i].y, t);
        players_interpolated[i].rotation = lerp(players_last[i].rotation, players[i].rotation, t);
#endif
    }
}

void render_game() {
    render_players();
}


void update_game(float dt) {
    const bool *state = SDL_GetKeyboardState(NULL);
    float rot = local_player.rotation;
    if (state[SDL_SCANCODE_W]) {
        speed = max_speed;
    } else {
        speed = 0.0f;
    }
    if (state[SDL_SCANCODE_S]) {
        speed = -max_speed;
    } 
    if (state[SDL_SCANCODE_A]) {
        rot -= rotation_speed;
    }
    if (state[SDL_SCANCODE_D]) {
        rot += rotation_speed;
    }

    rot = fmodf(rot, 2.0f * PI);
    if (rot < 0.0f) {
        rot += 2.0f * PI;
    }

    float direction_x = cosf(rot);
    float direction_y = sinf(rot);
    local_player.rotation = rot;
    local_player.x += direction_x * speed * dt;
    local_player.y += direction_y * speed * dt;
    update_players(dt);
}


SDL_AppResult SDL_AppIterate(void *appstate) {
    int game_running = 1;

    Uint32 current_time = SDL_GetTicks();
    float delta_time = (current_time - previous_time) / 1000.0f;
    previous_time = current_time;

    if(is_update_locked) return SDL_APP_CONTINUE;
    update_game(delta_time);
    render_game();
    
    //printf("processed %d, %d\n", local_player.x, local_player.y);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    SDL_DestroyTexture(player_sprites_map[local_player_data.sprite_id]);
}