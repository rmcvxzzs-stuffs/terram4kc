#include "net.h"
#include "gui.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

NetState net = {0};

static int net_send_sock(TCPsocket sock, NetMessageType type, const void *data, int len) {
    uint8_t header[5];
    header[0] = (uint8_t)type;
    header[1] = (len >> 0) & 0xFF;
    header[2] = (len >> 8) & 0xFF;
    header[3] = (len >> 16) & 0xFF;
    header[4] = (len >> 24) & 0xFF;

    if (SDLNet_TCP_Send(sock, header, 5) < 5) return -1;
    if (len > 0 && SDLNet_TCP_Send(sock, data, len) < len) return -1;
    return 0;
}

static int net_recv_sock(TCPsocket sock, uint8_t *type, uint8_t *buf, int maxLen) {
    uint8_t header[5];
    if (SDLNet_TCP_Recv(sock, header, 5) < 5) return -1;

    *type = header[0];
    int len = header[1] | (header[2] << 8) | (header[3] << 16) | (header[4] << 24);
    if (len > maxLen) return -1;
    if (len > 0 && SDLNet_TCP_Recv(sock, buf, len) < len) return -1;

    return len;
}

int net_init_host(int port, const char *worldName) {
    memset(&net, 0, sizeof(NetState));
    net.isHost = 1;
    strncpy(net.hostWorldName, worldName, 15);
    net.hostWorldName[15] = 0;

    if (SDLNet_Init() < 0) {
        printf("SDLNet_Init failed: %s\n", SDLNet_GetError());
        return -1;
    }

    if (SDLNet_ResolveHost(&net.address, NULL, port) < 0) {
        printf("ResolveHost failed: %s\n", SDLNet_GetError());
        return -1;
    }

    net.listenSocket = SDLNet_TCP_Open(&net.address);
    if (!net.listenSocket) {
        printf("TCP_Open (listen) failed: %s\n", SDLNet_GetError());
        return -1;
    }

    net.players[0].active = 1;
    net.players[0].isHost = 1;
    strncpy(net.players[0].username, "Host", 15);
    net.localPlayerId = 0;
    net.active = 1;
    net.isInGame = 1;

    printf("Hosting server on port %d, world: %s\n", port, worldName);
    return 0;
}

int net_init_join(const char *hostIp, int port) {
    memset(&net, 0, sizeof(NetState));
    net.isHost = 0;

    if (SDLNet_Init() < 0) {
        printf("SDLNet_Init failed: %s\n", SDLNet_GetError());
        return -1;
    }

    if (SDLNet_ResolveHost(&net.address, hostIp, port) < 0) {
        printf("ResolveHost failed: %s\n", SDLNet_GetError());
        return -1;
    }

    net.hostSocket = SDLNet_TCP_Open(&net.address);
    if (!net.hostSocket) {
        printf("TCP_Open (connect) failed: %s\n", SDLNet_GetError());
        return -1;
    }

    // Send join request
    char joinMsg[32] = {0};
    strncpy(joinMsg, "Player", 31);
    net_send_sock(net.hostSocket, NET_MSG_JOIN, joinMsg, 32);

    net.active = 1;
    printf("Connecting to %s:%d...\n", hostIp, port);
    return 0;
}

void net_quit(void) {
    if (!net.active) return;

    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        if (net.players[i].socket) {
            SDLNet_TCP_Close(net.players[i].socket);
        }
    }
    if (net.listenSocket) SDLNet_TCP_Close(net.listenSocket);
    if (net.hostSocket) SDLNet_TCP_Close(net.hostSocket);

    SDLNet_Quit();
    net.active = 0;
    net.isInGame = 0;
    printf("Network shutdown\n");
}

static void net_handleClient(int playerId, World *world) {
    NetPlayer *p = &net.players[playerId];
    uint8_t type, buf[NET_BUFFER_SIZE];
    int len;

    while ((len = net_recv_sock(p->socket, &type, buf, NET_BUFFER_SIZE)) >= 0) {
        switch (type) {
        case NET_MSG_JOIN: {
            p->active = 1;
            strncpy(p->username, (char*)buf, 15);
            p->username[15] = 0;

            // Send welcome with world info
            uint8_t welcome[76];
            welcome[0] = (uint8_t)playerId;
            memcpy(welcome + 1, &world->seed, 4);
            welcome[5] = (uint8_t)world->type;
            welcome[6] = (uint8_t)world->dayNightMode;
            memcpy(welcome + 7, &world->time, 4);
            strncpy((char*)welcome + 11, net.hostWorldName, 64);

            net_send_sock(p->socket, NET_MSG_WELCOME, welcome, 75);
            printf("Player %d joined: %s\n", playerId, p->username);
            break;
        }

        case NET_MSG_PLAYER_POS: {
            if (len >= 32) {
                memcpy(&p->posX, buf, 8);
                memcpy(&p->posY, buf + 8, 8);
                memcpy(&p->posZ, buf + 16, 8);
                memcpy(&p->hRot, buf + 24, 4);
                memcpy(&p->vRot, buf + 28, 4);
            }
            // Relay to other players
            for (int i = 0; i < NET_MAX_PLAYERS; i++) {
                if (i != playerId && net.players[i].active && net.players[i].socket) {
                    uint8_t relay[33];
                    relay[0] = (uint8_t)playerId;
                    memcpy(relay + 1, buf, 32);
                    net_send_sock(net.players[i].socket, NET_MSG_PLAYER_POS, relay, 33);
                }
            }
            break;
        }

        case NET_MSG_BLOCK: {
            if (len >= 13) {
                int x, y, z;
                Block block;
                memcpy(&x, buf, 4);
                memcpy(&y, buf + 4, 4);
                memcpy(&z, buf + 8, 4);
                block = buf[12];
                World_setBlock(world, x, y, z, block, 1);
                // Relay to all other players
                for (int i = 0; i < NET_MAX_PLAYERS; i++) {
                    if (i != playerId && net.players[i].active && net.players[i].socket) {
                        net_send_sock(net.players[i].socket, NET_MSG_BLOCK, buf, 13);
                    }
                }
            }
            break;
        }

        case NET_MSG_CHAT: {
            buf[len] = 0;
            printf("[%s] %s\n", p->username, buf);
            // Relay
            for (int i = 0; i < NET_MAX_PLAYERS; i++) {
                if (i != playerId && net.players[i].active && net.players[i].socket) {
                    net_send_sock(net.players[i].socket, NET_MSG_CHAT, buf, len);
                }
            }
            break;
        }

        case NET_MSG_LEAVE: {
            printf("Player %d left\n", playerId);
            p->active = 0;
            SDLNet_TCP_Close(p->socket);
            p->socket = NULL;
            return;
        }
        }
    }
}

void net_update(World *world, Player *localPlayer) {
    if (!net.active) return;

    // Host: accept new connections
    if (net.isHost && net.listenSocket) {
        TCPsocket newSocket = SDLNet_TCP_Accept(net.listenSocket);
        if (newSocket) {
            for (int i = 1; i < NET_MAX_PLAYERS; i++) {
                if (!net.players[i].active) {
                    net.players[i].socket = newSocket;
                    net.players[i].active = 1;
                    printf("Incoming connection -> slot %d\n", i);
                    break;
                }
            }
        }
    }

    // Client: handle host messages
    if (!net.isHost && net.hostSocket) {
        uint8_t type, buf[NET_BUFFER_SIZE];
        int len;
        while ((len = net_recv_sock(net.hostSocket, &type, buf, NET_BUFFER_SIZE)) >= 0) {
            switch (type) {
            case NET_MSG_WELCOME: {
                net.localPlayerId = buf[0];
                memcpy(&world->seed, buf + 1, 4);
                world->type = buf[5];
                world->dayNightMode = buf[6];
                memcpy(&world->time, buf + 7, 4);
                net.isInGame = 1;
                printf("Joined as player %d, seed=%llu\n", net.localPlayerId, (unsigned long long)world->seed);
                break;
            }
            case NET_MSG_PLAYER_POS: {
                if (len >= 33) {
                    int pid = buf[0];
                    if (pid >= 0 && pid < NET_MAX_PLAYERS && pid != net.localPlayerId) {
                        memcpy(&net.players[pid].posX, buf + 1, 8);
                        memcpy(&net.players[pid].posY, buf + 9, 8);
                        memcpy(&net.players[pid].posZ, buf + 17, 8);
                        memcpy(&net.players[pid].hRot, buf + 25, 4);
                        memcpy(&net.players[pid].vRot, buf + 29, 4);
                        net.players[pid].active = 1;
                    }
                }
                break;
            }
            case NET_MSG_BLOCK: {
                if (len >= 13) {
                    int x, y, z;
                    Block block;
                    memcpy(&x, buf, 4);
                    memcpy(&y, buf + 4, 4);
                    memcpy(&z, buf + 8, 4);
                    block = buf[12];
                    World_setBlock(world, x, y, z, block, 1);
                }
                break;
            }
            case NET_MSG_CHAT: {
                buf[len] = 0;
                chatAdd((char*)buf);
                break;
            }
            }
        }
    }

    // Host: handle all clients
    if (net.isHost) {
        for (int i = 1; i < NET_MAX_PLAYERS; i++) {
            if (net.players[i].socket && net.players[i].active) {
                net_handleClient(i, world);
            }
        }
    }

    // Send local player position (20Hz)
    static uint32_t lastPosSend = 0;
    if (net.isInGame && SDL_GetTicks() - lastPosSend > 50) {
        lastPosSend = SDL_GetTicks();

        uint8_t posBuf[32];
        memcpy(posBuf, &localPlayer->pos.x, 8);
        memcpy(posBuf + 8, &localPlayer->pos.y, 8);
        memcpy(posBuf + 16, &localPlayer->pos.z, 8);
        memcpy(posBuf + 24, &localPlayer->hRot, 4);
        memcpy(posBuf + 28, &localPlayer->vRot, 4);

        if (net.isHost) {
            // Host updates its own position in players[0]
            net.players[0].posX = localPlayer->pos.x;
            net.players[0].posY = localPlayer->pos.y;
            net.players[0].posZ = localPlayer->pos.z;
            net.players[0].hRot = localPlayer->hRot;
            net.players[0].vRot = localPlayer->vRot;
            // Broadcast to clients
            for (int i = 1; i < NET_MAX_PLAYERS; i++) {
                if (net.players[i].active && net.players[i].socket) {
                    uint8_t relay[33];
                    relay[0] = 0; // host is player 0
                    memcpy(relay + 1, posBuf, 32);
                    net_send_sock(net.players[i].socket, NET_MSG_PLAYER_POS, relay, 33);
                }
            }
        } else if (net.hostSocket) {
            net_send_sock(net.hostSocket, NET_MSG_PLAYER_POS, posBuf, 32);
        }
    }

    // Send pending chat
    if (net.hasPendingChat) {
        if (net.isHost) {
            chatAdd(net.pendingChat);
            for (int i = 1; i < NET_MAX_PLAYERS; i++) {
                if (net.players[i].active && net.players[i].socket) {
                    net_send_sock(net.players[i].socket, NET_MSG_CHAT, net.pendingChat, strlen(net.pendingChat));
                }
            }
        } else if (net.hostSocket) {
            net_send_sock(net.hostSocket, NET_MSG_CHAT, net.pendingChat, strlen(net.pendingChat));
        }
        net.hasPendingChat = 0;
    }
}

void net_sendChat(const char *text) {
    if (!net.active) return;
    strncpy(net.pendingChat, text, 127);
    net.pendingChat[127] = 0;
    net.hasPendingChat = 1;
}

void net_broadcastBlock(int x, int y, int z, Block block) {
    if (!net.active || !net.isInGame) return;

    uint8_t buf[13];
    memcpy(buf, &x, 4);
    memcpy(buf + 4, &y, 4);
    memcpy(buf + 8, &z, 4);
    buf[12] = block;

    if (net.isHost) {
        for (int i = 1; i < NET_MAX_PLAYERS; i++) {
            if (net.players[i].active && net.players[i].socket) {
                net_send_sock(net.players[i].socket, NET_MSG_BLOCK, buf, 13);
            }
        }
    } else if (net.hostSocket) {
        net_send_sock(net.hostSocket, NET_MSG_BLOCK, buf, 13);
    }
}