#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client/client.h"
#include "game_window.h"

#define GAME_TITLE "Bitwa Piracka"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define PLAYER_SPRITE_COUNT 5
Player players_interpolated[MAX_CLIENTS];

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

#define PLAYER_SPRITE_WIDTH 128.0f
#define PLAYER_SPRITE_HEIGHT 64.0f


float speed = 0.0f;
float rotation_speed = 0.005f;
float max_speed = 50.0f;
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
const char* water_frag = "../resources/shaders/water_frag.glsl";
const char* water_vert = "../resources/shaders/water_vert.glsl";
GLuint player_texture_map[MAX_CLIENTS];
#define INVALID_PLAYER_TEXTURE 4294967295 //? no idea if i can do that
GLfloat texcoords[MAX_CLIENTS][4];
GLuint water_texture;
GLfloat texcoords_water[4];

SDL_Texture *LoadTexture(SDL_Renderer *renderer, const char *file, bool transparent);

static bool shaders_supported;
int current_shader = 1;

float to_degrees(float radians) {
    return radians * (180.f / PI);
}

enum
{
    SHADER_COLOR,
    SHADER_TEXTURE,
    SHADER_TEXCOORDS,
    NUM_SHADERS
};

typedef struct
{
    GLhandleARB program;
    GLhandleARB vert_shader;
    GLhandleARB frag_shader;
    const char *vert_source;
    const char *frag_source;
} ShaderData;

ShaderData shaders[NUM_SHADERS];

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

    buffer[fileSize] = '\0'; // Null-terminate the buffer
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
static PFNGLLINKPROGRAMARBPROC pglLinkProgramARB;
static PFNGLSHADERSOURCEARBPROC pglShaderSourceARB;
static PFNGLUNIFORM1IARBPROC pglUniform1iARB;
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
    int i;
    GLint location;

    glGetError();

    data->program = pglCreateProgramObjectARB();
    data->vert_shader = pglCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    if(!CompileShaderFromFile(data->vert_shader, water_vert)) {
        return false;
    }
    data->frag_shader = pglCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
    if(!CompileShaderFromFile(data->frag_shader, water_frag)) {
        return false;
    }
    if (!LinkProgram(data)) {
        return false;
    }
   
    // set up uniforms
    pglUseProgramObjectARB(data->program);
    for (i = 0; i < num_tmus_bound; ++i) {
        char tex_name[5];
        (void)SDL_snprintf(tex_name, SDL_arraysize(tex_name), "tex%d", i);
        location = pglGetUniformLocationARB(data->program, tex_name);
        if (location >= 0) {
            pglUniform1iARB(location, i);
        }
    }
    pglUseProgramObjectARB(0);

    return (glGetError() == GL_NO_ERROR);
}

static void DestroyShaderProgram(ShaderData *data)
{
    if (shaders_supported) {
        pglDeleteObjectARB(data->vert_shader);
        pglDeleteObjectARB(data->frag_shader);
        pglDeleteObjectARB(data->program);
    }
}

static bool InitShaders(void)
{
    int i;

    /* Check for shader support */
    shaders_supported = false;
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
        pglUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC)SDL_GL_GetProcAddress("glUseProgramObjectARB");
        if (pglAttachObjectARB &&
            pglCompileShaderARB &&
            pglCreateProgramObjectARB &&
            pglCreateShaderObjectARB &&
            pglDeleteObjectARB &&
            pglGetInfoLogARB &&
            pglGetObjectParameterivARB &&
            pglGetUniformLocationARB &&
            pglLinkProgramARB &&
            pglShaderSourceARB &&
            pglUniform1iARB &&
            pglUseProgramObjectARB) {
            shaders_supported = true;
        }
    }

    if (!shaders_supported) {
        return false;
    }

    /* Compile all the shaders */
    for (i = 0; i < NUM_SHADERS; ++i) {
        if (!CompileShaderProgram(&shaders[i])) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to compile shader!");
            return false;
        }
    }

    /* We're done! */
    return true;
}

static void QuitShaders(void)
{
    int i;

    for (i = 0; i < NUM_SHADERS; ++i) {
        DestroyShaderProgram(&shaders[i]);
    }
}

/* Quick utility function for texture creation */
static int
power_of_two(int input)
{
    int value = 1;

    while (value < input) {
        value <<= 1;
    }
    return value;
}

static GLuint
SDL_GL_LoadTexture(SDL_Surface *surface, GLfloat *texcoord)
{
    GLuint texture;
    int w, h;
    SDL_Surface *image;
    SDL_Rect area;
    SDL_BlendMode saved_mode;

    /* Use the surface width and height expanded to powers of 2 */
    w = power_of_two(surface->w);
    h = power_of_two(surface->h);
    texcoord[0] = 0.0f;                    /* Min X */
    texcoord[1] = 0.0f;                    /* Min Y */
    texcoord[2] = (GLfloat)surface->w / w; /* Max X */
    texcoord[3] = (GLfloat)surface->h / h; /* Max Y */

    image = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!image) {
        printf("cant create surface from image");
        return 0;
    }

    /* Save the alpha blending attributes */
    SDL_GetSurfaceBlendMode(surface, &saved_mode);
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);

    /* Copy the surface into the GL texture image */
    area.x = 0;
    area.y = 0;
    area.w = surface->w;
    area.h = surface->h;
    SDL_BlitSurface(surface, &area, image, &area);

    /* Restore the alpha blending attributes */
    SDL_SetSurfaceBlendMode(surface, saved_mode);

    /* Create an OpenGL texture for the image */
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    SDL_DestroySurface(image); /* No longer needed */

    return texture;
}

/* A general OpenGL initialization function.    Sets all of the initial parameters. */
static void InitGL(int Width, int Height) /* We call this right after our OpenGL window is created. */
{
    GLdouble aspect;

    glViewport(0, 0, Width, Height);
    glClearColor(0.0f, 0.0f, 0.2f, 0.0f); /* This Will Clear The Background Color To Black */
    glClearDepth(1.0);                    /* Enables Clearing Of The Depth Buffer */
    glDepthFunc(GL_LESS);                 /* The Type Of Depth Test To Do */
    glEnable(GL_DEPTH_TEST);              /* Enables Depth Testing */
    glShadeModel(GL_SMOOTH);              /* Enables Smooth Color Shading */

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity(); /* Reset The Projection Matrix */

    aspect = (GLdouble)Width / Height;
    glOrtho(0.0, Width, Height, 0.0, -1.0, 1.0);

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

static void DrawSprite(GLuint texture, float x, float y, float width, float height, float angle, float z) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glEnable(GL_TEXTURE_2D);
    
    pglUseProgramObjectARB(shaders[current_shader].program);

    glPushMatrix();
    
    // rotate around origin
    glTranslatef(x + width / 2.0f, y + height / 2.0f, z);
    glRotatef((GLfloat)to_degrees(angle), 0.0f, 0.0f, 1.0f);
    glTranslatef(-width / 2.0f, -height / 2.0f, z);
    
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);            
    glTexCoord2f(1.0f, 0.0f); glVertex2f(width, 0.0f);            
    glTexCoord2f(1.0f, 1.0f); glVertex2f(width, height);   
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, height);            
    glEnd();
    
    glPopMatrix();

    pglUseProgramObjectARB(0);
    glDisable(GL_TEXTURE_2D);
}

void draw_players() {
    glColor3f(1.0f, 1.0f, 1.0f);

    for(int i = 0; i < MAX_CLIENTS; ++i) {
        if(players[i].id == INVALID_PLAYER_ID) continue;

        float x = players_interpolated[i].x;
        float y = players_interpolated[i].y;
        float angle = players_interpolated[i].rotation;
        DrawSprite(player_texture_map[i], x, y, PLAYER_SPRITE_WIDTH, 
            PLAYER_SPRITE_HEIGHT, angle, PLAYER_Z_COORD_MIN + PLAYER_Z_COORD_MULTIPLIER * players[i].id);
        printf("drawing id%d tex%u\n", players[i].id, player_texture_map[i]);
        //printf("drawn player ID: %d    ---  ", players[i].id);
    }
}

void draw_water() {
    float sprite_width = WINDOW_WIDTH;
    float sprite_height = WINDOW_HEIGHT;
    glColor3f(0.1f, 0.5f, 0.8f);

    DrawSprite(water_texture, 0.0f, 0.0f, sprite_width, 
            sprite_height, 0.0f, 0.0f);
    printf("water tex%u\n",  water_texture);
}

void draw_scene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    draw_water();
    draw_players();

    SDL_GL_SwapWindow(window);

    printf("r");
    fflush(stdout);
}

/* The main drawing function. */
static void DrawGLScene(SDL_Window *window, GLuint texture, GLfloat *texcoord)
{
    /* Texture coordinate lookup, to make it simple */
    enum
    {
        MINX,
        MINY,
        MAXX,
        MAXY
    };
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); /* Clear The Screen And The Depth Buffer */
    glLoadIdentity();                                   /* Reset The View */
    printf("rendering at X: Y: %f %f ", players_interpolated[0].x, players_interpolated[0].y);
    glTranslatef(players_interpolated[0].x, players_interpolated[0].y, 0.0f);

    /* Enable blending */
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* draw a textured square (quadrilateral) */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor3f(1.0f, 1.0f, 1.0f);
    pglUseProgramObjectARB(shaders[current_shader].program);
    glBegin(GL_QUADS); /* start drawing a polygon (4 sided) */
    glTexCoord2f(texcoord[MINX], texcoord[MINY]);
    glVertex3f(-1.0f, 1.0f, 0.0f); /* Top Left */
    glTexCoord2f(texcoord[MAXX], texcoord[MINY]);
    glVertex3f(1.0f, 1.0f, 0.0f); /* Top Right */
    glTexCoord2f(texcoord[MAXX], texcoord[MAXY]);
    glVertex3f(1.0f, -1.0f, 0.0f); /* Bottom Right */
    glTexCoord2f(texcoord[MINX], texcoord[MAXY]);
    glVertex3f(-1.0f, -1.0f, 0.0f); /* Bottom Left */
    glEnd();                        /* done with the polygon */
    pglUseProgramObjectARB(0);
    glDisable(GL_TEXTURE_2D);

    /* swap buffers to display, since we're double buffered. */
    SDL_GL_SwapWindow(window);
}


SDL_Texture* load_texture(SDL_Renderer* renderer, const char* file_path) {
    SDL_Texture* texture = NULL;
    SDL_Surface* surface = SDL_LoadBMP(file_path);
    if (!surface) {
        printf("Failed to load image: %s\n", SDL_GetError());
        return NULL;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        return NULL;
    }

    return texture;
}

void load_player_sprite(int sprite_id) {
    if(sprite_id == INVALID_PLAYER_ID) {
        printf("Tried to load a player sprite for an invalid sprite ID.");
        return;
    };
    printf("LOADING BMP %s\n", player_sprite_files[sprite_id]);
    fflush(stdout);
    SDL_Surface* surface = SDL_LoadBMP(player_sprite_files[sprite_id]);
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to load %s: %s", player_sprite_files[sprite_id], SDL_GetError());
        SDL_Quit();
        exit(3);
    }
    player_texture_map[sprite_id] = SDL_GL_LoadTexture(surface, texcoords[sprite_id]);
    SDL_DestroySurface(surface);
    printf("Loaded map for id: %d --> %u\n", sprite_id, player_texture_map[sprite_id]);
    fflush(stdout);
    return;
}

/*
void render_players() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (players[i].id != INVALID_PLAYER_ID) {
            SDL_FRect dstRect = { players_interpolated[i].x, players_interpolated[i].y, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT };
            SDL_RenderTextureRotated(renderer, player_sprites_map[player_data[i].sprite_id], NULL, 
                &dstRect, to_degrees(players_interpolated[i].rotation), NULL, SDL_FLIP_NONE);
        }
    }

    SDL_RenderPresent(renderer);
}*/

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) == false) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't initialize SDL!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }
    /*
    if (SDL_CreateWindowAndRenderer("Multiplayer Game Client", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer) == false) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Couldn't create window/renderer!", SDL_GetError(), NULL);
        return SDL_APP_FAILURE;
    }*/

    for(int i = 0; i <MAX_CLIENTS; ++i) {
        player_texture_map[i] = INVALID_PLAYER_TEXTURE;
    }

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
    InitShaders();

    SDL_Surface* water_surface = SDL_LoadBMP(water_file);
    water_texture = SDL_GL_LoadTexture(water_surface, texcoords_water);
    printf("LOADED BMP : %s\n", water_file);
    SDL_DestroySurface(water_surface);

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

    for(int i = 0; i < MAX_CLIENTS; ++i) {
        if(players[i].id == INVALID_PLAYER_ID) continue;
        float t = dt / GAME_STATE_UPDATE_FRAME_DELAY;
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
}

void render_game() {
    //render_players();
    draw_scene();
}


void update_game(float dt) {
    const bool *state = SDL_GetKeyboardState(NULL);
    float rot = local_player.rotation;
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
    if (state[SDL_SCANCODE_E]) {
        current_shader =( current_shader + 1 ) % 4;
    }

    rot = fmodf(rot, 2.0f * PI);
    if (rot < 0.0f) {
        rot += 2.0f * PI;
    }

    float direction_x = cosf(rot);
    float direction_y = sinf(rot);
    local_player.rotation = rot;
    local_player.x += direction_x * speed * dt;
    local_player.y += direction_y * speed * dt;
    update_players(dt);
}


SDL_AppResult SDL_AppIterate(void *appstate) {
    Uint32 current_time = SDL_GetTicks();
    float delta_time = (current_time - previous_time) / 1000.0f;
    previous_time = current_time;

    for(int i = 0; i < MAX_CLIENTS; ++i) {
        if(players[i].id == INVALID_PLAYER_ID) continue;
        if(player_texture_map[i] != INVALID_PLAYER_TEXTURE) continue;

        load_player_sprite(player_data[i].sprite_id);
    }

    if(is_update_locked) return SDL_APP_CONTINUE;
    update_game(delta_time);
    render_game();
    
    //printf("processed %d, %d\n", local_player.x, local_player.y);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    QuitShaders();
}