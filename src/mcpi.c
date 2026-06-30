#include "mcpi.h"
#include "terrain.h"
#include "blocks.h"
#include "main.h"

#include <SDL2/SDL.h>
#include <SDL_net.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MCPI_PORT     4711
#define MCPI_BUF_SIZE 1024
#define MCPI_MAX_CONN 4

/* -------------------------------------------------------------------------
 * Shared state
 * ---------------------------------------------------------------------- */
extern World world; /* defined in gameloop.c */

static SDL_mutex  *s_worldMutex = NULL;
static SDL_Thread *s_thread     = NULL;
static volatile int s_running   = 0;

/* -------------------------------------------------------------------------
 * Protocol helpers
 * ---------------------------------------------------------------------- */

/* Read a line (up to \n) from a socket into buf. Returns bytes read or -1. */
static int recv_line(TCPsocket sock, char *buf, int maxlen) {
    int i = 0;
    char c;
    while (i < maxlen - 1) {
        int n = SDLNet_TCP_Recv(sock, &c, 1);
        if (n <= 0) return -1;
        if (c == '\n') break;
        if (c != '\r') buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

static void send_str(TCPsocket sock, const char *s) {
    SDLNet_TCP_Send(sock, (void *)s, (int)strlen(s));
}

/* -------------------------------------------------------------------------
 * Command handlers
 * ---------------------------------------------------------------------- */

static void handle_command(TCPsocket sock, const char *line) {
    char cmd[64] = {0};
    char args[MCPI_BUF_SIZE] = {0};

    /* Split "cmd(args)" */
    const char *paren = strchr(line, '(');
    if (!paren) return;
    size_t cmdlen = paren - line;
    if (cmdlen >= sizeof(cmd)) return;
    strncpy(cmd, line, cmdlen);
    cmd[cmdlen] = '\0';

    const char *argstart = paren + 1;
    const char *argend   = strrchr(argstart, ')');
    if (!argend) return;
    size_t arglen = argend - argstart;
    if (arglen >= sizeof(args)) return;
    strncpy(args, argstart, arglen);
    args[arglen] = '\0';

    /* --- world.getBlock(x,y,z) --- */
    if (strcmp(cmd, "world.getBlock") == 0) {
        int x, y, z;
        if (sscanf(args, "%d,%d,%d", &x, &y, &z) != 3) return;
        SDL_LockMutex(s_worldMutex);
        Block b = World_getBlock(&world, x, y, z);
        SDL_UnlockMutex(s_worldMutex);
        char resp[32];
        snprintf(resp, sizeof(resp), "%d\n", (int)b);
        send_str(sock, resp);
    }

    /* --- world.setBlock(x,y,z,id) --- */
    else if (strcmp(cmd, "world.setBlock") == 0) {
        int x, y, z, id;
        if (sscanf(args, "%d,%d,%d,%d", &x, &y, &z, &id) != 4) return;
        SDL_LockMutex(s_worldMutex);
        World_setBlock(&world, x, y, z, (Block)id, 1);
        SDL_UnlockMutex(s_worldMutex);
    }

    /* --- world.setBlocks(x1,y1,z1,x2,y2,z2,id) --- */
    else if (strcmp(cmd, "world.setBlocks") == 0) {
        int x1, y1, z1, x2, y2, z2, id;
        if (sscanf(args, "%d,%d,%d,%d,%d,%d,%d",
                   &x1, &y1, &z1, &x2, &y2, &z2, &id) != 7) return;
        int minx = x1 < x2 ? x1 : x2, maxx = x1 > x2 ? x1 : x2;
        int miny = y1 < y2 ? y1 : y2, maxy = y1 > y2 ? y1 : y2;
        int minz = z1 < z2 ? z1 : z2, maxz = z1 > z2 ? z1 : z2;
        SDL_LockMutex(s_worldMutex);
        for (int x = minx; x <= maxx; x++)
        for (int y = miny; y <= maxy; y++)
        for (int z = minz; z <= maxz; z++)
            World_setBlock(&world, x, y, z, (Block)id, 1);
        SDL_UnlockMutex(s_worldMutex);
    }

    /* --- player.getPos() --- */
    else if (strcmp(cmd, "player.getPos") == 0) {
        SDL_LockMutex(s_worldMutex);
        double px = world.player.pos.x;
        double py = world.player.pos.y;
        double pz = world.player.pos.z;
        SDL_UnlockMutex(s_worldMutex);
        char resp[64];
        snprintf(resp, sizeof(resp), "%.2f,%.2f,%.2f\n", px, py, pz);
        send_str(sock, resp);
    }

    /* --- player.setPos(x,y,z) --- */
    else if (strcmp(cmd, "player.setPos") == 0) {
        double x, y, z;
        if (sscanf(args, "%lf,%lf,%lf", &x, &y, &z) != 3) return;
        SDL_LockMutex(s_worldMutex);
        world.player.pos.x = x;
        world.player.pos.y = y;
        world.player.pos.z = z;
        SDL_UnlockMutex(s_worldMutex);
    }

    /* --- player.getTile() --- */
    else if (strcmp(cmd, "player.getTile") == 0) {
        SDL_LockMutex(s_worldMutex);
        int px = (int)world.player.pos.x;
        int py = (int)world.player.pos.y;
        int pz = (int)world.player.pos.z;
        SDL_UnlockMutex(s_worldMutex);
        char resp[64];
        snprintf(resp, sizeof(resp), "%d,%d,%d\n", px, py, pz);
        send_str(sock, resp);
    }

    /* --- player.setTile(x,y,z) --- */
    else if (strcmp(cmd, "player.setTile") == 0) {
        int x, y, z;
        if (sscanf(args, "%d,%d,%d", &x, &y, &z) != 3) return;
        SDL_LockMutex(s_worldMutex);
        world.player.pos.x = (double)x + 0.5;
        world.player.pos.y = (double)y;
        world.player.pos.z = (double)z + 0.5;
        SDL_UnlockMutex(s_worldMutex);
    }

    /* --- world.getHeight(x,z) --- */
    else if (strcmp(cmd, "world.getHeight") == 0) {
        int x, z;
        if (sscanf(args, "%d,%d", &x, &z) != 2) return;
        SDL_LockMutex(s_worldMutex);
        int h = 0;
        for (int y = 3 * CHUNK_SIZE - 1; y >= 0; y--) {
            if (World_getBlock(&world, x, y, z) != BLOCK_AIR) {
                h = y + 1;
                break;
            }
        }
        SDL_UnlockMutex(s_worldMutex);
        char resp[32];
        snprintf(resp, sizeof(resp), "%d\n", h);
        send_str(sock, resp);
    }

    /* --- chat.post(msg) --- */
    else if (strcmp(cmd, "chat.post") == 0) {
        /* TerraM4KC doesn't have a chat overlay yet - just print to console */
        printf("[mcpi] chat: %s\n", args);
    }
}

/* -------------------------------------------------------------------------
 * Client handler - runs per accepted connection, blocking
 * ---------------------------------------------------------------------- */
static void handle_client(TCPsocket client) {
    char buf[MCPI_BUF_SIZE];
    SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
    SDLNet_TCP_AddSocket(set, client);
    printf("[mcpi] client connected\n");
    while (s_running) {
        int ready = SDLNet_CheckSockets(set, 100);
        if (ready <= 0) continue;
        int n = recv_line(client, buf, sizeof(buf));
        if (n < 0) break;
        if (n == 0) continue;
        handle_command(client, buf);
    }
    SDLNet_FreeSocketSet(set);
    printf("[mcpi] client disconnected\n");
    SDLNet_TCP_Close(client);
}

/* -------------------------------------------------------------------------
 * Listener thread
 * ---------------------------------------------------------------------- */
static int mcpi_thread(void *data) {
    (void)data;

    IPaddress ip;
    if (SDLNet_ResolveHost(&ip, NULL, MCPI_PORT) < 0) {
        fprintf(stderr, "[mcpi] SDLNet_ResolveHost: %s\n", SDLNet_GetError());
        return 1;
    }

    TCPsocket server = SDLNet_TCP_Open(&ip);
    if (!server) {
        fprintf(stderr, "[mcpi] SDLNet_TCP_Open: %s\n", SDLNet_GetError());
        return 1;
    }

    printf("[mcpi] listening on port %d\n", MCPI_PORT);

    SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
    SDLNet_TCP_AddSocket(set, server);

    while (s_running) {
        /* Poll so we can check s_running */
        int ready = SDLNet_CheckSockets(set, 100);
        if (ready <= 0) continue;
        if (!SDLNet_SocketReady(server)) continue;

        TCPsocket client = SDLNet_TCP_Accept(server);
        if (!client) continue;

        handle_client(client);
    }

    SDLNet_FreeSocketSet(set);
    SDLNet_TCP_Close(server);
    printf("[mcpi] server stopped\n");
    return 0;
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */
int mcpi_init(void) {
    if (SDLNet_Init() < 0) {
        fprintf(stderr, "[mcpi] SDLNet_Init: %s\n", SDLNet_GetError());
        return 1;
    }

    s_worldMutex = SDL_CreateMutex();
    if (!s_worldMutex) {
        fprintf(stderr, "[mcpi] SDL_CreateMutex: %s\n", SDL_GetError());
        return 1;
    }

    s_running = 1;
    s_thread  = SDL_CreateThread(mcpi_thread, "mcpi", NULL);
    if (!s_thread) {
        fprintf(stderr, "[mcpi] SDL_CreateThread: %s\n", SDL_GetError());
        s_running = 0;
        SDL_DestroyMutex(s_worldMutex);
        return 1;
    }

    return 0;
}

void mcpi_quit(void) {
    s_running = 0;
    if (s_thread) {
        SDL_WaitThread(s_thread, NULL);
        s_thread = NULL;
    }
    SDLNet_Quit();
    if (s_worldMutex) {
        SDL_DestroyMutex(s_worldMutex);
        s_worldMutex = NULL;
    }
}