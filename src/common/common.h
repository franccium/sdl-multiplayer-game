#pragma once

#define PORT 8080
#define MAX_CLIENTS 5
#define SERVER_IP "127.0.0.1"
#define DATA_SEND_SLEEP_TIME 5000
#define INVALID_PLAYER_ID -1


#define PLAYER_DYNAMIC_DATA_HEADER 0
#define PLAYER_STATIC_DATA_HEADER 1
#define USE_FRAMES 0

#if USE_FRAMES
typedef struct {
    char header;
    int id;
    int x, y;
} Player;

typedef struct {
    char header;
    int el1;
    int el2;
    int el3;
} ReceivedDataFrame;
#else
typedef struct {
    int id;
    int x, y;
} Player;
typedef struct {
    char header;
} ReceivedDataFrame;
#endif

typedef struct {
    int id;
    int sprite_id;
} PlayerStaticData;
