#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <time.h>

#include "server.h"
#include "common/common.h"
#include "common/collisions.h"
#include "common/hashset.h"

//NOTE: may need to find an optimal delay, or interpolate or sth if needed for performance, above 20 is too much
#define GAME_STATE_UPDATE_FRAME_DELAY 10

// global player state
Player players[MAX_CLIENTS];
PlayerStaticData player_data[MAX_CLIENTS];
int client_sockets[MAX_CLIENTS];
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bullets_mutex = PTHREAD_MUTEX_INITIALIZER;
CollisionHashSet collisionHashSet;

BulletNode* head;
int connected_clients_count = 0;
int server_busy = 0;
int bullets_count = 0;
unsigned char bid = 1; //NOTE: the client's buffer size can only a limited amount of bullets anyways, so if we really want more we either increase buffer size or send them in batches

int health_check(int exp, const char *msg) {
    if (exp == -1) {
        perror(msg);
        exit(1);
    }
    return exp;
}


void spawn_player_random_pos(int socket_id) {
    players[socket_id].x = MIN_SPAWN_X + rand() % (WINDOW_WIDTH - MIN_SPAWN_X);
    players[socket_id].y = MIN_SPAWN_Y + rand() % (WINDOW_WIDTH - MIN_SPAWN_Y);
}


bool is_bullet_dead(BulletNode* bullet_node){
    //check for dimensions
    float tolerance = BULLET_SPRITE_WIDTH + 16.0f; // dont want them to disappear before going fully out of bounds
    if (bullet_node->bullet.y > WINDOW_HEIGHT + tolerance || bullet_node->bullet.y < -tolerance
        || bullet_node->bullet.x > WINDOW_WIDTH - tolerance || bullet_node->bullet.x < -tolerance) {

            printf("\nkilled a bullet %d\n", bullets_count);
            return true;
        }
    RotatedRect bbox = {
        { bullet_node->bullet.x, bullet_node->bullet.y },
        { BULLET_HITBOX_WIDTH / 2.0f, BULLET_HITBOX_HEIGHT / 2.0f },
        0.0f
    };

    //check if it has hit a ship
    // id do it with other collision code but for now ok
    bool collides;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if(client_sockets[i] == UNUSED_SOCKET_ID
            || players[i].id == bullet_node->shooter_id) continue;

        RotatedRect otherBbox = {
            { players[i].x, players[i].y },
            { PLAYER_HITBOX_WIDTH / 2.0f, PLAYER_HITBOX_HEIGHT / 2.0f },
            players[i].rotation
        };
        collides = sat_obb_collision_check(&bbox, &otherBbox);
        if(collides) {
            players[i].hp -= BULLET_DAMAGE;
            printf("player hp %d\n", players[i].hp);
            break;
        }
    }
    
    return collides;
}

void get_bullet_direction(vec2 direction, Player *player) {
    // since player action is 1 for right and -1 for left, we just do this
    float angle = player->rotation + player->action * (PI / 2.0f);
    direction[0] = cosf(angle);
    direction[1] = sinf(angle);
}

void add_bullet(Player* shooter){
    BulletNode* new_node =(BulletNode*)malloc(sizeof(BulletNode)); 
    if (!new_node) {
        perror("Memory allocation failed");
        return;
    }
    Bullet new_bullet;
    new_bullet.x = shooter->x;
    new_bullet.y = shooter->y;
    new_bullet.header = BULLET_HEADER;
    vec2 direction;
    new_node->next = NULL; 
    new_node->shooter_id = shooter->id; 
    memcpy(&new_node->bullet, &new_bullet, sizeof(Bullet));
    get_bullet_direction(new_node->direction, shooter);
    new_node->bullet.id = bid;
    ++bid;
    if (head){
        BulletNode* next = head->next;
        head->next = new_node;
        new_node->next = next;
    }
    else{
        head = new_node;
    }
    bullets_count += 1;
    //printf("thats many bullets %d", bullets_count);
}

void update_bullet_position(BulletNode* bullet_node) { //adjust to whatever values
    if (!bullet_node) return;

    bullet_node->bullet.x += bullet_node->direction[0] * BULLET_SPEED;
    bullet_node->bullet.y += bullet_node->direction[1] * BULLET_SPEED;

    //printf("{%f, %f}\n", bullet_node->bullet.x, bullet_node->bullet.y);
}


void update_bullets(){
    pthread_mutex_lock(&bullets_mutex);
    BulletNode* current = head;
    BulletNode* last = NULL;
    while(current){
        update_bullet_position(current);
        if (is_bullet_dead(current)){
            bullets_count -= 1;
            if(last){
                last->next = current->next;
                BulletNode* temp = current;
                current = current->next;
                if(temp){
                    free(temp);
                    temp = NULL;
                }
            }
            else{
                head = current->next;
                BulletNode* temp = current;
                current = current->next;
                if(temp){
                    free(temp);
                    temp = NULL;
                }
            }
        }
        else{
            last = current;
            current = current->next;
        }
    }
    pthread_mutex_unlock(&bullets_mutex);
}


void check_collisions() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        // clear the collisions first
        players[i].collision_byte = 0;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == UNUSED_SOCKET_ID) continue;
        bool collides;
        char collisionMatrix = 0;
        char collisionCount = 0;
        RotatedRect bbox = {
            { players[i].x, players[i].y },
            { PLAYER_HITBOX_WIDTH / 2.0f, PLAYER_HITBOX_HEIGHT / 2.0f },
            players[i].rotation
        };

        for (int j = 0; j < MAX_CLIENTS; j++) {
            if (client_sockets[j] == UNUSED_SOCKET_ID) continue;
            if(i == j) continue;

            RotatedRect otherBbox = {
                { players[j].x, players[j].y },
                { PLAYER_HITBOX_WIDTH / 2.0f, PLAYER_HITBOX_HEIGHT / 2.0f },
                players[j].rotation
            };

            collides = sat_obb_collision_check(&bbox, &otherBbox);
            collides << j;
            collisionMatrix |= collides;
            int current_time = (int)time(NULL);
            int time_difference = current_time - getHashsetValue(&collisionHashSet, collisionMatrix);
            if(collides &&  time_difference > 5){
                ++collisionCount;
                players[j].hp -= BOAT_COLLISION_DAMAGE;
                players[i].hp -= BOAT_COLLISION_DAMAGE;
                players[i].collision_byte |= collisionMatrix;
                players[j].collision_byte |= collisionMatrix;
                putHashsetValue(&collisionHashSet, players[i].collision_byte, current_time);      
                printf("%d collides with %d\n", players[i].id, players[j].id);
                printf("collision pos: %f %f ; %f %f \n", players[i].x, players[i].y, players[j].x, players[j].y);
                printf("Collision matrix %c\n", collisionMatrix);
                printf("time difference: %d\n", time_difference);
            }
            

        }
       // printf("%d collisions found for player id: %d\n", collisionCount, players[i].id);
 
    }
}

void kill_player(int socket_id) {

}

void handle_player_death() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == UNUSED_SOCKET_ID) continue;
        if(players[i].hp <= 0) {
            kill_player(i);
        }
    }
}


void broadcast_bullets(){    
    pthread_mutex_lock(&clients_mutex);
    update_bullets();
    BulletNode* current = head;
    Bullet readyBullets[bullets_count + 1];
    readyBullets[0].header = BULLET_HEADER;
    readyBullets[0].id = bullets_count;
    int i = 1;
    while (current) {
        readyBullets[i] = current->bullet;
        current = current->next;
        ++i;
    }
    if(bullets_count > MAX_BULLETS_PER_BUFFER) {
        printf("bullets can overflow the buffer, how did we get there?\n");
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    //printf("sending %d bullets", bullets_count);
    //readyBullets[0].header += 100*bullets_count;
    //readyBullets[0].id = bullets_count;

    pthread_mutex_lock(&bullets_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != UNUSED_SOCKET_ID) {
            int ret = send(client_sockets[i], readyBullets, sizeof(readyBullets), 0);

        }
    }
    pthread_mutex_unlock(&bullets_mutex);
    pthread_mutex_unlock(&clients_mutex);
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
#if PRINT_SENT_DYNAMIC_DATA
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == UNUSED_SOCKET_ID) continue;
        printf("sent H:%d id:%d, {%f, %f, %f}\n", players[j].header, players[j].id, players[j].x, players[j].y, players[j].rotation);
    }
#endif
    pthread_mutex_unlock(&clients_mutex);
}

void broadcast_new_player(int new_player_index) {
    PlayerStaticData new_player_data = player_data[new_player_index];
    new_player_data.header = PLAYER_STATIC_DATA_HEADER;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != UNUSED_SOCKET_ID && i != new_player_index) {
            send(client_sockets[i], &new_player_data, sizeof(PlayerStaticData), 0);
#if PRINT_SENT_STATIC_DATA
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == UNUSED_SOCKET_ID) continue;
                printf("sent new player static H:%d id:%d\n", new_player_data.header, new_player_data.id);
            }
#endif
        }
    }
}

void send_existing_players_static(int client_socket) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != UNUSED_SOCKET_ID && player_data[i].id != INVALID_PLAYER_ID) {
            send(client_socket, &player_data[i], sizeof(PlayerStaticData), 0);
#if PRINT_SENT_STATIC_DATA
            printf("sent TO THE new player static H:%d id:%d\n", player_data[i].header, player_data[i].id);
#endif
        }
    }
}

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    // disable Nagle's algorithm for low-latency sends

    int client_id = INVALID_PLAYER_ID;
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

    if(client_id == INVALID_PLAYER_ID) {
        printf("No available slot for new client\n");
        close(client_socket);
        return NULL;
    }
    players[client_id].hp = INITIAL_PLAYER_HP;
    send(client_socket, &players[client_id], sizeof(Player), 0);

    server_busy = 1;
    
    pthread_mutex_lock(&players_mutex);
    player_data[client_id].id = players[client_id].id;
    player_data[client_id].sprite_id = client_id;

    send_existing_players_static(client_socket);
    broadcast_new_player(client_id);
    pthread_mutex_unlock(&players_mutex);
    printf("New client connected, assigned id: %d, static id: %d, sprite: %d\n", players[client_id].id, player_data[client_id].id, player_data[client_id].sprite_id);
    for(int i = 0; i < MAX_CLIENTS; ++i) {
        printf("----- KNOWN PLAYER: %d %d %d %d %f %f %f\n", players[i].header, players[i].id, players[i].action
                , players[i].collision_byte, players[i].x, players[i].y, players[i].rotation);
    }
    
    ++connected_clients_count;
    server_busy = 0;
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

        if(update.action) {
            if(update.action == PLAYER_ACTION_RESPAWN) {
                players[client_id].hp = INITIAL_PLAYER_HP;
                spawn_player_random_pos(client_id);
                printf("GOT RESPAWN");
            }
            else {
                pthread_mutex_lock(&bullets_mutex);
                printf("from %d received action %d -- ", update.id, update.action);
                add_bullet(&update);
                //printf("new bullet");
                pthread_mutex_unlock(&bullets_mutex);
            }
        }

        pthread_mutex_lock(&players_mutex);
        update.hp = players[client_id].hp;
        update.action = PLAYER_ACTION_NONE;
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
    spawn_player_random_pos(client_id);
    players[client_id].rotation = 0.0f;
    pthread_mutex_unlock(&players_mutex);
    close(client_socket);
    return NULL;
}

// a dedicated thread for game state broadcasting, works at fixed intervals
void *broadcast_loop(void *arg) {
    const int frame_delay_ms = GAME_STATE_UPDATE_FRAME_DELAY;
    while (1) {
        if(server_busy) usleep(frame_delay_ms * 1000);
        check_collisions();
        broadcast_players();
        broadcast_bullets();
        handle_player_death();
        usleep(frame_delay_ms * 1000);
    }
    return NULL;
}

int initialize_server(int *server_socket) {
    struct sockaddr_in server_address;

    initHashSet(&collisionHashSet);
    // Define data headers
    for(int i = 0; i < MAX_CLIENTS; ++i){
        players[i].header = PLAYER_DYNAMIC_DATA_HEADER;
        player_data[i].header = PLAYER_STATIC_DATA_HEADER;
        players[i].id = INVALID_PLAYER_ID;
        player_data[i].id = INVALID_PLAYER_ID;
        players[i].action = PLAYER_ACTION_NONE;
        spawn_player_random_pos(i);
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
        if (client_socket == NULL) {
            perror("Memory allocation failed");
            continue;
        }
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
