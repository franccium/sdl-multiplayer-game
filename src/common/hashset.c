#include "hashset.h"

void initHashSet(CollisionHashSet* set) {
    set->size = 0;
    for (int i = 0; i < MAX_HASHSET_SIZE; i++) {
        set->occupied[i] = 0;
    }
}

// unsigned char makeCollisionKey(char player1, char player2) {
//     if (player1 > player2) {
//         char temp = player1;
//         player1 = player2;
//         player2 = temp;
//     }
//     return ((player1 & 0x0F) << 4) | (player2 & 0x0F);
// }

int hash(unsigned char key, int tableSize) {
    return key % tableSize;
}

int getHashsetValue(CollisionHashSet* set, unsigned char key) {
    int index = hash(key, MAX_HASHSET_SIZE);
    int originalIndex = index;
    
    do {
        if (set->occupied[index] && set->keys[index] == key) {
            return set->values[index];
        }
        
        index = (index + 1) % MAX_HASHSET_SIZE;
    } while (index != originalIndex);
    
    return 10000; // basic value if there has been no collisions yet
}

int putHashsetValue(CollisionHashSet* set, unsigned char key, int value) {
    if (set->size >= MAX_HASHSET_SIZE) {
        return 0;
    }
    
    int index = hash(key, MAX_HASHSET_SIZE);
    int originalIndex = index;
    
    do {
        if (!set->occupied[index]) {
            set->keys[index] = key;
            set->values[index] = value;
            set->occupied[index] = 1;
            set->size++;
            return 1;
        } else if (set->keys[index] == key) {
            set->values[index] = value;
            return 1;
        }
        
        index = (index + 1) % MAX_HASHSET_SIZE;
    } while (index != originalIndex);
    
    return 0;
}

void updateHashsetCollision(CollisionHashSet* set, unsigned char collision_byte, int currentTime) {
    unsigned char key = collision_byte;
    putHashsetValue(set, key, currentTime);
}
