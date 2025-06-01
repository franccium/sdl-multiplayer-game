#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cglm/cglm.h>
#include "client/client.h"
#include "game_window.h"
#include "common/collisions.h"
#include "collision_queue.h"

#define GAME_TITLE "Bitwa Piracka"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define PLAYER_SPRITE_COUNT 5

CollisionQueue collisionQueue;

Player players_interpolated[MAX_CLIENTS];
Bullet bullets_interpolated[BULLETS_DEFAULT_CAPACITY]; //NOTE: if we want to fit more bullets that capacitry, this needs to be a dynamic array

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

#define PLAYER_SPRITE_WIDTH 128.0f
#define PLAYER_SPRITE_HEIGHT 96.0f
#define SHIP_SMOKE_WIDTH 128.0f
#define SHIP_SMOKE_HEIGHT 128.0f
#define SHIP_SMOKE_FRAME_COUNT 8
#define BULLET_SMOKE_WIDTH 64.0f
#define BULLET_SMOKE_HEIGHT 64.0f
#define BULLET_SMOKE_FRAME_COUNT 6
#define RESPAWN_TIMER_WIDTH 128.0f
#define RESPAWN_TIMER_HEIGHT 64.0f
#define RESPAWN_TIMER_FRAME_COUNT 4

//#define PLAYER_SPRITE_WIDTH 640.0f
//#define PLAYER_SPRITE_HEIGHT 360.0f


float speed = 0.0f;
float rotation_speed = 0.05f;
float max_speed = 130.0f;
char player_can_move = true;
float player_collision_fixed_direction_duration = 1.3f;
float player_collision_fixed_direction_timer = 0.0f;
vec2 player_dir = {0.f, 0.f};
float delta_time;
float total_time;
Uint32 previous_time;

//SDL_Texture* player_sprites_map[MAX_CLIENTS];
const char* player_sprite_files[MAX_CLIENTS] = {
    "../resources/sprites/boat-01.bmp",
    "../resources/sprites/boat-02.bmp",
    "../resources/sprites/boat-03.bmp",
    "../resources/sprites/boat-04.bmp",
    "../resources/sprites/boat-05.bmp"
};
const char* water_file = "../resources/sprites/water_noise.bmp";
const char* hp_bar_file = "../resources/sprites/boat-hp-bar-01.bmp";
const char* bullet_sprite_file = "../resources/sprites/bullet.bmp";
const char* foam_file = "../resources/sprites/foam1.bmp";
const char* water_frag = "../resources/shaders/water_frag.glsl";
const char* water_vert = "../resources/shaders/water_vert.glsl";
GLuint player_texture_map[MAX_CLIENTS];
#define INVALID_PLAYER_TEXTURE 4294967295 //? no idea if i can do that
GLuint water_texture;
GLuint bullet_texture;
GLuint foam_texture;
GLuint hp_bar_texture;

SDL_Texture *LoadTexture(SDL_Renderer *renderer, const char *file, bool transparent);
static bool InitShaders(void);

typedef struct
{
    GLhandleARB program;
    GLhandleARB vert_shader;
    GLhandleARB frag_shader;
    const char *vert_source;
    const char *frag_source;
} ShaderData;

enum
{
    PLAYER_SHADER = 0,
    WATER_SHADER,
    BULLET_SHADER,
    HP_BAR_SHADER,
    NUM_SHADERS
};
ShaderData shaders[NUM_SHADERS];

GLuint gProgramID = 0;
GLint gVertexPos2DLocation = -1;
GLuint gVBO = 0;
GLuint gIBO = 0;

const char* ship_smoke_spritesheet = "../resources/sprites/smoke-ship-Sheet.bmp";
const char* bullet_smoke_spritesheet = "../resources/sprites/bullet-smoke-2-Sheet.bmp";
const char* respawn_timer_spritesheet = "../resources/sprites/respawn_timer-Sheet.bmp";
GLuint ship_smoke_texture;
GLuint bullet_smoke_texture;
GLuint respawn_timer_texture;

void init_opengl() {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    window = SDL_CreateWindow(GAME_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create OpenGL window: %s", SDL_GetError());
        SDL_Quit();
        exit(2);
    }

    if (!SDL_GL_CreateContext(window)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create OpenGL context: %s", SDL_GetError());
        SDL_Quit();
        exit(2);
    }
    
    //Use Vsync
    if(SDL_GL_SetSwapInterval(1) < 0)
    {
        printf( "Warning: Unable to set vsync: %s\n", SDL_GetError() );
    }

    InitShaders();
}


float to_degrees(float radians) {
    return radians * (180.f / PI);
}

char* load_shader_file(const char* shader_file_path) {
    printf("LOADING SHADER: %s\n", shader_file_path);
    SDL_IOStream* rw = SDL_IOFromFile(shader_file_path, "rb");
    if (!rw) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not open shader file: %s", shader_file_path);
        return NULL;
    }

    Sint64 fileSize = SDL_GetIOSize(rw);
    if (fileSize <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid file size for shader file: %s", shader_file_path);
        SDL_CloseIO(rw);
        return NULL;
    }

    char* buffer = (char*)SDL_malloc((size_t)fileSize + 1);
    if (!buffer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory for shader file: %s", shader_file_path);
        SDL_CloseIO(rw);
        return NULL;
    }

    size_t readTotal = SDL_ReadIO(rw, buffer, (size_t)fileSize);
    if (readTotal != (size_t)fileSize) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read shader file: %s (read %zu bytes, expected %ld bytes)",
                     shader_file_path, readTotal, fileSize);
        SDL_free(buffer);
        SDL_CloseIO(rw);
        return NULL;
    }

    buffer[fileSize] = '\0';
    SDL_CloseIO(rw);
    return buffer;
}

static PFNGLATTACHOBJECTARBPROC pglAttachObjectARB;
static PFNGLCOMPILESHADERARBPROC pglCompileShaderARB;
static PFNGLCREATEPROGRAMOBJECTARBPROC pglCreateProgramObjectARB;
static PFNGLCREATESHADEROBJECTARBPROC pglCreateShaderObjectARB;
static PFNGLDELETEOBJECTARBPROC pglDeleteObjectARB;
static PFNGLGETINFOLOGARBPROC pglGetInfoLogARB;
static PFNGLGETOBJECTPARAMETERIVARBPROC pglGetObjectParameterivARB;

static PFNGLGETUNIFORMLOCATIONARBPROC pglGetUniformLocationARB;
static PFNGLUNIFORM1IARBPROC pglUniform1iARB;
static PFNGLUNIFORM1FARBPROC pglUniform1fARB;
static PFNGLUNIFORMMATRIX4FVARBPROC pglUniformMat4fvARB;

static PFNGLUNIFORM2FARBPROC pglUniform2fARB;
static PFNGLUNIFORM3FARBPROC pglUniform3fARB;
static PFNGLUNIFORM4FARBPROC pglUniform4fARB;

static PFNGLLINKPROGRAMARBPROC pglLinkProgramARB;
static PFNGLSHADERSOURCEARBPROC pglShaderSourceARB;
static PFNGLUSEPROGRAMOBJECTARBPROC pglUseProgramObjectARB;

bool CompileShaderFromFile(GLhandleARB shader, const char *filePath)
{
    char* source = load_shader_file(filePath);
    if (!source) {
        return false;
    }
#if PRINT_SHADER_COMPILATION_INFO
    printf("compiling: %s", source);
#endif
    GLint status = 0;
    pglShaderSourceARB(shader, 1, (const GLcharARB **)&source, NULL);
    pglCompileShaderARB(shader);
    pglGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);

    if (status == 0) {
        GLint length = 0;
        pglGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
        char *info = (char *)SDL_malloc((size_t)length + 1);
        if (info) {
            pglGetInfoLogARB(shader, length, NULL, info);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to compile shader %s:", filePath);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", info);
            SDL_free(info);
        }
        SDL_free(source);
        return false;
    }

    SDL_free(source);
    return true;
}


static bool LinkProgram(ShaderData *data)
{
    GLint status = 0;

    pglAttachObjectARB(data->program, data->vert_shader);
    pglAttachObjectARB(data->program, data->frag_shader);
    pglLinkProgramARB(data->program);

    pglGetObjectParameterivARB(data->program, GL_OBJECT_LINK_STATUS_ARB, &status);
    if (status == 0) {
        GLint length = 0;
        char *info;

        pglGetObjectParameterivARB(data->program, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
        info = (char *)SDL_malloc((size_t)length + 1);
        if (!info) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!");
        } else {
            pglGetInfoLogARB(data->program, length, NULL, info);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to link program:");
	    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", info);
            SDL_free(info);
        }
        return false;
    } else {
        return true;
    }
}

static bool CompileShaderProgram(ShaderData *data)
{
    const int num_tmus_bound = 4;
    GLint location;

    glGetError();

    data->program = pglCreateProgramObjectARB();
    data->vert_shader = pglCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    if(!CompileShaderFromFile(data->vert_shader, data->vert_source)) {
        return false;
    }
    data->frag_shader = pglCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
    if(!CompileShaderFromFile(data->frag_shader, data->frag_source)) {
        return false;
    }
    if (!LinkProgram(data)) {
        return false;
    }
   
    // set up uniforms
    pglUseProgramObjectARB(data->program);

    // Bind texture samplers
    for (int i = 0; i < num_tmus_bound; ++i) {
        char tex_name[5];
        (void)SDL_snprintf(tex_name, SDL_arraysize(tex_name), "tex%d", i);
        location = pglGetUniformLocationARB(data->program, tex_name);
        if (location >= 0) {
            pglUniform1iARB(location, i); // Bind texture unit i to the sampler
        }
    }

    pglUseProgramObjectARB(0);

    return (glGetError() == GL_NO_ERROR);
}

static void DestroyShaderProgram(ShaderData *data)
{
    pglDeleteObjectARB(data->vert_shader);
    pglDeleteObjectARB(data->frag_shader);
    pglDeleteObjectARB(data->program);
}

static bool InitShaders(void)
{
    if (SDL_GL_ExtensionSupported("GL_ARB_shader_objects") &&
        SDL_GL_ExtensionSupported("GL_ARB_shading_language_100") &&
        SDL_GL_ExtensionSupported("GL_ARB_vertex_shader") &&
        SDL_GL_ExtensionSupported("GL_ARB_fragment_shader")) {
        pglAttachObjectARB = (PFNGLATTACHOBJECTARBPROC)SDL_GL_GetProcAddress("glAttachObjectARB");
        pglCompileShaderARB = (PFNGLCOMPILESHADERARBPROC)SDL_GL_GetProcAddress("glCompileShaderARB");
        pglCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC)SDL_GL_GetProcAddress("glCreateProgramObjectARB");
        pglCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC)SDL_GL_GetProcAddress("glCreateShaderObjectARB");
        pglDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC)SDL_GL_GetProcAddress("glDeleteObjectARB");
        pglGetInfoLogARB = (PFNGLGETINFOLOGARBPROC)SDL_GL_GetProcAddress("glGetInfoLogARB");
        pglGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC)SDL_GL_GetProcAddress("glGetObjectParameterivARB");
        pglGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC)SDL_GL_GetProcAddress("glGetUniformLocationARB");
        pglLinkProgramARB = (PFNGLLINKPROGRAMARBPROC)SDL_GL_GetProcAddress("glLinkProgramARB");
        pglShaderSourceARB = (PFNGLSHADERSOURCEARBPROC)SDL_GL_GetProcAddress("glShaderSourceARB");
        pglUniform1iARB = (PFNGLUNIFORM1IARBPROC)SDL_GL_GetProcAddress("glUniform1iARB");
        pglUniform1fARB = (PFNGLUNIFORM1FARBPROC)SDL_GL_GetProcAddress("glUniform1fARB");
        pglUniform2fARB = (PFNGLUNIFORM2FARBPROC)SDL_GL_GetProcAddress("glUniform2fARB");
        pglUniform3fARB = (PFNGLUNIFORM3FARBPROC)SDL_GL_GetProcAddress("glUniform3fARB");
        pglUniform4fARB = (PFNGLUNIFORM4FARBPROC)SDL_GL_GetProcAddress("glUniform4fARB");
        pglUniformMat4fvARB = (PFNGLUNIFORMMATRIX4FVARBPROC)SDL_GL_GetProcAddress("glUniform4fARB");
        pglUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC)SDL_GL_GetProcAddress("glUseProgramObjectARB");
    }

    shaders[PLAYER_SHADER].vert_source = "../resources/shaders/boat_vert.glsl";
    shaders[PLAYER_SHADER].frag_source = "../resources/shaders/boat_frag.glsl";
    shaders[BULLET_SHADER].vert_source = "../resources/shaders/bullet_vert.glsl";
    shaders[BULLET_SHADER].frag_source = "../resources/shaders/bullet_frag.glsl";
    shaders[HP_BAR_SHADER].vert_source = "../resources/shaders/hp_vert.glsl";
    shaders[HP_BAR_SHADER].frag_source = "../resources/shaders/hp_frag.glsl";
    shaders[WATER_SHADER].vert_source = "../resources/shaders/water_vert.glsl";
    shaders[WATER_SHADER].frag_source = "../resources/shaders/water_frag.glsl";

    for (int i = 0; i < NUM_SHADERS; ++i) {
        if (!CompileShaderProgram(&shaders[i])) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to compile shader!");
            return false;
        }
    }

    GLhandleARB program = shaders[WATER_SHADER].program;
    pglUseProgramObjectARB(program);
    GLint timeScaleLocation = pglGetUniformLocationARB(program, "u_timeScale");
    //printf("found:%d", timeScaleLocation);
    GLint amplitudeLocation = pglGetUniformLocationARB(program, "u_initialAmplitude");
    GLint amplitudeGainLocation = pglGetUniformLocationARB(program, "u_amplitudeGain");
    GLint rotationAngleLocation = pglGetUniformLocationARB(program, "u_rotationAngle");
    GLint colorMixFactorLocation = pglGetUniformLocationARB(program, "u_colorMixFactor");

    if (timeScaleLocation >= 0) {
        pglUniform1fARB(timeScaleLocation, 1.0f);
    }
    if (amplitudeLocation >= 0) {
        pglUniform1fARB(amplitudeLocation, 0.3f);
    }
    if (amplitudeGainLocation >= 0) {
        pglUniform1fARB(amplitudeGainLocation, 0.7f);
    }
    if (rotationAngleLocation >= 0) {
        pglUniform1fARB(rotationAngleLocation, 0.5f);
    }
    if (colorMixFactorLocation >= 0) {
        pglUniform1fARB(colorMixFactorLocation, 4.0f);
    }
    pglUseProgramObjectARB(0);

    return true;
}

static void QuitShaders(void)
{
    for (int i = 0; i < NUM_SHADERS; ++i) {
        DestroyShaderProgram(&shaders[i]);
    }
}

static int power_of_two(int input)
{
    int value = 1;

    while (value < input) {
        value <<= 1;
    }
    return value;
}

static GLuint SDL_GL_LoadTexture(SDL_Surface *surface)
{
    GLuint texture;
    int w, h;
    SDL_Surface *image;
    SDL_Rect area;
    SDL_BlendMode saved_mode;

    w = power_of_two(surface->w);
    h = power_of_two(surface->h);

    image = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!image) {
        printf("cant create surface from image");
        return 0;
    }

    SDL_GetSurfaceBlendMode(surface, &saved_mode);
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);

    area.x = 0;
    area.y = 0;
    area.w = surface->w;
    area.h = surface->h;
    SDL_BlitSurface(surface, &area, image, &area);

    SDL_SetSurfaceBlendMode(surface, saved_mode);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    SDL_DestroySurface(image);

    return texture;
}

void update_shader_data() {
    //NOTE: the variables have to be used in a meaningful way in the shaders for the compilator not to optimise them out
    pglUseProgramObjectARB(shaders[WATER_SHADER].program);
    int location = pglGetUniformLocationARB(shaders[WATER_SHADER].program, "u_time");
    if (location >= 0) {
        pglUniform1fARB(location, total_time);
    }
    location = pglGetUniformLocationARB(shaders[WATER_SHADER].program, "u_playerPos");
    if (location >= 0) {
        float cX = local_player.x + PLAYER_SPRITE_WIDTH / 2;
        float cY = local_player.y + PLAYER_SPRITE_HEIGHT / 2;
        float x = cX / 1280.0f;
        float y = cY / 720.0f;
        pglUniform2fARB(location, x, y);
    }
    //printf("found: %d", location);
    location = pglGetUniformLocationARB(shaders[WATER_SHADER].program, "u_aspectRatio");
    if (location >= 0) {
        float aspectRatioX = 1280.0f / 720.0f;
        float aspectRatioY = 720.0f / 1280.0f; 
        pglUniform2fARB(location, aspectRatioX, aspectRatioY);
    }
    //printf("found: %d", location);
    location = pglGetUniformLocationARB(shaders[WATER_SHADER].program, "u_foamRadiusWorld");
    if (location >= 0) {
        pglUniform1fARB(location, 500.0f / 1280.0f);
    }

    location = pglGetUniformLocationARB(shaders[WATER_SHADER].program, "u_playerRotation");
    if (location >= 0) {
        pglUniform1fARB(location, local_player.rotation);
    }

    pglUseProgramObjectARB(0);

    pglUseProgramObjectARB(shaders[BULLET_SHADER].program);
    location = pglGetUniformLocationARB(shaders[BULLET_SHADER].program, "u_time");
    if (location >= 0) {
        pglUniform1fARB(location, total_time);
    }
    pglUseProgramObjectARB(0);

    pglUseProgramObjectARB(shaders[HP_BAR_SHADER].program);
    location = pglGetUniformLocationARB(shaders[HP_BAR_SHADER].program, "u_hpPercentage");
    if (location >= 0) {
        pglUniform1fARB(location, (float)local_player.hp / INITIAL_PLAYER_HP);
    }
    pglUseProgramObjectARB(0);
}

static void InitGL(int Width, int Height)
{
    int windowWidth, windowHeight;
    SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glDepthFunc(GL_LESS); 
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    //glOrtho(0.0, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0, -1.0, 1.0);
    //glOrtho(0.0, Width, Height, 0.0, -1.0, 1.0);

    glOrtho(0.0, windowWidth, windowHeight, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
}

void SetCamera(float camera_x, float camera_y) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(camera_x - WINDOW_WIDTH / 2, camera_x + WINDOW_WIDTH / 2,
            camera_y + WINDOW_HEIGHT / 2, camera_y - WINDOW_HEIGHT / 2,
            -1.0, 1.0);

    GLdouble aspect = (GLdouble)WINDOW_WIDTH / WINDOW_HEIGHT;
    //glOrtho(0.0, 2.0, -2, 2.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void DrawSprite(GLuint texture, float x, float y, 
    float width, float height, float angle, float z) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glEnable(GL_TEXTURE_2D);
    glPushMatrix();
    // rotate around origin
    //width *= 1.63f;
    glTranslatef(x, y, z);
    glTranslatef(width / 2.0f, height / 2.0f, 0.0f);
    glRotatef((GLfloat)to_degrees(angle), 0.0f, 0.0f, 1.0f);
    glTranslatef(-width / 2.0f, -height / 2.0f, 0.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);            
    glTexCoord2f(1.0f, 0.0f); glVertex2f(width, 0.0f);            
    glTexCoord2f(1.0f, 1.0f); glVertex2f(width, height);   
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, height);            
    glEnd();
    
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);
}

void DrawSpriteFrame(GLuint texture, float x, float y, float width, float height, float angle, float z,
                     int frameIndex, int frameCols, int frameRows)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glEnable(GL_TEXTURE_2D);
    glPushMatrix();

    glTranslatef(x, y, z);
    glTranslatef(width / 2.0f, height / 2.0f, 0.0f);
    glRotatef(to_degrees(angle), 0.0f, 0.0f, 1.0f);
    glTranslatef(-width / 2.0f, -height / 2.0f, 0.0f);

    float tex_w = 1.0f / frameCols;
    float tex_h = 1.0f / frameRows;

    int col = frameIndex % frameCols;
    int row = frameIndex / frameCols;

    float u = col * tex_w;
    float v = row * tex_h;

    glBegin(GL_QUADS);
    glTexCoord2f(u, v); glVertex2f(0.0f, 0.0f);
    glTexCoord2f(u+tex_w, v); glVertex2f(width, 0.0f);
    glTexCoord2f(u+tex_w, v+tex_h); glVertex2f(width, height);
    glTexCoord2f(u, v+tex_h); glVertex2f(0.0f, height);
    glEnd();

    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
}


void draw_rotated_bounding_box(const RotatedRect* rect, float z) {
    vec2 corners[4];
    get_rotated_rect_corners(rect, corners);
    
    glDisable(GL_TEXTURE_2D);
    glColor3f(1.0f, 0.0f, 0.0f);
    glLineWidth(2.0f);
    
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 4; i++) {
        glVertex3f(corners[i][0], corners[i][1], z);
    }
    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(1.0f);
    glEnable(GL_TEXTURE_2D);
}

void draw_collision_effects() {
    pglUseProgramObjectARB(shaders[BULLET_SHADER].program);
    for(int i = 0; i < collisionQueue.size; ++i) {
        int index = (collisionQueue.front + i);
        CollisionInfo* info = &collisionQueue.data[index];

        if(info->type == COLLISION_TYPE_PLAYER) {
            float x = info->x - SHIP_SMOKE_WIDTH / 2;
            float y = info->y - SHIP_SMOKE_HEIGHT / 2;
            int frame = (int)(((PLAYER_COLLISION_ANIM_TIME - info->timer) / PLAYER_COLLISION_ANIM_TIME) * SHIP_SMOKE_FRAME_COUNT) % SHIP_SMOKE_FRAME_COUNT;
            //printf("Collision at (%f, %f) with timer %f\n", info->x, info->y, info->timer);
            DrawSpriteFrame(ship_smoke_texture, x, y, SHIP_SMOKE_WIDTH, 
                SHIP_SMOKE_HEIGHT, 0.0f, PLAYER_COLLISION_MIN, frame, SHIP_SMOKE_FRAME_COUNT, 1);
        } else {
            float x = info->x - BULLET_SMOKE_WIDTH / 2;
            float y = info->y - BULLET_SMOKE_HEIGHT / 2;
            int frame = (int)(((BULLET_COLLISION_ANIM_TIME - info->timer) / BULLET_COLLISION_ANIM_TIME) * BULLET_SMOKE_FRAME_COUNT) % BULLET_SMOKE_FRAME_COUNT;
            //printf("Collision BULLET at (%f, %f) with timer %f\n", info->x, info->y, info->timer);
            DrawSpriteFrame(bullet_smoke_texture, x, y, BULLET_SMOKE_WIDTH, 
                BULLET_SMOKE_HEIGHT, 0.0f, BULLET_COLLISION_MIN, frame, BULLET_SMOKE_FRAME_COUNT, 1);
        }
    }
    /*
    float x = players_interpolated[player_id].x - PLAYER_SPRITE_WIDTH / 2;
    float y = players_interpolated[player_id].y - PLAYER_SPRITE_HEIGHT / 2;
    printf("collision drawn for : %d at pos : %f %f\n", player_id, x, y);

    DrawSprite(bullet_texture, x, y, BULLET_SPRITE_WIDTH, 
           BULLET_SPRITE_HEIGHT, 0.0f, PLAYER_COLLISION_MIN);*/
    pglUseProgramObjectARB(0);
}

void draw_players() {
    glColor3f(1.0f, 1.0f, 1.0f);
    //pglUseProgramObjectARB(shaders[PLAYER_SHADER].program);
    for(int i = 0; i < MAX_CLIENTS; ++i) {
        if(players[i].id == INVALID_PLAYER_ID) continue;
        pglUseProgramObjectARB(shaders[PLAYER_SHADER].program);

        float x = players_interpolated[i].x - PLAYER_SPRITE_WIDTH / 2;
        float y = players_interpolated[i].y - PLAYER_SPRITE_HEIGHT / 2;
        float angle = players_interpolated[i].rotation;
        //DrawSprite(player_texture_map[i], x, y, PLAYER_SPRITE_WIDTH, 
           //PLAYER_SPRITE_HEIGHT, angle, PLAYER_Z_COORD_MIN + PLAYER_Z_COORD_MULTIPLIER * i);
            
        DrawSpriteFrame(player_texture_map[i], x, y, PLAYER_SPRITE_WIDTH, 
           PLAYER_SPRITE_HEIGHT, angle, 
           PLAYER_Z_COORD_MIN + PLAYER_Z_COORD_MULTIPLIER * i,
        0, 1, 1);
            
        //DrawSprite(foam_texture, x, y, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT, angle, PLAYER_Z_COORD_MIN - 0.1);
            
        pglUseProgramObjectARB(0);

        /*
        RotatedRect bbox = {
            { players[i].x, players[i].y },
            { PLAYER_HITBOX_WIDTH/2, PLAYER_HITBOX_HEIGHT/2 },
            players[i].rotation
        };
        draw_rotated_bounding_box(&bbox, 0.4f + 0.05 * i);
        */
        
        if ((players[i].collision_byte & PLAYER_COLLISION_BITS) != 0) {
            printf("byte: %d\n", players[i].collision_byte >> i);
            CollisionInfo info = {players[i].x, players[i].y, PLAYER_COLLISION_ANIM_TIME, COLLISION_TYPE_PLAYER};
            enqueue_collision(&collisionQueue, info);
        }
        if (players[i].collision_byte & BULLET_COLLISION_MASK) {
            printf("BULLET COLLISION: %d\n", players[i].id);
            CollisionInfo info = {players[i].x, players[i].y, BULLET_COLLISION_ANIM_TIME, COLLISION_TYPE_BULLET};
            enqueue_collision(&collisionQueue, info);
        }
        //!printf("wohle: %d\n", players[i].collision_byte);
        /*
        // no i+1 here cause shift by 0 to right is valid for bit nr 0
        for(int j = 0; j < MAX_CLIENTS; ++j) {
            if(players[j].id == INVALID_PLAYER_ID) continue;
            if((players[i].collision_byte >> j) & 0x01) {
                printf("byte: %d\n", players[i].collision_byte >> i);
                //! probably j not i?
                CollisionInfo info = {players[i].x, players[i].y, PLAYER_COLLISION_ANIM_TIME, COLLISION_TYPE_PLAYER};
                enqueue_collision(&collisionQueue, info);
            }
        }*/
    }
    //pglUseProgramObjectARB(0);
}

void draw_bullets() {
    glColor3f(1.0f, 1.0f, 1.0f);
    //pglUseProgramObjectARB(shaders[BULLET_SHADER].program);
    for(int i = 0; i < existing_bullets; ++i) {
        pglUseProgramObjectARB(shaders[BULLET_SHADER].program);

        float x = bullets[i].x - BULLET_SPRITE_WIDTH / 2;
        float y = bullets[i].y - BULLET_SPRITE_HEIGHT / 2;
        DrawSprite(bullet_texture, x, y, BULLET_SPRITE_WIDTH, 
           BULLET_SPRITE_HEIGHT, 0.0f, BULLET_Z_COORD_MIN + BULLET_Z_COORD_MULTIPLIER * i);
            
        pglUseProgramObjectARB(0);

        /*
        RotatedRect bbox = {
            { bullets[i].x, bullets[i].y },
            { BULLET_HITBOX_WIDTH/2, BULLET_HITBOX_HEIGHT/2 },
            0.0f
        };
        draw_rotated_bounding_box(&bbox, 0.4f + 0.05 * i);
        */
    }
    //pglUseProgramObjectARB(0);
}


void draw_water() {
    float sprite_width = WINDOW_WIDTH;
    float sprite_height = WINDOW_HEIGHT;
    pglUseProgramObjectARB(shaders[WATER_SHADER].program);
    glColor3f(0.1f, 0.5f, 0.8f);

    DrawSprite(water_texture, 0.0f, 0.0f, sprite_width, 
            sprite_height, 0.0f, 0.0f);
    pglUseProgramObjectARB(0);
    //printf("water tex%u\n",  water_texture);
}

void draw_ui() {
    pglUseProgramObjectARB(shaders[HP_BAR_SHADER].program);
    glColor3f(1.0f, 1.0f, 1.0f);

    DrawSprite(hp_bar_texture, 20.0f, 20.0f, PLAYER_SPRITE_WIDTH, 
            PLAYER_SPRITE_HEIGHT, 0.0f, 0.95f);
    pglUseProgramObjectARB(0);
    
    if(is_dead) {
        pglUseProgramObjectARB(shaders[PLAYER_SHADER].program);
        int frame = RESPAWN_TIMER_FRAME_COUNT - 1;
        if(respawn_timer > 0.f) frame = (int)(((RESPAWN_COOLDOWN - respawn_timer) / RESPAWN_COOLDOWN) * RESPAWN_TIMER_FRAME_COUNT) % RESPAWN_TIMER_FRAME_COUNT;
        DrawSpriteFrame(respawn_timer_texture, 20.0f, 130.0f, RESPAWN_TIMER_WIDTH, 
                RESPAWN_TIMER_HEIGHT, 0.0f, 0.95f, frame, RESPAWN_TIMER_FRAME_COUNT, 1);
        pglUseProgramObjectARB(0);
    }
    //printf("water tex%u\n",  water_texture);
}

void draw_scene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //SetCamera(local_player.x, local_player.y);

    draw_water();
    draw_bullets();
    draw_players();
    draw_collision_effects();
    draw_ui();

    SDL_GL_SwapWindow(window);
}

void load_player_sprite(int sprite_id) {
    printf("GOT %d\n", sprite_id);
    if(sprite_id == INVALID_PLAYER_ID) {
        printf("Tried to load a player sprite for an invalid sprite ID.");
        return;
    };
    if(sprite_id > MAX_CLIENTS || sprite_id < 0) return;
    printf("LOADING BMP %s\n", player_sprite_files[sprite_id]);
    fflush(stdout);
    SDL_Surface* surface = SDL_LoadBMP(player_sprite_files[sprite_id]);
    if (!surface) {
        printf("CANT LOAD\n");
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to load %s: %s", player_sprite_files[sprite_id], SDL_GetError());
        SDL_Quit();
        exit(3);
    }
    player_texture_map[sprite_id] = SDL_GL_LoadTexture(surface);
    SDL_DestroySurface(surface);
    printf("Loaded map for id: %d --> %u\n", sprite_id, player_texture_map[sprite_id]);
    fflush(stdout);
    return;
}

static GLuint LoadTextureFromBMP(const char* file_path) {
    SDL_Surface* surface = SDL_LoadBMP(file_path);
    if (!surface) {
        printf("Failed to load BMP: %s\n", file_path);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to load %s: %s", file_path, SDL_GetError());
        return 0; // Return 0 to indicate failure
    }

    GLuint texture = SDL_GL_LoadTexture(surface);
    printf("LOADED BMP : %s\n", file_path);
    SDL_DestroySurface(surface);
    return texture;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) == false) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }
    init_collision_queue(&collisionQueue);
    init_opengl();

    for(int i = 0; i <MAX_CLIENTS; ++i) {
        player_texture_map[i] = INVALID_PLAYER_TEXTURE;
    }
    bullets = (Bullet*)malloc(sizeof(Bullet) * BULLETS_DEFAULT_CAPACITY);
    bullets_last = (Bullet*)calloc(BULLETS_DEFAULT_CAPACITY, sizeof(Bullet));

    water_texture = LoadTextureFromBMP(water_file);
    bullet_texture = LoadTextureFromBMP(bullet_sprite_file);
    foam_texture = LoadTextureFromBMP(foam_file);
    ship_smoke_texture = LoadTextureFromBMP(ship_smoke_spritesheet);
    bullet_smoke_texture = LoadTextureFromBMP(bullet_smoke_spritesheet);
    hp_bar_texture = LoadTextureFromBMP(hp_bar_file);
    respawn_timer_texture = LoadTextureFromBMP(respawn_timer_spritesheet);

    InitGL(WINDOW_WIDTH, WINDOW_HEIGHT);
    connect_to_server();

    while(is_player_initialized == false) {};

    //load_player_sprite(local_player_data.sprite_id);

    previous_time = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float lerp_halflife(float a, float b, float t, float p, float dt) {
    float hl = -t/log2f(p);
    return lerp(a, b, 1-exp2f(-dt/hl));
}

void update_players(float dt) {
    float precision = 1.0f/100.0f;
    float t = dt / GAME_STATE_UPDATE_FRAME_DELAY;

    for(int i = 0; i < MAX_CLIENTS; ++i) {
        if(players[i].id == INVALID_PLAYER_ID) continue;
#if USE_HL_LERP
        players_interpolated[i].x = lerp_halflife(players_last[i].x, players[i].x, t, precision, dt);
        players_interpolated[i].y = lerp_halflife(players_last[i].y, players[i].y, t, precision, dt);
        players_interpolated[i].rotation = lerp_halflife(players_last[i].rotation, players[i].rotation, t, precision, dt);
#else
        players_interpolated[i].x = lerp(players_last[i].x, players[i].x, t);
        players_interpolated[i].y = lerp(players_last[i].y, players[i].y, t);
        players_interpolated[i].rotation = lerp(players_last[i].rotation, players[i].rotation, t);
#endif
    }
    /*
    for(int i = 0; i < existing_bullets; ++i) {
        if(bullets_last[i].id != bullets[i].id) {
            //! found a killed bullet 
            //TODO: need handling or nah?
            // override or set position to something out of the screen to be sure its not visible
            //bullets_last[i] = bullets[i];
        }
        //bullets_interpolated[i].x = lerp(bullets_last[i].x, bullets[i].x, t);
        //bullets_interpolated[i].y = lerp(bullets_last[i].y, bullets[i].y, t);
        bullets_interpolated[i].x = bullets[i].x;
        bullets_interpolated[i].y = bullets[i].y;
    }*/
}

void render_game() {
    //render_players();
    draw_scene();
}


void update_death_state() {
    if(local_player.hp <= 0) {
        if(!is_dead) {
            respawn_timer = RESPAWN_COOLDOWN;
            is_dead = true;
            local_player.x = DEAD_PLAYER_POS_X;
            local_player.y = DEAD_PLAYER_POS_Y;
        }
    } 
    else is_dead = false;
}

void reflect_player_direction(vec2 collision_normal) {
    float v_dot_n = player_dir[0] * collision_normal[0] + player_dir[1] * collision_normal[1];
    player_dir[0] = player_dir[0] - 2.0f * v_dot_n * collision_normal[0];
    player_dir[1] = player_dir[1] - 2.0f * v_dot_n * collision_normal[1];
    local_player.rotation = atan2f(player_dir[1], player_dir[0]);
}

void update_local_player(float dt) {
    if((local_player.collision_byte & PLAYER_COLLISION_BITS) != 0) {
        player_can_move = false;
        player_collision_fixed_direction_timer = player_collision_fixed_direction_duration;
        
        int index = -1;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (local_player.collision_byte & (char)(1 << i)) {
                index = i;
            }
        }
        if(index == -1) {
            printf("how index -1\n");
            return;
        }
        printf("found collsion with index: %d \n", index);

        Player* other_player = &players[index];
        RotatedRect bbox1 = {
            { local_player.x, local_player.y },
            { PLAYER_HITBOX_WIDTH / 2.0f, PLAYER_HITBOX_HEIGHT / 2.0f },
            local_player.rotation
        };
        RotatedRect bbox2 = {
            { other_player->x, other_player->y },
            { PLAYER_HITBOX_WIDTH / 2.0f, PLAYER_HITBOX_HEIGHT / 2.0f },
            other_player->rotation
        };
        vec2 normal;
        sat_obb_collision_check(&bbox1, &bbox2, &normal);
        reflect_player_direction(normal);


        printf("PLAYER COLLIDED CANT MOVE \n");
    }
    if(player_collision_fixed_direction_timer > 0.f) {
        player_collision_fixed_direction_timer -= dt;
        if (player_collision_fixed_direction_timer < 0.f) {
            player_can_move = true;
            printf("PLAYER CAN MOVE AGAIN \n");
        }
    }
    //*printf("can move? : %d \n", player_can_move);

    const bool *state = SDL_GetKeyboardState(NULL);
    update_death_state();
    if (is_dead) {
        // if is dead, process respawn action
        if (state[SDL_SCANCODE_R]){
            //printf("trying respawn\n");
            if(respawn_timer < 0.0f) {
                //printf("SENT RESPAWN");
                local_player.action = PLAYER_ACTION_RESPAWN;
            }
        }
        return;
    }

   
    float rot = local_player.rotation;
    if(player_can_move) {
        if (state[SDL_SCANCODE_W]) {
            speed = max_speed;
        } else {
            speed = 0.0f;
        }
        if (state[SDL_SCANCODE_S]) {
            speed = -max_speed;
        } 
        if (state[SDL_SCANCODE_A]) {
            rot -= rotation_speed;
        }
        if (state[SDL_SCANCODE_D]) {
            rot += rotation_speed;
        }
    }

    // if not alive, don't process this so we don't replace respawn action accidentally
    if (state[SDL_SCANCODE_Q]){
        if(shoot_timer < 0.0f)
            local_player.action = PLAYER_ACTION_SHOOT_RIGHT;
    }
    else if (state[SDL_SCANCODE_E]){
        if(shoot_timer < 0.0f)
            local_player.action = PLAYER_ACTION_SHOOT_LEFT;
    }

    rot = fmodf(rot, 2.0f * PI);
    if (rot < 0.0f) {
        rot += 2.0f * PI;
    }

    player_dir[0] = cosf(rot);
    player_dir[1] = sinf(rot);
    local_player.rotation = rot;
    local_player.x += player_dir[0] * speed * dt;
    local_player.y += player_dir[1] * speed * dt;
}

void update_game(float dt) {
    update_local_player(dt);
    update_players(dt);
}

void update_timers() {
    Uint32 current_time = SDL_GetTicks();
    delta_time = (current_time - previous_time) / 1000.0f;
    previous_time = current_time;
    total_time += delta_time;
    total_time = fmodf(total_time, 3600.0f);
    shoot_timer -= delta_time;
    respawn_timer -= delta_time;
    update_collision_queue(&collisionQueue, delta_time);
    //printf("respawn time: %f delta: %f\n", respawn_timer, delta_time);
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    update_timers();
    update_shader_data();

    //if(should_update_sprites) {
    for(int i = 0; i < MAX_CLIENTS; ++i) {
        if(players[i].id == INVALID_PLAYER_ID) continue;
        if(player_texture_map[i] != INVALID_PLAYER_TEXTURE) continue;
        if(player_data[i].sprite_id != players[i].id) player_data[i].sprite_id = players[i].id;
        load_player_sprite(player_data[i].sprite_id);
    }
    should_update_sprites = 0;
   // }

    if(is_update_locked) {
        return SDL_APP_CONTINUE;
    } 
    update_game(delta_time);
    render_game();
    
    //printf("processed %d, %d\n", local_player.x, local_player.y);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    QuitShaders();
}