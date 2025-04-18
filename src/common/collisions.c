#include "collisions.h"

void get_rotated_rect_corners(const RotatedRect* rect, vec2 corners[4]) {
    float cos_a = cosf(rect->angle);
    float sin_a = sinf(rect->angle);

    vec2 baseCorners[4] = {
        { -rect->half_size[0], -rect->half_size[1] },
        {  rect->half_size[0], -rect->half_size[1] },
        {  rect->half_size[0],  rect->half_size[1] },
        { -rect->half_size[0],  rect->half_size[1] }
    };
    
    for (int i = 0; i < 4; i++) {
        corners[i][0] = rect->center[0] + (baseCorners[i][0] * cos_a - baseCorners[i][1] * sin_a);
        corners[i][1] = rect->center[1] + (baseCorners[i][0] * sin_a + baseCorners[i][1] * cos_a);
    }
}

bool sat_obb_collision_check(const RotatedRect* rect1, const RotatedRect* rect2, vec2* collision_normal) {
    vec2 corners_a[4];
    vec2 corners_b[4];
    get_rotated_rect_corners(rect1, corners_a);
    get_rotated_rect_corners(rect2, corners_b);

    float min_overlap = INFINITY;
    vec2 best_axis = {0.0f, 0.0f};

    // check all normals for both rects
    for (int i = 0; i < 4; i++) {
        vec2 edge_vec;
        glm_vec2_sub(corners_a[(i + 1) % 4], corners_a[i], edge_vec);
        
        // normal axis
        vec2 axis = { -edge_vec[1], edge_vec[0] };
        glm_vec2_normalize(axis);
        
        // project onto the axis
        float min_a = INFINITY, max_a = -INFINITY;
        float min_b = INFINITY, max_b = -INFINITY;
        for (int j = 0; j < 4; j++) {
            float projected_a = glm_vec2_dot(corners_a[j], axis);
            float projected_b = glm_vec2_dot(corners_b[j], axis);
            
            min_a = fminf(min_a, projected_a);
            max_a = fmaxf(max_a, projected_a);
            min_b = fminf(min_b, projected_b);
            max_b = fmaxf(max_b, projected_b);
        }
        // check if there was no collision
        if(!((min_b <= max_a) && max_b >= min_a)) return false;

        float overlap = fminf(max_a, max_b) - fmaxf(min_a, min_b);
        if (overlap < min_overlap) {
            min_overlap = overlap;
            glm_vec2_copy(axis, best_axis); // Store the best axis
        }
    }

    if(collision_normal != NULL) {
        glm_vec2_copy(best_axis, *collision_normal);
    }

    return true;
}