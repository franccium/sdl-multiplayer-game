#include <cglm/cglm.h>

typedef struct {
    vec2 center;
    vec2 half_size;
    float angle;
} RotatedRect;

void get_rotated_rect_corners(const RotatedRect* rect, vec2 corners[4]);
bool sat_obb_collision_check(const RotatedRect* rect1, const RotatedRect* rect2, vec2* collision_normal);