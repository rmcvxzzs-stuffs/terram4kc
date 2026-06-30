#include "gl_renderer.h"
#include "main.h"
#include "gui.h"        /* BUFFER_W, BUFFER_H, BUFFER_SCALE, WINDOW_W, WINDOW_H */
#include "gameloop.h"   /* gameLoop(), Inputs, world, player */
#include "imgui_renderer.h"
#include "textures.h"   /* textures[], BLOCK_TEXTURE_W/H, TEXTURES_SIZE */
#include "terrain.h"    /* World, Chunk, CHUNKARR_SIZE, CHUNK_SIZE */
#include "blocks.h"     /* NUMBER_OF_BLOCKS, block IDs */
#include "options.h"    /* options.drawDistance, options.fov, etc */

#include "glad/glad.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -------------------------------------------------------------------------
 * World volume dimensions
 * We unpack the 3x3x3 chunk grid into a flat 192^3 GL_TEXTURE_3D each frame,
 * centred on the player's chunk The shader samples it with a world-space
 * offset to look up block IDs
 * ---------------------------------------------------------------------- */
#define WORLD_TEX_SIZE (CHUNKARR_DIAM * CHUNK_SIZE)   /* 3*64 = 192 */

/* Atlas layout:
 *   width  = NUMBER_OF_BLOCKS * BLOCK_TEXTURE_W  (16*16 = 256)
 *   height = BLOCK_TEXTURE_H * 3                 (16*3  = 48)
 *   Each block occupies a 16-wide column; rows 0-15=side, 16-31=top, 32-47=bottom
 */
#define ATLAS_W  (NUMBER_OF_BLOCKS * BLOCK_TEXTURE_W)
#define ATLAS_H  (BLOCK_TEXTURE_H * 3)

/* -------------------------------------------------------------------------
 * Internal state
 * ---------------------------------------------------------------------- */
static SDL_GLContext  s_glctx  = NULL;
static SDL_Window    *s_window = NULL;

/* Fullscreen quad */
static GLuint s_vao  = 0;
static GLuint s_vbo  = 0;

/* Raycaster shader program */
static GLuint s_prog = 0;

/* World volume: R8UI 192^3 */
static GLuint s_worldTex = 0;
static uint8_t s_worldBuf[WORLD_TEX_SIZE * WORLD_TEX_SIZE * WORLD_TEX_SIZE];

/* Texture atlas: RGBA8 256x48 */
static GLuint s_atlasTex = 0;
static uint8_t s_atlasBuf[ATLAS_W * ATLAS_H * 4];

/* Cached chunk origin (player's chunk coords * 64, minus one chunk radius) */
static int s_originX = 0, s_originY = 0, s_originZ = 0;

/* Pixel buffer for non-gameplay states (menus, loading, etc.) */
static GLuint  s_frameTex = 0;
static uint8_t *s_pixels  = NULL; /* BUFFER_W * BUFFER_H * 4, allocated in init */

/* Simple blit shader for menu/non-gameplay states */
static GLuint s_blitProg = 0;

/* -------------------------------------------------------------------------
 * Blit shader - used for menus/non-gameplay states
 * Just samples the CPU-rendered pixel buffer and outputs it
 * ---------------------------------------------------------------------- */
static const char *BLIT_FRAG_SRC =
    "#version 330 core\n"
    "in vec2 vUV;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uFrame;\n"
    "void main() {\n"
    "    fragColor = texture(uFrame, vUV);\n"
    "}\n";

/* -------------------------------------------------------------------------
 * Vertex shader - fullscreen quad, pass through UV
 * ---------------------------------------------------------------------- */
static const char *VERT_SRC =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec2 aUV;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "    vUV = aUV;\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "}\n";

/* -------------------------------------------------------------------------
 * Fragment shader - GLSL port of the gameloop.c DDA raycaster
 *
 * Uniforms:
 *   uWorld      - R8UI sampler3D, 192^3 block IDs
 *   uAtlas      - RGBA8 sampler2D, texture atlas
 *   uOrigin     - ivec3, world-space position of texel (0,0,0) in uWorld
 *   uPos        - vec3,  player world position
 *   uVecH       - vec2,  (sin hRot, cos hRot)
 *   uVecV       - vec2,  (sin vRot, cos vRot)
 *   uFov        - float, effective FOV divisor
 *   uDrawDist   - float, effective draw distance
 *   uTimeCoef   - float, day/night brightness [0..1]
 *   uHeadWater  - int,   1 if head is in water
 *   uFogType    - int,   options.fogType
 *   uBlockSel   - ivec3, currently selected block (for outline)
 *   uHasBlockSel- int,   1 if a block is selected
 *   uGuiOn      - int,   1 if GUI is on
 *   uGamePopup  - int,   current popup ID (0 = HUD)
 *   uTrapMouse  - int,   options.trapMouse
 *   uMouseX/Y   - int,   mouse pixel coords (unscaled)
 *   uResolution - vec2,  BUFFER_W, BUFFER_H
 * ---------------------------------------------------------------------- */
static const char *FRAG_SRC =
    "#version 330 core\n"
    "in vec2 vUV;\n"
    "out vec4 fragColor;\n"
    "\n"
    "uniform usampler3D uWorld;\n"
    "uniform sampler2D  uAtlas;\n"
    "uniform ivec3  uOrigin;\n"
    "uniform vec3   uPos;\n"
    "uniform vec2   uVecH;\n"
    "uniform vec2   uVecV;\n"
    "uniform float  uFov;\n"
    "uniform float  uDrawDist;\n"
    "uniform float  uTimeCoef;\n"
    "uniform int    uHeadWater;\n"
    "uniform int    uFogType;\n"
    "uniform ivec3  uBlockSel;\n"
    "uniform int    uHasBlockSel;\n"
    "uniform int    uGuiOn;\n"
    "uniform int    uGamePopup;\n"
    "uniform int    uTrapMouse;\n"
    "uniform int    uMouseX;\n"
    "uniform int    uMouseY;\n"
    "uniform vec2   uResolution;\n"
    "\n"
    "#define NUM_BLOCKS   16\n"
    "#define TEX_W        16\n"
    "#define TEX_H        16\n"
    "#define ATLAS_COLS   16\n"
    "#define WORLD_SZ     192\n"
    "\n"
    "int getBlock(ivec3 wp) {\n"
    "    ivec3 local = wp - uOrigin;\n"
    "    if (any(lessThan(local, ivec3(0))) ||\n"
    "        any(greaterThanEqual(local, ivec3(WORLD_SZ)))) return 0;\n"
    "    return int(texelFetch(uWorld, local, 0).r);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    int pixelX = int(vUV.x * uResolution.x);\n"
    "    int pixelY = int(vUV.y * uResolution.y);\n"
    "    float halfW = uResolution.x * 0.5;\n"
    "    float halfH = uResolution.y * 0.5;\n"
    "\n"
    "    float rayOffsetX = (float(pixelX) - halfW) / uFov;\n"
    "    float rayOffsetY = (float(pixelY) - halfH) / uFov;\n"
    "\n"
    "    float f21 = 1.0;\n"
    "    float f22 = f21 * uVecV.y + rayOffsetY * uVecV.x;\n"
    "    float f23 = rayOffsetY * uVecV.y - f21 * uVecV.x;\n"
    "    float f24 = rayOffsetX * uVecH.y + f22 * uVecH.x;\n"
    "    float f25 = f22 * uVecH.y - rayOffsetX * uVecH.x;\n"
    "\n"
    "    vec3 skyCol = uHeadWater == 1\n"
    "        ? vec3(48.0, 96.0, 200.0) * uTimeCoef / 255.0\n"
    "        : vec3(153.0, 204.0, 255.0) * uTimeCoef / 255.0;\n"
    "\n"
    "    bool  hitFound = false;\n"
    "    float bestDist = uDrawDist;\n"
    "    vec3  bestCol  = vec3(0.0);\n"
    "    int   bestFace = 0;\n"
    "\n"
    "    for (int blockFace = 0; blockFace < 3; blockFace++) {\n"
    "        float f27 = (blockFace == 0) ? f24 : (blockFace == 1) ? f23 : f25;\n"
    "        float f28 = 1.0 / max(abs(f27), 1e-10);\n"
    "        float f29 = f24 * f28;\n"
    "        float f30 = f23 * f28;\n"
    "        float f31 = f25 * f28;\n"
    "\n"
    "        float posAxis;\n"
    "        if (blockFace == 0) posAxis = uPos.x;\n"
    "        else if (blockFace == 1) posAxis = uPos.y;\n"
    "        else posAxis = uPos.z;\n"
    "        float f32 = posAxis - floor(posAxis);\n"
    "        if (f27 > 0.0) f32 = 1.0 - f32;\n"
    "        float f33 = f28 * f32;\n"
    "        float f34 = uPos.x + f29 * f32;\n"
    "        float f35 = uPos.y + f30 * f32;\n"
    "        float f36 = uPos.z + f31 * f32;\n"
    "        if (f27 < 0.0) {\n"
    "            if (blockFace == 0) f34 -= 1.0;\n"
    "            else if (blockFace == 1) f35 -= 1.0;\n"
    "            else f36 -= 1.0;\n"
    "        }\n"
    "\n"
    "        for (int step = 0; step < 128; step++) {\n"
    "            if (f33 >= bestDist) break;\n"
    "\n"
    "            ivec3 bp = ivec3(floor(f34), floor(f35), floor(f36));\n"
    "            int block = getBlock(bp);\n"
    "\n"
    "            if (block != 0 && !(uHeadWater == 1 && block == 10)) {\n"
    "                int texX, texY;\n"
    "                if (blockFace == 1) {\n"
    "                    texX = int(floor(f34 * 16.0)) & 0xF;\n"
    "                    texY = int(floor(f36 * 16.0)) & 0xF;\n"
    "                    if (f30 < 0.0) texY += 32;\n"
    "                } else {\n"
    "                    texX = int(floor((f34 + f36) * 16.0)) & 0xF;\n"
    "                    texY = (int(floor(f35 * 16.0)) & 0xF) + 16;\n"
    "                }\n"
    "\n"
    "                bool isSelected = (uHasBlockSel == 1 &&\n"
    "                    bp == uBlockSel &&\n"
    "                    (texX == 0 || texY % 16 == 0 ||\n"
    "                     texX == 15 || texY % 16 == 15) &&\n"
    "                    uGuiOn == 1 && uGamePopup == 0);\n"
    "\n"
    "                vec3 col;\n"
    "                if (isSelected) {\n"
    "                    col = vec3(1.0);\n"
    "                } else if (block >= NUM_BLOCKS) {\n"
    "                    col = vec3(1.0, 0.0, 0.0);\n"
    "                } else {\n"
    "                    float au = (float(block * TEX_W + texX) + 0.5)\n"
    "                               / float(ATLAS_COLS * TEX_W);\n"
    "                    float av = (float(texY) + 0.5) / float(TEX_H * 3);\n"
    "                    col = texture(uAtlas, vec2(au, av)).rgb;\n"
    "                }\n"
    "\n"
    "                if (dot(col, col) > 0.0) {\n"
    "                    hitFound = true;\n"
    "                    bestDist = f33;\n"
    "                    bestCol  = col;\n"
    "                    bestFace = blockFace;\n"
    "                    break;\n"
    "                }\n"
    "            }\n"
    "\n"
    "            f34 += f29;\n"
    "            f35 += f30;\n"
    "            f36 += f31;\n"
    "            f33 += f28;\n"
    "        }\n"
    "    }\n"
    "\n"
    "    if (hitFound) {\n"
    "        float pixelMist  = 255.0 - bestDist / uDrawDist * 255.0;\n"
    "        int   pixelShade = 255 - (bestFace + 2) % 3 * 50;\n"
    "        float shade = float(pixelShade) / 255.0;\n"
    "        float mist;\n"
    "        if (uFogType == 1)\n"
    "            mist = sqrt(pixelMist / 255.0);\n"
    "        else\n"
    "            mist = pixelMist / 255.0;\n"
    "\n"
    "        vec3 shaded = bestCol * shade;\n"
    "        vec3 fogged = mix(skyCol, shaded, mist);\n"
    "\n"
    "        if (uTrapMouse == 1) {\n"
    "            bool onH = (pixelX == int(halfW) && abs(int(halfH) - pixelY) < 4);\n"
    "            bool onV = (pixelY == int(halfH) && abs(int(halfW) - pixelX) < 4);\n"
    "            if (onH || onV) fogged = 1.0 - fogged;\n"
    "        }\n"
    "\n"
    "        if (uHeadWater == 1)\n"
    "            fogged = mix(fogged, vec3(16.0/255.0, 32.0/255.0, 1.0), 0.5);\n"
    "\n"
    "        fragColor = vec4(fogged, 1.0);\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    vec3 sky = skyCol;\n"
    "    if (uTrapMouse == 1) {\n"
    "        bool onH = (pixelX == int(halfW) && abs(int(halfH) - pixelY) < 4);\n"
    "        bool onV = (pixelY == int(halfH) && abs(int(halfW) - pixelX) < 4);\n"
    "        if (onH || onV) sky = 1.0 - sky;\n"
    "    }\n"
    "    fragColor = vec4(sky, 1.0);\n"
    "}\n";

/* Fullscreen quad */
static const float QUAD_VERTS[] = {
    -1.0f,  1.0f,  0.0f, 0.0f,
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f,
};

/* -------------------------------------------------------------------------
 * Shader helpers
 * ---------------------------------------------------------------------- */


static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "[gl_renderer] shader error:\n%s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vert, GLuint frag) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vert);
    glAttachShader(p, frag);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "[gl_renderer] link error:\n%s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

/* -------------------------------------------------------------------------
 * Atlas upload - convert textures[] int array to RGBA bytes
 * Layout: one 16-wide column per block, 48 rows tall (side/top/bottom)
 * ---------------------------------------------------------------------- */
static void upload_atlas(void) {
    memset(s_atlasBuf, 0, sizeof(s_atlasBuf));
    for (int blockId = 0; blockId < NUMBER_OF_BLOCKS; blockId++) {
        for (int y = 0; y < ATLAS_H; y++) {
            for (int x = 0; x < BLOCK_TEXTURE_W; x++) {
                int src_idx = x + y * BLOCK_TEXTURE_W +
                              blockId * BLOCK_TEXTURE_W * BLOCK_TEXTURE_H * 3;
                int packed = textures[src_idx];
                int dst_idx = (blockId * BLOCK_TEXTURE_W + x +
                               y * ATLAS_W) * 4;
                s_atlasBuf[dst_idx + 0] = (packed >> 16) & 0xFF; /* R */
                s_atlasBuf[dst_idx + 1] = (packed >>  8) & 0xFF; /* G */
                s_atlasBuf[dst_idx + 2] = (packed       ) & 0xFF; /* B */
                s_atlasBuf[dst_idx + 3] = (packed == 0 && blockId != 0) ? 0 : 255;
            }
        }
    }

    glBindTexture(GL_TEXTURE_2D, s_atlasTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ATLAS_W, ATLAS_H,
                    GL_RGBA, GL_UNSIGNED_BYTE, s_atlasBuf);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* -------------------------------------------------------------------------
 * World volume upload
 * Unpacks the 27 chunks into a flat 192^3 array. The array origin is set
 * to (player_chunk - 1) * 64 so the player is always in the centre chunk.
 * ---------------------------------------------------------------------- */
extern World world; /* defined in gameloop.c */

static void upload_world(void) {
    /* Centre the volume on the player's chunk */
    int pcx = (int)floor(world.player.pos.x / CHUNK_SIZE) - CHUNKARR_RAD;
    int pcy = (int)floor(world.player.pos.y / CHUNK_SIZE) - CHUNKARR_RAD;
    int pcz = (int)floor(world.player.pos.z / CHUNK_SIZE) - CHUNKARR_RAD;

    s_originX = pcx * CHUNK_SIZE;
    s_originY = pcy * CHUNK_SIZE;
    s_originZ = pcz * CHUNK_SIZE;

    memset(s_worldBuf, 0, sizeof(s_worldBuf));

    for (int ci = 0; ci < CHUNKARR_SIZE; ci++) {
        Chunk *ch = &world.chunk[ci];
        if (!ch->loaded || !ch->blocks) continue;

        /* Chunk world-space origin */
        int wx = ch->center.x - CHUNK_SIZE / 2;
        int wy = ch->center.y - CHUNK_SIZE / 2;
        int wz = ch->center.z - CHUNK_SIZE / 2;

        /* Offset into our volume */
        int ox = wx - s_originX;
        int oy = wy - s_originY;
        int oz = wz - s_originZ;

        /* Skip if entirely outside volume */
        if (ox < 0 || oy < 0 || oz < 0 ||
            ox + CHUNK_SIZE > WORLD_TEX_SIZE ||
            oy + CHUNK_SIZE > WORLD_TEX_SIZE ||
            oz + CHUNK_SIZE > WORLD_TEX_SIZE) continue;

        for (int bz = 0; bz < CHUNK_SIZE; bz++)
        for (int by = 0; by < CHUNK_SIZE; by++)
        for (int bx = 0; bx < CHUNK_SIZE; bx++) {
            Block b = ch->blocks[bx + (by << 6) + (bz << 12)];
            int idx = (ox + bx) +
                      (oy + by) * WORLD_TEX_SIZE +
                      (oz + bz) * WORLD_TEX_SIZE * WORLD_TEX_SIZE;
            s_worldBuf[idx] = b;
        }
    }

    glBindTexture(GL_TEXTURE_3D, s_worldTex);
    glTexSubImage3D(GL_TEXTURE_3D, 0,
                    0, 0, 0,
                    WORLD_TEX_SIZE, WORLD_TEX_SIZE, WORLD_TEX_SIZE,
                    GL_RED_INTEGER, GL_UNSIGNED_BYTE, s_worldBuf);
    glBindTexture(GL_TEXTURE_3D, 0);
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */
int gl_renderer_init(SDL_Window *window) {
    s_window = window;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    s_glctx = SDL_GL_CreateContext(window);
    if (!s_glctx) {
        fprintf(stderr, "[gl_renderer] SDL_GL_CreateContext: %s\n", SDL_GetError());
        return 1;
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        fprintf(stderr, "[gl_renderer] GLAD failed\n");
        return 1;
    }
    printf("[gl_renderer] OpenGL %s  GLSL %s\n",
           glGetString(GL_VERSION),
           glGetString(GL_SHADING_LANGUAGE_VERSION));

    SDL_GL_SetSwapInterval(1);

    /* Pixel buffer for menu blitting */
    s_pixels = calloc(BUFFER_W * BUFFER_H * 4, 1);
    if (!s_pixels) {
        fprintf(stderr, "[gl_renderer] out of memory for pixel buffer\n");
        return 1;
    }

    /* Shaders - try loading from src/shaders/ first, fall back to embedded */
    GLuint vert = compile_shader(GL_VERTEX_SHADER, VERT_SRC);
    if (!vert) return 1;

    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, FRAG_SRC);
    if (!frag) { glDeleteShader(vert); return 1; }
    s_prog = link_program(vert, frag);
    glDeleteShader(frag);
    if (!s_prog) { glDeleteShader(vert); return 1; }

    /* Quad VAO/VBO */
    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);
    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTS), QUAD_VERTS, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    /* Blit program (menus) - reuses same vert shader */
    GLuint bfrag = compile_shader(GL_FRAGMENT_SHADER, BLIT_FRAG_SRC);
    if (!bfrag) { glDeleteShader(vert); return 1; }
    s_blitProg = link_program(vert, bfrag);
    glDeleteShader(bfrag);
    glDeleteShader(vert); /* done with vert now */
    if (!s_blitProg) return 1;

    /* Frame texture for menu blitting */
    glGenTextures(1, &s_frameTex);
    glBindTexture(GL_TEXTURE_2D, s_frameTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 BUFFER_W, BUFFER_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* World texture - R8UI 192^3 */
    glGenTextures(1, &s_worldTex);
    glBindTexture(GL_TEXTURE_3D, s_worldTex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI,
                 WORLD_TEX_SIZE, WORLD_TEX_SIZE, WORLD_TEX_SIZE,
                 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_3D, 0);

    /* Atlas texture - RGBA8 */
    glGenTextures(1, &s_atlasTex);
    glBindTexture(GL_TEXTURE_2D, s_atlasTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 ATLAS_W, ATLAS_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glViewport(0, 0, WINDOW_W, WINDOW_H);
    printf("[gl_renderer] init OK  world=%d^3  atlas=%dx%d\n",
           WORLD_TEX_SIZE, ATLAS_W, ATLAS_H);
    return 0;
}

void gl_renderer_quit(void) {
    free(s_pixels);       s_pixels    = NULL;
    if (s_worldTex)  { glDeleteTextures(1, &s_worldTex);  s_worldTex  = 0; }
    if (s_atlasTex)  { glDeleteTextures(1, &s_atlasTex);  s_atlasTex  = 0; }
    if (s_frameTex)  { glDeleteTextures(1, &s_frameTex);  s_frameTex  = 0; }
    if (s_vbo)       { glDeleteBuffers(1, &s_vbo);        s_vbo       = 0; }
    if (s_vao)       { glDeleteVertexArrays(1, &s_vao);   s_vao       = 0; }
    if (s_prog)      { glDeleteProgram(s_prog);            s_prog      = 0; }
    if (s_blitProg)  { glDeleteProgram(s_blitProg);        s_blitProg  = 0; }
    if (s_glctx)     { SDL_GL_DeleteContext(s_glctx);      s_glctx     = NULL; }
}

SDL_GLContext gl_renderer_get_context(void) { return s_glctx; }

/* -------------------------------------------------------------------------
 * Frame
 *
 * The GL path runs the full gameLoop() for game logic (movement, menus, block
 * selection, etc.) using a tiny 1x1 software renderer - no pixels are drawn
 * to it, we only need the side effects (player pos, block selection, world
 * mutation). The actual visuals come from the GLSL raycaster.
 *
 * Block selection (which block the cursor is pointing at) still happens on
 * the CPU inside gameLoop(); we just pass the result to the shader as a
 * uniform so it can draw the outline.
 * ---------------------------------------------------------------------- */

/* These are exposed by gameloop.c so we can read them */
extern int gameState;
extern int gamePopup;

/* Block selection state - read from gameloop internals via these externs.
 * We expose them from gameloop.c with a small helper. */
int  gl_getBlockSelected(void);
void gl_getBlockSelectCoords(int *x, int *y, int *z);
int  gl_getGuiOn(void);
int  gl_getDebugOn(void);

int gl_renderer_frame(void *inputs_v) {
    Inputs *inputs = (Inputs *)inputs_v;

    /* --- Run game logic on a 1x1 software surface (no pixel output needed) --- */
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(
        0, BUFFER_W, BUFFER_H, 32, SDL_PIXELFORMAT_ABGR8888);
    SDL_Renderer *soft = surf ? SDL_CreateSoftwareRenderer(surf) : NULL;
    if (!surf || !soft) {
        if (soft) SDL_DestroyRenderer(soft);
        if (surf) SDL_FreeSurface(surf);
        return 0;
    }

    int running = gameLoop(inputs, soft);

    int isGameplay = (gameState == 5); /* STATE_GAMEPLAY */
    int hasPopup   = isGameplay && (gamePopup != 0);

    /* Skip raycaster until player position is valid (non-zero after world load) */
    if (isGameplay && world.player.pos.y == 0.0) {
        SDL_DestroyRenderer(soft);
        SDL_FreeSurface(surf);
        SDL_GL_SwapWindow(s_window);
        return running;
    }

    /* Always upload world when in gameplay state regardless of popup */
    if (isGameplay) {
        static int s_atlasUploaded = 0;
        if (!s_atlasUploaded) {
            upload_atlas();
            s_atlasUploaded = 1;
        }
        upload_world();
    }

    /* Capture software pixels - always, for HUD, menus, logo, popups */
    SDL_RenderFlush(soft);
    SDL_RenderReadPixels(soft, NULL, SDL_PIXELFORMAT_ABGR8888,
                         s_pixels, BUFFER_W * 4);
    glBindTexture(GL_TEXTURE_2D, s_frameTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, BUFFER_W, BUFFER_H,
                    GL_RGBA, GL_UNSIGNED_BYTE, s_pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    SDL_DestroyRenderer(soft);
    SDL_FreeSurface(surf);

    glClear(GL_COLOR_BUFFER_BIT);

    if (!isGameplay) {
        /* Pure blit - title/loading/world select screens */
        glUseProgram(s_blitProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_frameTex);
        glUniform1i(glGetUniformLocation(s_blitProg, "uFrame"), 0);
        glBindVertexArray(s_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        if (g_debug_mode) imgui_render();
        SDL_GL_SwapWindow(s_window);
        return running;
    }

    /* --- Set uniforms --- */
    glUseProgram(s_prog);

    /* World texture */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, s_worldTex);
    glUniform1i(glGetUniformLocation(s_prog, "uWorld"), 0);

    /* Atlas texture */
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_atlasTex);
    glUniform1i(glGetUniformLocation(s_prog, "uAtlas"), 1);

    /* World origin */
    glUniform3i(glGetUniformLocation(s_prog, "uOrigin"),
                s_originX, s_originY, s_originZ);

    /* Player */
    glUniform3f(glGetUniformLocation(s_prog, "uPos"),
                (float)world.player.pos.x,
                (float)world.player.pos.y,
                (float)world.player.pos.z);
    glUniform2f(glGetUniformLocation(s_prog, "uVecH"),
                (float)world.player.vectorH.x,
                (float)world.player.vectorH.y);
    glUniform2f(glGetUniformLocation(s_prog, "uVecV"),
                (float)world.player.vectorV.x,
                (float)world.player.vectorV.y);

    /* FOV & draw distance */
    float effectFov = (float)options.fov;
    int   headWater = (World_getBlock(&world,
                          (int)world.player.pos.x,
                          (int)world.player.pos.y,
                          (int)world.player.pos.z) == BLOCK_WATER);
    if (headWater) effectFov += 20.0f;
    float effectDist = (float)options.drawDistance;
    if (headWater) effectDist = 10.0f;

    glUniform1f(glGetUniformLocation(s_prog, "uFov"),      effectFov);
    glUniform1f(glGetUniformLocation(s_prog, "uDrawDist"), effectDist);

    /* Day/night */
    double tc;
    switch (world.dayNightMode) {
    case 0: {
        double t = (double)(world.time % 102944) / 16384.0;
        t = sin(t);
        t /= sqrt(t * t + (1.0/128.0));
        tc = (t + 1.0) / 2.0;
        break;
    }
    case 1: tc = 1.0; break;
    default: tc = 0.0; break;
    }
    glUniform1f(glGetUniformLocation(s_prog, "uTimeCoef"), (float)tc);
    glUniform1i(glGetUniformLocation(s_prog, "uHeadWater"), headWater);
    glUniform1i(glGetUniformLocation(s_prog, "uFogType"),   options.fogType);

    /* Block selection */
    int bsel = gl_getBlockSelected();
    int bx, by, bz;
    gl_getBlockSelectCoords(&bx, &by, &bz);
    glUniform1i(glGetUniformLocation(s_prog, "uHasBlockSel"), bsel);
    glUniform3i(glGetUniformLocation(s_prog, "uBlockSel"), bx, by, bz);
    glUniform1i(glGetUniformLocation(s_prog, "uGuiOn"),    gl_getGuiOn());
    glUniform1i(glGetUniformLocation(s_prog, "uGamePopup"), gamePopup);

    /* Mouse */
    glUniform1i(glGetUniformLocation(s_prog, "uTrapMouse"), options.trapMouse);
    glUniform1i(glGetUniformLocation(s_prog, "uMouseX"),    inputs->mouse.x);
    glUniform1i(glGetUniformLocation(s_prog, "uMouseY"),    inputs->mouse.y);

    /* Resolution */
    glUniform2f(glGetUniformLocation(s_prog, "uResolution"),
                (float)BUFFER_W, (float)BUFFER_H);

    /* --- Draw --- */
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    /* --- Composite software surface on top (HUD, popups, etc.) --- */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(s_blitProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_frameTex);
    glUniform1i(glGetUniformLocation(s_blitProg, "uFrame"), 0);
    glBindVertexArray(s_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDisable(GL_BLEND);

    /* --- ImGui --- */
    if (g_debug_mode) {
        imgui_render();
    }

    SDL_GL_SwapWindow(s_window);
    return running;
}