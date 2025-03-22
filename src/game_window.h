#pragma once

#include <math.h>
#define PI 3.1415927f
#define EPSILON 1e-6f

//NOTE: may need to find an optimal delay, or interpolate or sth if needed for performance, above 20 is too much
#define GAME_STATE_UPDATE_FRAME_DELAY 10