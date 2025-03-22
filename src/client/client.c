#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "client.h"

Player players[MAX_CLIENTS];
PlayerStaticData player_data[MAX_CLIENTS];
Player local_player = {INVALID_PLAYER_ID, 100, 100};
PlayerStaticData local_player_data = {INVALID_PLAYER_ID, 0};

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

int check(int exp, const char *msg) {
    if (exp == -1) {
        perror(msg);
        exit(1);
    }
    return exp;
}

#if USE_FRAMES
void receive_server_data(int client_socket) {
    //TODO: data frames
    ReceivedDataFrame received_data;
    int bytes_received = recv(client_socket, &received_data, sizeof(ReceivedDataFrame), 0);
    printf("Received %d, el1: %d, el2: %d\n", received_data.header, received_data.el1, received_data.el2);
    switch(received_data.header) {
        case PLAYER_STATIC_DATA_HEADER:
            PlayerStaticData data;
            data.id = received_data.el1;
            data.sprite_id = received_data.el2;
            player_data[data.id].id = data.id;
            player_data[data.id].sprite_id = data.sprite_id;
            printf("Received static data for player ID: %d, Sprite ID: %d\n", data.id, data.sprite_id);
            break;

        case PLAYER_DYNAMIC_DATA_HEADER:
            //TODO: right now we just update the whole array
            /*Player updated_players[MAX_CLIENTS];
            bytes_received = recv(client_socket, updated_players, sizeof(updated_players), 0);
            if (bytes_received <= 0) {
                perror("Receive failed");
                break;
            }
            memcpy(players, updated_players, sizeof(updated_players));
            break;*/
            if (bytes_received <= 0) {
                perror("Receive failed");
                break;
            }
            memcpy(&players[received_data.el1], &received_data, sizeof(received_data));
            break;
    }
}
#else
void receive_server_data(int client_socket) {
    //TODO: data frames
    ReceivedDataFrame received_data;
    int bytes_received = recv(client_socket, &received_data, sizeof(ReceivedDataFrame), 0);
#if PRINT_RECEIVED_INFO
    printf("Received %d\n", received_data.header);
#endif
    switch(received_data.header) {
        case PLAYER_STATIC_DATA_HEADER:
            PlayerStaticData data;
            bytes_received = recv(client_socket, &data, sizeof(PlayerStaticData), 0);
            player_data[data.id].id = data.id;
            player_data[data.id].sprite_id = data.sprite_id;
            printf("Received static data for player ID: %d, Sprite ID: %d\n", data.id, data.sprite_id);
            break;

        case PLAYER_DYNAMIC_DATA_HEADER:
            //TODO: right now we just update the whole array
            Player updated_players[MAX_CLIENTS];
            bytes_received = recv(client_socket, updated_players, sizeof(updated_players), 0);
            if (bytes_received <= 0) {
                perror("Receive failed");
                break;
            }
#if PRINT_RECEIVED_INFO
            printf("Received %d, el1: %d, el2: %d\n", updated_players[0].id,updated_players[0].x, updated_players[0].y);
#endif
            memcpy(players, updated_players, sizeof(updated_players));
            break;
    }
}
#endif

void *client_communication(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    // disable Nagle’s algorithm
    int flag = 1;
    setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    int assigned_id = 0;
    int bytes_received = recv(client_socket, &assigned_id, sizeof(int), 0);
    if (bytes_received <= 0) {
        perror("Failed to receive an assigned ID");
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

        receive_server_data(client_socket);

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