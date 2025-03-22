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

 //NOTE: may need to find an optimal delay, or interpolate or sth if needed for performance, above 20 is too much
#define GAME_STATE_UPDATE_FRAME_DELAY 10


// global player state
Player players[MAX_CLIENTS];
PlayerStaticData player_data[MAX_CLIENTS];
int client_sockets[MAX_CLIENTS];
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int connected_clients_count = 0;

int check(int exp, const char *msg) {
    if (exp == -1) {
        perror(msg);
        exit(1);
    }
    return exp;
}

#if USE_FRAMES
void broadcast_players() {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0) {
            /*
            players->header = PLAYER_DYNAMIC_DATA_HEADER; //! placeholder
            int ret = send(client_sockets[i], players, sizeof(players), 0);
            if (ret < 0) {
                perror("Send failed, removing client");
                close(client_sockets[i]);
                client_sockets[i] = 0;
            }*/
            for(int j = 0; j < connected_clients_count; j++) {
                players[j].header = PLAYER_DYNAMIC_DATA_HEADER; //! placeholder
                int ret = send(client_sockets[i], &players[j], sizeof(Player), 0);
                if (ret < 0) {
                    perror("Send failed, removing client");
                    close(client_sockets[i]);
                    client_sockets[i] = 0;
                }
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

//TODO: doesnt make sense to make new copies like this i think
void broadcast_new_player(int new_player_index) {
    ReceivedDataFrame new_player_data;
    new_player_data.header = PLAYER_STATIC_DATA_HEADER;
    new_player_data.el1 = player_data[new_player_index].id;
    new_player_data.el2 = player_data[new_player_index].sprite_id;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0 && i != new_player_index) {
            send(client_sockets[i], &new_player_data, sizeof(ReceivedDataFrame), 0);
        }
    }
}

void send_existing_players(int client_socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (player_data[i].id != INVALID_PLAYER_ID) {
            ReceivedDataFrame new_player_data;
            new_player_data.header = PLAYER_STATIC_DATA_HEADER;
            new_player_data.el1 = player_data[i].id;
            new_player_data.el2 = player_data[i].sprite_id;
            send(client_socket, &player_data[i], sizeof(ReceivedDataFrame), 0);
        }
    }
}

#else
/*
void broadcast_players() {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0) {
            int ret = send(client_sockets[i], players, sizeof(players), 0);
            if (ret < 0) {
                perror("Send failed, removing client");
                close(client_sockets[i]);
                client_sockets[i] = 0;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}*/
void broadcast_players() {
    ReceivedDataFrame frame;
    frame.header = PLAYER_DYNAMIC_DATA_HEADER;

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0) {
            int ret = send(client_sockets[i], &frame, sizeof(ReceivedDataFrame), 0); 
            if (ret < 0) {
                perror("Send failed, removing client");
                close(client_sockets[i]);
                client_sockets[i] = 0;
            }
            ret = send(client_sockets[i], players, sizeof(players), 0);
            if (ret < 0) {
                perror("Send failed, removing client");
                close(client_sockets[i]);
                client_sockets[i] = 0;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void broadcast_new_player(int new_player_index) {
    ReceivedDataFrame frame;
    frame.header = PLAYER_STATIC_DATA_HEADER;

    PlayerStaticData new_player_data = player_data[new_player_index];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0 && i != new_player_index) {
            send(client_sockets[i], &frame, sizeof(ReceivedDataFrame), 0); 
            send(client_sockets[i], &new_player_data, sizeof(PlayerStaticData), 0);
        }
    }
}

void send_existing_players(int client_socket) {
    ReceivedDataFrame frame;
    frame.header = PLAYER_STATIC_DATA_HEADER;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (player_data[i].id != INVALID_PLAYER_ID) {
            send(client_sockets[i], &frame, sizeof(ReceivedDataFrame), 0); 
            send(client_socket, &player_data[i], sizeof(PlayerStaticData), 0);
        }
    }
}
#endif

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    // disable Nagle's algorithm for low-latency sends
    int flag = 1;
    setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    int new_client_id = -1;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (players[i].id == INVALID_PLAYER_ID) {
            players[i].id = i;
            new_client_id = i;
            client_sockets[i] = client_socket;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if(new_client_id == -1) {
        printf("No available slot for new client\n");
        close(client_socket);
        return NULL;
    }

    player_data[new_client_id].id = players[new_client_id].id;
    player_data[new_client_id].sprite_id = new_client_id;

    send(client_socket, &players[new_client_id].id, sizeof(int), 0);
    send_existing_players(client_socket);
    broadcast_new_player(new_client_id);
    printf("New client connected, assigned id: %d\n", players[new_client_id].id);
    ++connected_clients_count;
    
    Player update;
    while (1) {
        int bytes_received = recv(client_socket, &update, sizeof(Player), 0);
        if (bytes_received <= 0) {
            perror("Receive failed or client disconnected");
            break;
        }
        pthread_mutex_lock(&players_mutex);
        players[new_client_id] = update;
        //players[new_client_id].id = new_client_id + 1;
        pthread_mutex_unlock(&players_mutex);
    }

    // clean up on disconnect
    pthread_mutex_lock(&clients_mutex);
    client_sockets[new_client_id] = 0;
    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_lock(&players_mutex);
    players[new_client_id].id = INVALID_PLAYER_ID;
    players[new_client_id].x = 0;
    players[new_client_id].y = 0;
    --connected_clients_count;
    pthread_mutex_unlock(&players_mutex);
    close(client_socket);
    return NULL;
}

// a dedicated thread for game state broadcasting, works at fixed intervals
void *broadcast_loop(void *arg) {
    const int frame_delay_ms = GAME_STATE_UPDATE_FRAME_DELAY;
    while (1) {
        broadcast_players();
        usleep(frame_delay_ms * 1000);
    }
    return NULL;
}

int main() {
    int server_socket, *client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t addr_len = sizeof(client_address);

    memset(client_sockets, 0, sizeof(client_sockets));
    memset(players, 0, sizeof(players));
    memset(player_data, 0, sizeof(player_data));
    for(int i = 0; i < MAX_CLIENTS; ++i) {
        players[i].id = INVALID_PLAYER_ID;
        player_data[i].id = INVALID_PLAYER_ID;
    }

    server_socket = check(socket(AF_INET, SOCK_STREAM, 0), "Socket creation failed");

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    check(bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)), "Bind failed");
    check(listen(server_socket, MAX_CLIENTS), "Listen failed");

    printf("Server listening on port %d...\n", PORT);

    // start the game state broadcast thread
    pthread_t bcast_tid;
    pthread_create(&bcast_tid, NULL, broadcast_loop, NULL);
    pthread_detach(bcast_tid);

    while (1) {
        client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr *)&client_address, &addr_len);
        if (*client_socket < 0) {
            perror("Accept failed");
            free(client_socket);
            continue;
        }
        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, client_socket);
        pthread_detach(tid);
    }

    close(server_socket);
    return 0;
}
