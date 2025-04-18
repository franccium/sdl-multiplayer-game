#pragma once

#define PLAYER_COLLISION_ANIM_TIME 2.0f
#define BULLET_COLLISION_ANIM_TIME 2.0f
#define COLLISION_TYPE_PLAYER 1
#define COLLISION_TYPE_BULLET 2
#define MAX_COLLISION_COUNT 20

typedef struct CollisionInfo {
    float x;
    float y;
    float timer;
    char type;
} CollisionInfo;

typedef struct {
    CollisionInfo data[MAX_COLLISION_COUNT];
    int front;
    int rear;
    int size;
} CollisionQueue;

void init_collision_queue(CollisionQueue *queue);
int is_collision_queue_empty(CollisionQueue *queue);
int is_collision_queue_full(CollisionQueue *queue);
int enqueue_collision(CollisionQueue *queue, CollisionInfo info);
int dequeue_collision(CollisionQueue *queue, CollisionInfo *info);
void update_collision_queue(CollisionQueue *queue, float delta_time);
