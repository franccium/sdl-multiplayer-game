#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "client.h"

Player players[MAX_CLIENTS];
Player players_last[MAX_CLIENTS];
PlayerStaticData player_data[MAX_CLIENTS];
Player local_player = {.header=PLAYER_DYNAMIC_DATA_HEADER ,.action=PLAYER_ACTION_NONE, .id=INVALID_PLAYER_ID, .hp=INITIAL_PLAYER_HP, .x=100, .y=100, .collision_byte=0, .rotation=0};
PlayerStaticData local_player_data = {INVALID_PLAYER_ID, 0};

Bullet* bullets;
Bullet* bullets_render;
Bullet* bullets_last;
int existing_bullets = 0;

char is_player_initialized = 1;
char is_update_locked = 0;
char should_update_sprites = 0;
float shoot_timer = 0.0f;
float respawn_timer = 0.0f;
char is_dead = 0;
int bullet_capacity = BULLETS_DEFAULT_CAPACITY;

pthread_mutex_t client_bullets_mutex = PTHREAD_MUTEX_INITIALIZER;

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

int check_connection(int exp, const char *msg) {
    if (exp == -1) {
        perror(msg);
        exit(1);
    }
    return exp;
}

void receive_server_data(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    char header = buffer[0];
  //  printf("Received %d\n", header);
    switch(header) {
        case PLAYER_STATIC_DATA_HEADER:
            if (bytes_received < sizeof(PlayerStaticData)) {
                printf("Incomplete PlayerStaticData received\n");
                return;
            }
            PlayerStaticData *data = (PlayerStaticData *)buffer;
            player_data[data->id].id = data->id;
            player_data[data->id].sprite_id = data->sprite_id;
            printf("Received static data for player ID: %d, Sprite ID: %d\n", data->id, data->sprite_id);
            //if(data->id == local_player.id) break;
            //load_player_sprite(data->id); //! cant load here
            should_update_sprites = 1;
            break;

        case PLAYER_DYNAMIC_DATA_HEADER:
            //We are sending the whole array
            Player updated_players[MAX_CLIENTS];
            if (bytes_received <= 0) {
                perror("Receive failed");
                break;
            }
            memcpy(updated_players, (Player*)buffer, sizeof(players));
            for(int i=0; i<MAX_CLIENTS; i++){
                //printf("----- RECEIVED PLAYER: %d %d %d %d %f %f %f\n", updated_players[i].header, updated_players[i].id, updated_players[i].action, updated_players[i].collision_byte, updated_players[i].x, updated_players[i].y, updated_players[i].rotation);
            }
            /*
            for(int i=0; i<MAX_CLIENTS; i++){
                Player* received_player;
                received_player = (Player *) (buffer + i*PLAYER_DYNAMIC_SIZE);
                memcpy(&updated_players[i], received_player, sizeof(Player));

                printf("----- RECEIVED PLAYER: %d %d %d %d %f %f %f\n", updated_players[i].header, updated_players[i].id, updated_players[i].action
                , updated_players[i].collision_byte, updated_players[i].x, updated_players[i].y, updated_players[i].rotation);
            }*/

            memcpy(players_last, players, sizeof(players));
            memcpy(players, updated_players, sizeof(updated_players));
            // updating the whole local_player or using it as a pointer to the player array lags the position updates and such
            local_player.hp = players[local_player.id].hp;
            local_player.collision_byte = players[local_player.id].collision_byte;

            break;

        case BULLET_HEADER:
            is_update_locked = 1; //TODO: still needed or not?
            //printf("cool it's a bullet\n");
            Bullet* info = (Bullet*)buffer;
            if (info[BULLET_COUNT_INDEX].id <= 0) {
                // get out asap before integers overflow
                is_update_locked = 0;
                return;
            }
            size_t bullets_to_copy = info[BULLET_COUNT_INDEX].id;
            //printf("copy %ld\n", bullets_to_copy);
            if (bullets_to_copy > bullet_capacity) {
                printf("clients bullets overflowed >.<\n"); 
                return; //NOTE: if we want to fit more bullets than capacitry, bullets_interpolated needs to be a dynamic array (and increased buffer size / batches for sending them)
                size_t new_capacity = bullets_to_copy * 2;
                printf("overflowed, resizing to %ld\n", new_capacity);

                Bullet *new_bullets = realloc(bullets, sizeof(Bullet) * new_capacity);
                Bullet *new_bullets_last = realloc(bullets_last, sizeof(Bullet) * new_capacity);

                if (!new_bullets || !new_bullets_last) {
                    perror("realloc failed");
                    free(new_bullets);
                    free(new_bullets_last);
                    is_update_locked = 0;
                    return;
                }

                bullets = new_bullets;
                bullets_last = new_bullets_last;
                bullet_capacity = new_capacity;
            }
            //todo if not over last overflowed capacity for some set time, can go back to capacity /= 2 
            memcpy(bullets_last, bullets, sizeof(bullets)); //todo add the ones that wont be copied
            memcpy(bullets, &info[1], bullets_to_copy * sizeof(Bullet));

            //memcpy(bullets, (Bullet*)buffer, sizeof(Bullet) * bullet_capacity);
            //printf("got bullets : %d\n", existing_bullets);
            existing_bullets = info[BULLET_COUNT_INDEX].id;

            is_update_locked = 0;
            break;

        default:
            printf("wtf is this header");
            break;
    }
}

void *client_communication(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    /*
    char assigned_id = 0;
    int bytes_received = recv(client_socket, &assigned_id, sizeof(char), 0);
    if (bytes_received <= 0) {
        printf("Bytes received: %d, %d\n", bytes_received, assigned_id);
        perror("Failed to receive an assigned ID");
        close(client_socket);
        return NULL;
    }
    local_player.id = assigned_id;*/
    int bytes_received = recv(client_socket, &local_player, sizeof(Player), 0);
    if (bytes_received <= 0) {
        printf("Bytes received: %d\n", bytes_received);
        perror("Failed to receive initial local player data");
        close(client_socket);
        return NULL;
    }
    local_player.header = PLAYER_DYNAMIC_DATA_HEADER;
    printf("Assigned ID: %d\n", local_player.id);


    is_player_initialized = 1;

    while (1) {
        receive_server_data(client_socket);
        // Check if there's an action and apply cooldown if needed
        //if ((shoot_timer > 0.0f)) {
           // local_player.action = PLAYER_ACTION_NONE;
        //}
        // If there's no action, send immediately without cooldown
        if (send(client_socket, &local_player, sizeof(Player), 0) < 0) {
            perror("Send failed");
            break;
        } else {
            //printf("lcoal's player hp: %d", local_player.hp);
            //printf("sent (no action)\n");
            if(local_player.action == PLAYER_ACTION_SHOOT_LEFT || local_player.action == PLAYER_ACTION_SHOOT_RIGHT) {
                shoot_timer = SHOOT_COOLDOWN;
            }
            if(local_player.action == PLAYER_ACTION_RESPAWN) {
                // we shouldn't need to do this but w/e
                local_player.x = MIN_SPAWN_X + rand() % (WINDOW_WIDTH - MIN_SPAWN_X);
                local_player.y = MIN_SPAWN_Y + rand() % (WINDOW_HEIGHT - MIN_SPAWN_Y);
            }
        }

        //printf("shoot cd: %f\n", shoot_timer);
        //if(local_player.action != 0) printf("shoot action: %d\n", local_player.action);
    
        // Reset the action after sending
        local_player.action = PLAYER_ACTION_NONE;
    
        usleep(DATA_SEND_SLEEP_TIME); // Delay between sending
    }

    close(client_socket);
    return NULL;
}

void connect_to_server() {
    is_player_initialized = 0;

    int client_socket;
    struct sockaddr_in server_address;

    client_socket = check_connection(socket(AF_INET, SOCK_STREAM, 0), "Socket creation failed");

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr);

    check_connection(connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)), "Connection failed");
    printf("Connected to server at %s:%d\n", SERVER_IP, PORT);

    pthread_t tid;
    int *socket_ptr = malloc(sizeof(int));
    *socket_ptr = client_socket;
    pthread_create(&tid, NULL, client_communication, socket_ptr);
    pthread_detach(tid);
}

