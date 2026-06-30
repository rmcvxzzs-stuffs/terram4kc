# Building TerraM4KC

## Prerequisities

- Linux/MacOS/Windows with CMake 3.10+
- MSYS2 Installed (preferrably latest version) with the PATH set up and installedon the C: drive
- A functional computer/laptop
- A functional brain

## Guide

0. You don't have to install SDL2/other libs!! The `include` headers are already vendored at `win/` (for SDL2) and their respective names (for other libraries like `discord-rpc/` and `imgui/`) (im sorry if this made the repo bigger)

1. Assuming you have cloned the repository, see below for options.

| Generators            | Commands                                         |
| --------------------- | ------------------------------------------------ |
| Ninja                 | `cmake -B build -G Ninja -S .`                   |
| Visual Studio 18 2026 | `cmake -B build -G "Visual Studio 18 2026" -S .` |
| MinGW Makefiles       | `cmake -B build -G "MinGW Makefiles" -S .`       |

Build files are generated on `./build`.

2. Type `cmake --build build` (or `ninja -C build` if using ninja) to build TerraM4KC. Executable and output will be at the `./build/output` folder.<br> Don't worry, the libraries' DLL (SDL2, SDL2_net, Discord RPC) are already copied.

There you have it, Fresh TerraM4KC out of the terminal!
