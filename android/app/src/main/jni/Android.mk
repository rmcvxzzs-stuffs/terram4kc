LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := mcvoxel

LOCAL_SRC_FILES := \
    ../../../../src/main.c \
    ../../../../src/gameloop.c \
    ../../../../src/gui.c \
    ../../../../src/inputbuffer.c \
    ../../../../src/menus.c \
    ../../../../src/minecraftfont.c \
    ../../../../src/options.c \
    ../../../../src/player.c \
    ../../../../src/terrain.c \
    ../../../../src/textures.c \
    ../../../../src/utility.c \
    ../../../../src/data.c

LOCAL_C_INCLUDES := ../../../../src

LOCAL_LDLIBS := -llog -landroid

include $(BUILD_SHARED_LIBRARY)
