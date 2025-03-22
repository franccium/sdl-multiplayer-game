#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "common/common.h"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

/*
TODO:
make a single define file for stuff like PORT, SERVER_IP, MAX_PLAYERS, sent structs (Player), etc
do some server stuff

front:
load sprites
battle stuff
some ui
polish graphics
*/



// server debug info
#define PRINT_RECEIVED_INFO 0
#define PRINT_SENT_PLAYER_UPDATE 0

// SDL
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

Player players[MAX_CLIENTS];
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;

static Player local_player = {0, 100, 100};
SDL_Texture* player_texture = NULL;


int check(int exp, const char *msg) {
    if (exp == -1) {
        perror(msg);
        exit(1);
    }
    return exp;
}


void *client_communication(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    // disable Nagle’s algorithm
    int flag = 1;
    setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    int assigned_id = 0;
    int bytes_received = recv(client_socket, &assigned_id, sizeof(int), 0);
    if (bytes_received <= 0) {
        perror("Failed to receive assigned ID");
        close(client_socket);
        return NULL;
    }
    local_player.id = assigned_id;
    printf("Assigned ID: %d\n", assigned_id);

    while (1) {
        if (send(client_socket, &local_player, sizeof(Player), 0) < 0) {
            perror("Send failed");
            break;
        }

        Player updated_players[MAX_CLIENTS];
        bytes_received = recv(client_socket, updated_players, sizeof(updated_players), 0);
        if (bytes_received <= 0) {
            perror("Receive failed");
            break;
        }
        memcpy(players, updated_players, sizeof(updated_players));

        usleep(DATA_SEND_SLEEP_TIME);
    }

    close(client_socket);
    return NULL;
}

void connect_to_server() {
    int client_socket;
    struct sockaddr_in server_address;

    client_socket = check(socket(AF_INET, SOCK_STREAM, 0), "Socket creation failed");

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr);

    check(connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)), "Connection failed");
    printf("Connected to server at %s:%d\n", SERVER_IP, PORT);

    pthread_t tid;
    int *socket_ptr = malloc(sizeof(int));
    *socket_ptr = client_socket;
    pthread_create(&tid, NULL, client_communication, socket_ptr);
    pthread_detach(tid);
}

void render_players() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (players[i].id != 0) {
            SDL_FRect dstRect = { (float)players[i].x, (float)players[i].y, 128.0f, 64.0f };
            SDL_RenderTexture(renderer, player_texture, NULL, &dstRect);
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

    player_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!player_texture) {
        fprintf(stderr, "Could not create texture from surface: %s\n", SDL_GetError());
    }

    connect_to_server();

    return SDL_APP_CONTINUE;  /* Carry on with the program! */
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

    render_players();

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    SDL_DestroyTexture(player_texture);
}