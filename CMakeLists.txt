cmake_minimum_required(VERSION 3.10)
project(terram4kc VERSION 0.1.0 LANGUAGES C CXX)

# Options
option(TERRAM4KC_SMALL "Build optimized small binary" OFF)
option(TERRAM4KC_INSTALL "Enable install targets" ON)
option(TERRAM4KC_DISCORD_RPC "Enable Discord Rich Presence" ON)

# Set output directory for all build artifacts
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/output)

# SDL2 finding — cross-platform
if(WIN32)
    if(DEFINED ENV{SDL2_DIR})
        set(SDL2_DIR $ENV{SDL2_DIR})
    endif()
    
    if(EXISTS "${CMAKE_SOURCE_DIR}/win/SDL2/include/SDL2/SDL.h")
        set(SDL2_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/win/SDL2/include/SDL2")
        set(SDL2_LIBRARIES 
            "${CMAKE_SOURCE_DIR}/win/SDL2/lib/libSDL2.dll.a"
            # "${CMAKE_SOURCE_DIR}/win/SDL2/lib/libSDL2main.a"
        )
        message(STATUS "Using bundled SDL2 from win/SDL2/")
    elseif(SDL2_DIR)
        set(SDL2_INCLUDE_DIRS "${SDL2_DIR}/include")
        find_library(SDL2_LIBRARY SDL2 PATHS "${SDL2_DIR}/lib" "${SDL2_DIR}/lib/x64" NO_DEFAULT_PATH)
        find_library(SDL2MAIN_LIBRARY SDL2main PATHS "${SDL2_DIR}/lib" "${SDL2_DIR}/lib/x64" NO_DEFAULT_PATH)
        set(SDL2_LIBRARIES ${SDL2MAIN_LIBRARY} ${SDL2_LIBRARY})
        message(STATUS "Using SDL2 from ${SDL2_DIR}")
    else()
        find_package(SDL2 QUIET)
        if(NOT SDL2_FOUND)
            message(FATAL_ERROR "SDL2 not found. Set SDL2_DIR env var, or place SDL2 in win/SDL2/")
        endif()
    endif()
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET sdl2)
    endif()
    if(NOT SDL2_FOUND)
        find_package(SDL2 REQUIRED)
    endif()
endif()

# SDL2_net finding — bundled in win/
if(WIN32)
    if(EXISTS "${CMAKE_SOURCE_DIR}/win/SDL2_net/include/SDL2/SDL_net.h")
        set(SDL2_NET_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/win/SDL2_net/include/SDL2")
        if(EXISTS "${CMAKE_SOURCE_DIR}/win/SDL2_net/lib/libSDL2_net.dll.a")
            set(SDL2_NET_LIBRARIES "${CMAKE_SOURCE_DIR}/win/SDL2_net/lib/libSDL2_net.dll.a")
            message(STATUS "Using bundled SDL2_net (MinGW) from win/SDL2_net/")
        elseif(EXISTS "${CMAKE_SOURCE_DIR}/win/SDL2_net/lib/SDL2_net.lib")
            set(SDL2_NET_LIBRARIES "${CMAKE_SOURCE_DIR}/win/SDL2_net/lib/SDL2_net.lib")
            message(STATUS "Using bundled SDL2_net (MSVC) from win/SDL2_net/")
        else()
            message(FATAL_ERROR "SDL2_net headers found but no libraries in win/SDL2_net/lib/")
        endif()
    else()
        message(FATAL_ERROR "SDL2_net not found in win/SDL2_net/. Download from https://github.com/libsdl-org/SDL_net/releases")
    endif()
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2_NET QUIET SDL2_net)
    endif()
    if(NOT SDL2_NET_FOUND)
        find_library(SDL2_NET_LIBRARY SDL2_net)
        find_path(SDL2_NET_INCLUDE_DIR SDL_net.h PATH_SUFFIXES SDL2)
        if(SDL2_NET_LIBRARY AND SDL2_NET_INCLUDE_DIR)
            set(SDL2_NET_LIBRARIES ${SDL2_NET_LIBRARY})
            set(SDL2_NET_INCLUDE_DIRS ${SDL2_NET_INCLUDE_DIR})
            set(SDL2_NET_FOUND TRUE)
        else()
            message(FATAL_ERROR "SDL2_net not found. Install libsdl2-net-dev or similar.")
        endif()
    endif()
endif()

# Discord RPC setup
if(TERRAM4KC_DISCORD_RPC)
    if(WIN32)
        if(EXISTS "${CMAKE_SOURCE_DIR}/discord-rpc/include/discord_rpc.h")
            set(DISCORD_RPC_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/discord-rpc/include")
            if(EXISTS "${CMAKE_SOURCE_DIR}/discord-rpc/lib/libdiscord-rpc.dll.a")
                set(DISCORD_RPC_LIBRARIES "${CMAKE_SOURCE_DIR}/discord-rpc/lib/libdiscord-rpc.dll.a")
                set(DISCORD_RPC_DLL "${CMAKE_SOURCE_DIR}/discord-rpc/bin/discord-rpc.dll")
                message(STATUS "Using bundled Discord RPC (MinGW) from discord-rpc/")
            elseif(EXISTS "${CMAKE_SOURCE_DIR}/discord-rpc/lib/discord-rpc.lib")
                set(DISCORD_RPC_LIBRARIES "${CMAKE_SOURCE_DIR}/discord-rpc/lib/discord-rpc.lib")
                set(DISCORD_RPC_DLL "${CMAKE_SOURCE_DIR}/discord-rpc/bin/discord-rpc.dll")
                message(STATUS "Using bundled Discord RPC (MSVC) from discord-rpc/")
            else()
                message(WARNING "Discord RPC headers found but no libraries. Disabling Discord RPC.")
                set(TERRAM4KC_DISCORD_RPC OFF)
            endif()
        else()
            message(WARNING "Discord RPC not found in discord-rpc/ folder. Disabling Discord RPC.")
            set(TERRAM4KC_DISCORD_RPC OFF)
        endif()
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(DISCORD_RPC QUIET discord-rpc)
        endif()
        if(NOT DISCORD_RPC_FOUND)
            find_library(DISCORD_RPC_LIBRARY discord-rpc)
            find_path(DISCORD_RPC_INCLUDE_DIR discord_rpc.h PATH_SUFFIXES discord-rpc)
            if(DISCORD_RPC_LIBRARY AND DISCORD_RPC_INCLUDE_DIR)
                set(DISCORD_RPC_LIBRARIES ${DISCORD_RPC_LIBRARY})
                set(DISCORD_RPC_INCLUDE_DIRS ${DISCORD_RPC_INCLUDE_DIR})
                set(DISCORD_RPC_FOUND TRUE)
            else()
                message(WARNING "Discord RPC not found. Disabling Discord RPC.")
                set(TERRAM4KC_DISCORD_RPC OFF)
            endif()
        endif()
    endif()
endif()

# Source files
set(SOURCES
    src/main.c
    src/audio.c
    src/discord_presence.c
    src/data.c
    src/gameloop.c
    src/gui.c
    src/inputbuffer.c
    src/menus.c
    src/minecraftfont.c
    src/options.c
    src/player.c
    src/player_model.c
    src/net.c
    src/terrain.c
    src/textures.c
    src/utility.c
    src/imgui_renderer.cpp
)

set(HEADERS
    src/blocks.h
    src/compat.h
    src/coords.h
    src/data.h
    src/gameloop.h
    src/gui.h
    src/inputbuffer.h
    src/main.h
    src/menus.h
    src/minecraftfont.h
    src/options.h
    src/player.h
    src/player_model.h
    src/net.h
    src/terrain.h
    src/textures.h
    src/utility.h
    src/audio.h
    src/discord_presence.h
    src/imgui_renderer.h
)

# Dear ImGui (SDL2 + SDLRenderer2 backends)
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/imgui")

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${SDL2_INCLUDE_DIRS}
)

set_target_properties(imgui PROPERTIES LINKER_LANGUAGE CXX)

add_executable(m4kc ${SOURCES} ${HEADERS})

target_include_directories(m4kc PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_NET_INCLUDE_DIRS} src)
target_link_libraries(m4kc PRIVATE imgui ${SDL2_LIBRARIES} ${SDL2_NET_LIBRARIES})

# Discord RPC compile definitions and linking
if(TERRAM4KC_DISCORD_RPC)
    target_compile_definitions(m4kc PRIVATE TERRAM4KC_DISCORD_RPC=1)
    target_include_directories(m4kc PRIVATE ${DISCORD_RPC_INCLUDE_DIRS})
    target_link_libraries(m4kc PRIVATE ${DISCORD_RPC_LIBRARIES})
    message(STATUS "Discord RPC enabled")
else()
    message(STATUS "Discord RPC disabled")
endif()

# Platform-specific
if(WIN32)
    # set_target_properties(m4kc PROPERTIES WIN32_EXECUTABLE TRUE)
    
    # Copy SDL2.dll
    if(EXISTS "${CMAKE_SOURCE_DIR}/win/SDL2.dll")
        add_custom_command(TARGET m4kc POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/win/SDL2.dll"
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            COMMENT "Copying SDL2.dll"
        )
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/win/SDL2/bin/SDL2.dll")
        add_custom_command(TARGET m4kc POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/win/SDL2/bin/SDL2.dll"
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            COMMENT "Copying SDL2.dll from bin"
        )
    endif()
    
    # Copy SDL2_net.dll
    if(EXISTS "${CMAKE_SOURCE_DIR}/win/SDL2_net/include/SDL2/SDL_net.h")
        add_custom_command(TARGET m4kc POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/win/SDL2_net/bin/SDL2_net.dll"
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            COMMENT "Copying SDL2_net.dll"
        )
    endif()
    
    # Copy Discord RPC DLL if enabled
    if(TERRAM4KC_DISCORD_RPC AND EXISTS "${DISCORD_RPC_DLL}")
        add_custom_command(TARGET m4kc POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DISCORD_RPC_DLL}"
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            COMMENT "Copying discord-rpc.dll"
        )
    endif()

    # Copy MinGW runtime DLLs (needed when linking C++)
    foreach(MINGW_DLL
        libgcc_s_seh-1.dll
        libstdc++-6.dll
        libwinpthread-1.dll
    )
        find_file(MINGW_DLL_PATH_${MINGW_DLL} ${MINGW_DLL}
            PATHS
                "C:/msys64/mingw64/bin"
                "C:/mingw64/bin"
                "C:/mingw/bin"
            NO_DEFAULT_PATH
        )
        if(MINGW_DLL_PATH_${MINGW_DLL})
            add_custom_command(TARGET m4kc POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_DLL_PATH_${MINGW_DLL}}"
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
                COMMENT "Copying ${MINGW_DLL}"
            )
        endif()
    endforeach()
endif()

# Temporary console mode
if(MINGW)
    target_link_options(m4kc PRIVATE -mconsole)
endif()

# Copy assets folder to output directory
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_custom_command(TARGET m4kc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
        COMMENT "Copying assets folder"
    )
endif()

# Compiler flags
if(MSVC)
    target_compile_options(m4kc PRIVATE /W4)
    if(TERRAM4KC_SMALL)
        target_compile_options(m4kc PRIVATE /O1 /GL)
        target_link_options(m4kc PRIVATE /LTCG)
    endif()
else()
    target_compile_options(m4kc PRIVATE -Wall -Wextra)
    if(TERRAM4KC_SMALL)
        target_compile_options(m4kc PRIVATE
            -Os -g0
            -fno-stack-protector
            -fno-unwind-tables
            -fno-asynchronous-unwind-tables
        )
        target_compile_definitions(m4kc PRIVATE small)
        target_link_options(m4kc PRIVATE -s)
        if(NOT WIN32)
            add_custom_command(TARGET m4kc POST_BUILD
                COMMAND strip $<TARGET_FILE:m4kc>
                    -S --strip-unneeded
                    --remove-section=.note.gnu.gold-version
                    --remove-section=.comment
                    --remove-section=.note
                    --remove-section=.note.gnu.build-id
                    --remove-section=.note.ABI-tag
                COMMENT "Stripping binary"
            )
        endif()
    else()
        target_compile_options(m4kc PRIVATE -g)
    endif()
endif()

# Install
if(TERRAM4KC_INSTALL AND NOT WIN32)
    include(GNUInstallDirs)
    install(TARGETS m4kc DESTINATION ${CMAKE_INSTALL_PREFIX}/games)
    install(FILES m4kc.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
    install(FILES icons/icon.png DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/256x256/apps RENAME m4kc.png)
endif()

# CPack
set(CPACK_PACKAGE_NAME "TerraM4KC")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)