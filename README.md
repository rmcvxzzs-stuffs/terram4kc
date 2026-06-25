# TerraM4KC

![Grass block icon](icons/icon.png)

### TerraM4KC is a modified version of M4KC that adds New Features.
It adds many new features that are (definitely) not on the M4KC roadmap.
Simply, i am not affiliated with Sasha Koshka nor Mojang Studios.

### Features implemented:
- Discord RPC
- Audio (only clicking is there atm)
- Player model (really experimental.)
- Multiplayer (experimental)

### Libraries used by TerraM4KC/M4KC:
- SDL2
- SDL2_net
- Dear ImGui
- miniaudio
- Discord RPC (deprecated version)
- CMake build system

### Notes:
- The [build.sh](./old/build.sh) system is now retired, please do not use it for building as it was not edited! (im sorry.)
- Expect the executable file to be above 1mb, as i integrated ImGui.
- There's an unused [`dpad.h`](./src/android/dpad.h). it is there as a placeholder for now.
- TerraM4KC will have [localization/translation](./lang/), they're just placeholders for now

~~[You can click this to see the instruction on how to build.](https://github.com/sashakoshka/m4kc/wiki/Building-From-Source)~~ Check [BUILDING.md](/BUILDING.md).
