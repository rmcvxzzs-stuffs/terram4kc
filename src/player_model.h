#ifndef PLAYER_MODEL_H
#define PLAYER_MODEL_H

#include "main.h"
#include "terrain.h"

// Player model dimensions (in blocks)
#define PM_HEAD_W 8
#define PM_HEAD_H 8
#define PM_HEAD_D 8

#define PM_BODY_W 8
#define PM_BODY_H 12
#define PM_BODY_D 4

#define PM_ARM_W 4
#define PM_ARM_H 12
#define PM_ARM_D 4

#define PM_LEG_W 4
#define PM_LEG_H 12
#define PM_LEG_D 4

// Colors for each part (RGB)
#define PM_COLOR_HEAD   0xFFCC99
#define PM_COLOR_BODY   0x3366CC
#define PM_COLOR_ARM    0xFFCC99
#define PM_COLOR_LEG    0x333399
#define PM_COLOR_OUTLINE 0x000000

void playerModel_render(SDL_Renderer *renderer, World *world,
    double posX, double posY, double posZ,
    double hRot, double vRot,
    int isLocalPlayer);

int worldToScreen(double worldX, double worldY, double worldZ,
    double camX, double camY, double camZ,
    double hRot,
    int *screenX, int *screenY);

#endif