#include <discord_rpc.h>  // SDK header
#include "discord_presence.h"

#ifdef TERRAM4KC_DISCORD_RPC
#include "../discord-rpc/include/discord_rpc.h"
#include <stdio.h>

#define DISCORD_CLIENT_ID "1346042657251659827"

static int initialized = 0;

static void discord_ready(const DiscordUser *request) {
    (void)request;
    printf("Discord RPC connected!\n");
}

static void discord_disconnected(int errcode, const char *message) {
    printf("Discord RPC disconnected: %d - %s\n", errcode, message);
}

static void discord_error(int errcode, const char *message) {
    printf("Discord RPC error: %d - %s\n", errcode, message);
}

int discord_rpc_init(void) {
    DiscordEventHandlers handlers = {
        .ready = discord_ready,
        .disconnected = discord_disconnected,
        .errored = discord_error,
        .joinGame = NULL,
        .spectateGame = NULL,
        .joinRequest = NULL
    };

    Discord_Initialize(DISCORD_CLIENT_ID, &handlers, 1, NULL);
    initialized = 1;
    return 0;
}

void discord_rpc_quit(void) {
    if (!initialized) return;
    Discord_Shutdown();
    initialized = 0;
}

void discord_rpc_update_presence(const char *state, const char *details, int64_t start_timestamp) {
    if (!initialized) return;

    DiscordRichPresence presence = {0};
    presence.state = state;
    presence.details = details;
    presence.startTimestamp = start_timestamp;
    presence.largeImageKey = "m4kc_icon";
    presence.largeImageText = "TerraM4KC";
    presence.instance = 0;

    Discord_UpdatePresence(&presence);
}

#else
// Stubs when Discord RPC is disabled

int discord_rpc_init(void) { return 0; }
void discord_rpc_quit(void) { }
void discord_rpc_update_presence(const char *state, const char *details, int64_t start_timestamp) {
    (void)state; (void)details; (void)start_timestamp;
}

#endif