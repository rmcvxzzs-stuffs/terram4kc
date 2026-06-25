#include "player_model.h"
#include "gui.h"
#include <math.h>

int worldToScreen(double worldX, double worldY, double worldZ,
    double camX, double camY, double camZ,
    double camHRot,
    int *screenX, int *screenY) {

    double dx = worldX - camX;
    double dy = worldY - camY;
    double dz = worldZ - camZ;

    double vectorH_x = sin(camHRot);
    double vectorH_y = cos(camHRot);

    double forward = dx * vectorH_y + dz * vectorH_x;

    if (forward < 0.1) return 0;

    double right = dx * vectorH_x - dz * vectorH_y;
    double up = dy;

    double fovScale = 140.0;

    *screenX = BUFFER_HALF_W + (int)(right * fovScale / forward);
    *screenY = BUFFER_HALF_H - (int)(up * fovScale / forward);

    return 1;
}

static void drawVoxel(SDL_Renderer *renderer,
    double x, double y, double z, double size,
    double camX, double camY, double camZ,
    double hRot, double vRot,
    int color, int isOutline) {

    int sx[8], sy[8];
    int visible = 0;

    double c0[3] = {x,       y,       z};
    double c1[3] = {x+size,  y,       z};
    double c2[3] = {x+size,  y+size,  z};
    double c3[3] = {x,       y+size,  z};
    double c4[3] = {x,       y,       z+size};
    double c5[3] = {x+size,  y,       z+size};
    double c6[3] = {x+size,  y+size,  z+size};
    double c7[3] = {x,       y+size,  z+size};

    double *corners[8] = {c0, c1, c2, c3, c4, c5, c6, c7};

    for (int i = 0; i < 8; i++) {
        if (worldToScreen(corners[i][0], corners[i][1], corners[i][2],
            camX, camY, camZ, hRot, &sx[i], &sy[i])) {
            visible++;
        }
    }

    if (visible < 4) return;

    int minX = sx[0], maxX = sx[0];
    int minY = sy[0], maxY = sy[0];
    for (int i = 1; i < 8; i++) {
        if (sx[i] < minX) minX = sx[i];
        if (sx[i] > maxX) maxX = sx[i];
        if (sy[i] < minY) minY = sy[i];
        if (sy[i] > maxY) maxY = sy[i];
    }

    int r = (color >> 16) & 0xFF;
    int g = (color >> 8) & 0xFF;
    int b = color & 0xFF;

    if (isOutline) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect outline = {minX, minY, maxX-minX+1, maxY-minY+1};
        SDL_RenderDrawRect(renderer, &outline);
    } else {
        int shade = 200;
        SDL_SetRenderDrawColor(renderer,
            (r * shade) >> 8,
            (g * shade) >> 8,
            (b * shade) >> 8,
            255);
        SDL_Rect fill = {minX+1, minY+1, maxX-minX-1, maxY-minY-1};
        if (fill.w > 0 && fill.h > 0) {
            SDL_RenderFillRect(renderer, &fill);
        }
    }
}

static void drawBodyPart(SDL_Renderer *renderer,
    double baseX, double baseY, double baseZ,
    double blockSize,
    int w, int h, int d,
    int color,
    double camX, double camY, double camZ,
    double hRot, double vRot) {

    drawVoxel(renderer, baseX-0.05, baseY-0.05, baseZ-0.05,
        w * blockSize + 0.1, camX, camY, camZ, hRot, vRot,
        PM_COLOR_OUTLINE, 1);

    for (int ix = 0; ix < w; ix++) {
        for (int iy = 0; iy < h; iy++) {
            for (int iz = 0; iz < d; iz++) {
                double vx = baseX + ix * blockSize;
                double vy = baseY + iy * blockSize;
                double vz = baseZ + iz * blockSize;

                drawVoxel(renderer, vx, vy, vz, blockSize,
                    camX, camY, camZ, hRot, vRot,
                    color, 0);
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 64);
    for (int ix = 0; ix <= w; ix++) {
        for (int iy = 0; iy <= h; iy++) {
            int sx1, sy1, sx2, sy2;
            if (worldToScreen(baseX + ix*blockSize, baseY + iy*blockSize, baseZ,
                camX, camY, camZ, hRot, &sx1, &sy1) &&
                worldToScreen(baseX + ix*blockSize, baseY + iy*blockSize, baseZ + d*blockSize,
                camX, camY, camZ, hRot, &sx2, &sy2)) {
                SDL_RenderDrawLine(renderer, sx1, sy1, sx2, sy2);
            }
        }
    }
}

void playerModel_render(SDL_Renderer *renderer, World *world,
    double posX, double posY, double posZ,
    double hRot, double vRot,
    int isLocalPlayer) {

    (void)world;
    (void)vRot;
    if (isLocalPlayer) return;

    double blockSize = 0.125;
    double baseX = posX - 0.5;
    double baseY = posY - 1.5;
    double baseZ = posZ - 0.5;
    double camX = posX;
    double camY = posY;
    double camZ = posZ;

    drawBodyPart(renderer,
        baseX + 2*blockSize, baseY + 24*blockSize, baseZ + 2*blockSize,
        blockSize,
        PM_HEAD_W, PM_HEAD_H, PM_HEAD_D,
        PM_COLOR_HEAD,
        camX, camY, camZ, hRot, vRot);

    drawBodyPart(renderer,
        baseX + 2*blockSize, baseY + 12*blockSize, baseZ + 2*blockSize,
        blockSize,
        PM_BODY_W, PM_BODY_H, PM_BODY_D,
        PM_COLOR_BODY,
        camX, camY, camZ, hRot, vRot);

    drawBodyPart(renderer,
        baseX - 2*blockSize, baseY + 12*blockSize, baseZ + 2*blockSize,
        blockSize,
        PM_ARM_W, PM_ARM_H, PM_ARM_D,
        PM_COLOR_ARM,
        camX, camY, camZ, hRot, vRot);

    drawBodyPart(renderer,
        baseX + 10*blockSize, baseY + 12*blockSize, baseZ + 2*blockSize,
        blockSize,
        PM_ARM_W, PM_ARM_H, PM_ARM_D,
        PM_COLOR_ARM,
        camX, camY, camZ, hRot, vRot);

    drawBodyPart(renderer,
        baseX + 2*blockSize, baseY, baseZ + 2*blockSize,
        blockSize,
        PM_LEG_W, PM_LEG_H, PM_LEG_D,
        PM_COLOR_LEG,
        camX, camY, camZ, hRot, vRot);

    drawBodyPart(renderer,
        baseX + 6*blockSize, baseY, baseZ + 2*blockSize,
        blockSize,
        PM_LEG_W, PM_LEG_H, PM_LEG_D,
        PM_COLOR_LEG,
        camX, camY, camZ, hRot, vRot);
}