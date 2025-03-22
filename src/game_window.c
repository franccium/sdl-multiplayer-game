#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client/client.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define PLAYER_SPRITE_COUNT 5

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

#define PLAYER_SPRITE_WIDTH 128.0f
#define PLAYER_SPRITE_HEIGHT 64.0f

SDL_Texture* player_sprites_map[MAX_CLIENTS];

const char* player_sprite_files[MAX_CLIENTS] = {
    "boat-01.bmp",
    "boat-02.bmp",
    "boat-03.bmp",
    "boat-04.bmp",
    "boat-05.bmp"
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

void render_players() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (players[i].id != INVALID_PLAYER_ID) {
            SDL_FRect dstRect = { (float)players[i].x, (float)players[i].y, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT };
            SDL_RenderTexture(renderer, player_sprites_map[players[i].id], NULL, &dstRect);
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

    SDL_Surface *surface = SDL_LoadBMP("../resources/sprites/boat-01.bmp");
    if (!surface) {
        fprintf(stderr, "Could not load BMP image: %s\n", SDL_GetError());
    }

    player_sprites_map[player_data->sprite_id] = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!player_sprites_map[player_data->sprite_id]) {
        fprintf(stderr, "Could not create texture from surface: %s\n", SDL_GetError());
    }

    connect_to_server();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    const bool *state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_W]) local_player.y -= 2;
    if (state[SDL_SCANCODE_S]) local_player.y += 2;
    if (state[SDL_SCANCODE_A]) local_player.x -= 2;
    if (state[SDL_SCANCODE_D]) local_player.x += 2; 
    //printf("processed %d, %d\n", local_player.x, local_player.y);
    render_players();

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    SDL_DestroyTexture(player_sprites_map[local_player_data.sprite_id]);
}