#pragma once

#include <math.h>
#include <cglm/cglm.h>
#define PI 3.1415927f
#define EPSILON 1e-6f

#define PORT 8080
#define MAX_CLIENTS 5
#define SERVER_IP "127.0.0.1"
#define DATA_SEND_SLEEP_TIME 5000
#define INVALID_PLAYER_ID -1

#define GAME_STATE_UPDATE_FRAME_DELAY 10

#define BUFFER_SIZE 80 //SIZE_OF_PLAYER-> the biggest struct
#define PLAYER_DYNAMIC_SIZE 20

#define PLAYER_DYNAMIC_DATA_HEADER 0
#define PLAYER_STATIC_DATA_HEADER 1
#define BULLET_HEADER 2
#define NO_ACTION 0

#define PLAYER_HITBOX_WIDTH 128.0f
#define PLAYER_HITBOX_HEIGHT 96.0f
#define BULLET_HITBOX_WIDTH 32.0f
#define BULLET_HITBOX_HEIGHT 32.0f

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#define BULLET_SPEED 1.0f

typedef struct {
    unsigned char header; // 0 for player position
    char id;
    unsigned char action; // 0 -> no action, 1 -> shoot left, 2 -> shoot right
    unsigned char collision_byte; //each bit specifies whether collision occurs with a specific player
    // example: 00000010 -> collision with player 2, 00000100 collision with player 4, simillar to dr's Lebiedź marking solution
    float x, y, rotation;
} Player;

typedef struct {
    unsigned char header; // 1 for static
    char id;
    unsigned char sprite_id;
} PlayerStaticData;


typedef struct{
    unsigned char header; // 2 for bullet
    float x, y;
} Bullet; // Assign special value for a destroyed bullet,
//  remember to assign a specific header when creating new bullet object


typedef struct BulletNode BulletNode;

typedef struct BulletNode {
    Bullet bullet;
    vec2 direction;
    BulletNode* next; // Pointer to the next BulletNode
} BulletNode;
