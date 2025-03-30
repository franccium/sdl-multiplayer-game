
#include "common.h"

typedef struct {
    unsigned char keys[MAX_HASHSET_SIZE];
    int values[MAX_HASHSET_SIZE];
    int occupied[MAX_HASHSET_SIZE];
    int size;
} CollisionHashSet;

void initHashSet(CollisionHashSet* set);
int hash(unsigned char key, int tableSize);
int getHashsetValue(CollisionHashSet* set, unsigned char key);
int putHashsetValue(CollisionHashSet* set, unsigned char key, int value);
void updateHashsetCollision(CollisionHashSet* set, unsigned char collision_byte, int currentTime);
