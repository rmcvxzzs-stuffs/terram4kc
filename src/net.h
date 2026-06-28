#ifndef NET_H
#define NET_H

#include <SDL2/SDL.h>
#include <SDL_net.h>
#include "terrain.h"
#include "main.h"

#define NET_MAX_PLAYERS 8
#define NET_PORT 42069
#define NET_BUFFER_SIZE 4096

typedef enum {
    NET_MSG_JOIN,
    NET_MSG_WELCOME,
    NET_MSG_PLAYER_POS,
    NET_MSG_BLOCK,
    NET_MSG_CHAT,
    NET_MSG_LEAVE,
    NET_MSG_PING,
} NetMessageType;

typedef struct {
    int active;
    int isHost;
    TCPsocket socket;
    IPaddress address;
    char username[16];
    double posX, posY, posZ;
    double hRot, vRot;
    uint32_t lastPing;
} NetPlayer;

typedef struct {
    int active;
    int isHost;
    TCPsocket listenSocket;
    TCPsocket hostSocket;
    IPaddress address;
    NetPlayer players[NET_MAX_PLAYERS];
    int localPlayerId;
    char pendingChat[128];
    int hasPendingChat;
    char hostWorldName[16];
    int isInGame;
} NetState;

extern NetState net;

int net_init_host(int port, const char *worldName);
int net_init_join(const char *hostIp, int port);
void net_quit(void);
void net_update(World *world, Player *localPlayer);
void net_sendChat(const char *text);
void net_broadcastBlock(int x, int y, int z, Block block);

#endif