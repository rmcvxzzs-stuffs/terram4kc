#pragma once

/* mcpi
 * Minecraft Pi Edition API server for TerraM4KC.
 * Activated with --mcpi-on flag.
 * Listens on TCP port 4711, speaks the standard MCPI text protocol.
 *
 * Supported commands:
 *   world.getBlock(x,y,z)
 *   world.setBlock(x,y,z,id)
 *   world.setBlocks(x1,y1,z1,x2,y2,z2,id)
 *   player.getPos()
 *   player.setPos(x,y,z)
 *   player.getTile()
 *   player.setTile(x,y,z)
 *   world.getHeight(x,z)
 *   chat.post(msg)
 */

int  mcpi_init (void);   /* Start listener thread. Returns 0 on success. */
void mcpi_quit (void);   /* Signal thread to stop and join. */