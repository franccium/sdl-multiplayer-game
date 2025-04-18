#include "collision_queue.h"

void init_collision_queue(CollisionQueue *queue) {
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
}
int is_collision_queue_empty(CollisionQueue *queue) {
    return queue->size == 0;
}
int is_collision_queue_full(CollisionQueue *queue) {
    return queue->size == MAX_COLLISION_COUNT;
}
int enqueue_collision(CollisionQueue *queue, CollisionInfo info) {
    if (is_collision_queue_full(queue)) {
        return 0;
    }
    queue->rear = (queue->rear + 1) % MAX_COLLISION_COUNT;
    queue->data[queue->rear] = info;
    queue->size++;
    return 1;
}
int dequeue_collision(CollisionQueue *queue, CollisionInfo *info) {
    if (is_collision_queue_empty(queue)) {
        return 0; // Queue is empty
    }
    *info = queue->data[queue->front];
    queue->front = (queue->front + 1) % MAX_COLLISION_COUNT;
    queue->size--;
    return 1; // Success
}
void update_collision_queue(CollisionQueue *queue, float delta_time) {
    int count = queue->size;
    for (int i = 0; i < count; i++) {
        CollisionInfo info;
        dequeue_collision(queue, &info);
        info.timer -= delta_time;
        if (info.timer > 0.0f) {
            enqueue_collision(queue, info); // Re-add if still active
        }
    }
}
