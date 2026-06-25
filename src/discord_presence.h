#ifndef DISCORD_PRESENCE_H
#define DISCORD_PRESENCE_H

#include <stdint.h>

int discord_rpc_init(void);
void discord_rpc_quit(void);
void discord_rpc_update_presence(const char *state, const char *details, int64_t start_timestamp);

#endif