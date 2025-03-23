#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "client.h"

Player players[MAX_CLIENTS];
Player players_last[MAX_CLIENTS];
PlayerStaticData player_data[MAX_CLIENTS];
Player local_player = {INVALID_PLAYER_ID, 100, 100};
PlayerStaticData local_player_data = {INVALID_PLAYER_ID, 0};
char is_player_initialized = 1;
char is_update_locked = 0;
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
            break;

        case BULLET_HEADER:
            printf("womp womp i don't know how to handle a bullet");
            break;

        default:
            printf("wtf is this header");
            break;
    }
}

void *client_communication(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char assigned_id = 0;
    int bytes_received = recv(client_socket, &assigned_id, sizeof(char), 0);
    if (bytes_received <= 0) {
        printf("Bytes received: %d, %d\n", bytes_received, assigned_id);
        perror("Failed to receive an assigned ID");
        close(client_socket);
        return NULL;
    }
    local_player.id = assigned_id;
    local_player.header = PLAYER_DYNAMIC_DATA_HEADER;
    printf("Assigned ID: %d\n", assigned_id);


    is_player_initialized = 1;

    while (1) {
        receive_server_data(client_socket);
        if (send(client_socket, &local_player, sizeof(Player), 0) < 0) {
            perror("Send failed");
            break;
        }else{
     //       printf("sent %d, %f, %f, %d\n", local_player.id, local_player.x, local_player.y, local_player.header);
        }

        //usleep(DATA_SEND_SLEEP_TIME);
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