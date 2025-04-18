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

#define BUFFER_SIZE 2048 //! can fit up to 200 bullets
#define MAX_BULLETS_PER_BUFFER BUFFER_SIZE / sizeof(Bullet)
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

#define BULLET_SPEED 1.5f
#define BULLET_COUNT_INDEX 0
#define BULLET_SPRITE_WIDTH 32.0f
#define BULLET_SPRITE_HEIGHT 32.0f

#define SHOOT_RIGHT 1.0
#define SHOOT_LEFT -1.0

#define INITIAL_PLAYER_HP 100
#define BULLET_DAMAGE 20
#define BOAT_COLLISION_DAMAGE 30

#define MAX_HASHSET_SIZE 25

typedef struct {
    unsigned char header; // 0 for player position
    char id;
    char action; // 0 -> no action, 1 -> shoot left, 2 -> shoot right
    unsigned char collision_byte; //each bit specifies whether collision occurs with a specific player
    // example: 00000010 -> collision with player 2, 00000100 collision with player 4, simillar to dr's Lebiedź marking solution
    int hp;
    float x, y, rotation;
} Player;

typedef struct {
    unsigned char header; // 1 for static
    char id;
    unsigned char sprite_id;
} PlayerStaticData;


typedef struct{
    unsigned char header; // 2 for bullet
    unsigned int id;
    float x, y;
} Bullet; // Assign special value for a destroyed bullet,
//  remember to assign a specific header when creating new bullet object

typedef struct BulletNode BulletNode;

typedef struct BulletNode {
    Bullet bullet;
    vec2 direction;
    char shooter_id; // need this to avoid bullet colliding with the shooter 
    BulletNode* next; // Pointer to the next BulletNode
} BulletNode;
