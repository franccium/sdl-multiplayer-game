#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "server.h"
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

int health_check(int exp, const char *msg) {
    if (exp == -1) {
        perror(msg);
        exit(1);
    }
    return exp;
}

void broadcast_players() {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != UNUSED_SOCKET_ID) {
            int ret = send(client_sockets[i], players, sizeof(players), 0);
            if (ret < 0) {
                perror("Send failed, removing client");
                close(client_sockets[i]);
                client_sockets[i] = UNUSED_SOCKET_ID;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void broadcast_new_player(int new_player_index) {
    PlayerStaticData new_player_data = player_data[new_player_index];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != UNUSED_SOCKET_ID && i != new_player_index) {
            send(client_sockets[i], &new_player_data, sizeof(PlayerStaticData), 0);
            printf("for others sent static: %d, %d  \n", new_player_data.id, new_player_data.sprite_id);
        }
    }
}

void send_existing_players_static(int client_socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != UNUSED_SOCKET_ID && player_data[i].id != INVALID_PLAYER_ID) {
            send(client_socket, &player_data[i], sizeof(PlayerStaticData), 0);
            printf("for new sent static: %d, %d\n", player_data[i].id, player_data[i].sprite_id);
        }
    }
}


void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    // disable Nagle's algorithm for low-latency sends
    int flag = 1;
    setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    int client_id = -1;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {        
        if (players[i].id == INVALID_PLAYER_ID) {
            players[i].id = i;
            client_id = i;
            client_sockets[i] = client_socket;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if(client_id == -1) {
        printf("No available slot for new client\n");
        close(client_socket);
        return NULL;
    }
    send(client_socket, &players[client_id].id, sizeof(char), 0);
    
    pthread_mutex_lock(&players_mutex);
    player_data[client_id].id = players[client_id].id;
    player_data[client_id].sprite_id = client_id;

    send_existing_players_static(client_socket);
    broadcast_new_player(client_id);
    pthread_mutex_unlock(&players_mutex);
    printf("New client connected, assigned id: %d, static id: %d, sprite: %d\n", players[client_id].id, player_data[client_id].id, player_data[client_id].sprite_id);
    ++connected_clients_count;
    
    Player update;
    while (1) {
        int bytes_received = recv(client_socket, &update, sizeof(Player), 0);
        if (bytes_received <= 0) {
            perror("Receive failed or client disconnected");
            break;
        }
#if PRINT_RECEIVED_DATA
        printf("received id:%d, {%f, %f, %f}\n", update.id, update.x, update.y, update.rotation);
#endif
        pthread_mutex_lock(&players_mutex);
        players[client_id] = update;
        pthread_mutex_unlock(&players_mutex);
    }

    // clean up on disconnect
    pthread_mutex_lock(&clients_mutex);
    client_sockets[client_id] = UNUSED_SOCKET_ID;
    --connected_clients_count;
    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_lock(&players_mutex);
    players[client_id].id = INVALID_PLAYER_ID;
    players[client_id].x = 0;
    players[client_id].y = 0;
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


int initialize_server(int *server_socket) {
    struct sockaddr_in server_address;

    // Define data headers
    for(int i = 0; i < MAX_CLIENTS; ++i){
        players[i].header = PLAYER_DYNAMIC_DATA_HEADER;
        player_data[i].header = PLAYER_STATIC_DATA_HEADER;
        players[i].id = INVALID_PLAYER_ID;
        player_data[i].id = INVALID_PLAYER_ID;
    }
    
    // Create socket
    *server_socket = health_check(socket(AF_INET, SOCK_STREAM, 0), "Socket creation failed");
    
    // Configure server address structure
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);
    
    // Bind socket to address
    health_check(bind(*server_socket, (struct sockaddr *)&server_address, sizeof(server_address)), "Bind failed");

    // Start listening for incoming connections
    health_check(listen(*server_socket, MAX_CLIENTS), "Listen failed");

    printf("Server listening on port %d...\n", PORT);
    return 0;
}


int main() {
    int server_socket, *client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t addr_len = sizeof(client_address);

    memset(client_sockets, UNUSED_SOCKET_ID, sizeof(client_sockets));
    memset(players, 0, sizeof(players));
    memset(player_data, 0, sizeof(player_data));

    if (initialize_server(&server_socket) < 0) {
        fprintf(stderr, "Server initialization failed\n");
        return 1;
    }

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
