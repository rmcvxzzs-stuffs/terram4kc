#include "imgui_renderer.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

extern "C" {
#include "terrain.h"
#include "player.h"
#include "main.h"
#include "gui.h"

extern World  world;
extern Player *player;
extern int    g_debug_mode;
}

extern "C" ImguiDebugState g_imgui_debug;
ImguiDebugState g_imgui_debug = { 0 };

static SDL_Renderer *s_renderer = NULL;

extern "C" {

void imgui_init(SDL_Window *window, SDL_Renderer *renderer) {
    s_renderer = renderer;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.Alpha         = 0.88f;
    style.WindowRounding = 5.0f;
    style.FrameRounding  = 3.0f;
    style.GrabRounding   = 3.0f;
    style.WindowPadding  = ImVec2(10, 8);

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
}

void imgui_shutdown(void) {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void imgui_process_event(SDL_Event *event) {
    ImGui_ImplSDL2_ProcessEvent(event);
}

void imgui_new_frame(void) {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void imgui_render(void) {
    SDL_RenderSetScale(s_renderer, 1.0f, 1.0f);

    if (g_debug_mode) {
        // Menu bar window (top strip, full width)
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 0), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::Begin("##menubar", NULL,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_MenuBar
        );
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Debug")) {
                ImGui::MenuItem("Show Overlay", NULL, true);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Player")) {
                ImGui::MenuItem("(view only)", NULL, false, false);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("World")) {
                ImGui::MenuItem("(view only)", NULL, false, false);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGui::End();

        // Main debug panel
        float menuBarHeight = ImGui::GetFrameHeight();
        ImGui::SetNextWindowPos(ImVec2(10, menuBarHeight + 6), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(300, 460), ImGuiCond_Once);
        ImGui::Begin("TerraM4KC Debug");

        // --- Performance ---
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            float ft = g_imgui_debug.fps > 0 ? 1000.0f / g_imgui_debug.fps : 0.0f;
            ImGui::Text("FPS: %d  (%.2f ms/frame)", g_imgui_debug.fps, ft);
            float frac = (float)g_imgui_debug.fps / 60.0f;
            if (frac > 1.0f) frac = 1.0f;
            ImVec4 col = frac > 0.75f
                ? ImVec4(0.2f,0.9f,0.2f,1)
                : frac > 0.4f
                    ? ImVec4(0.9f,0.8f,0.1f,1)
                    : ImVec4(0.9f,0.2f,0.2f,1);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
            ImGui::ProgressBar(frac, ImVec2(-1, 6), "");
            ImGui::PopStyleColor();
        }

        // --- Player ---
        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (player == NULL) {
                ImGui::TextColored(ImVec4(1,0.5f,0.5f,1), "Not in game");
            } else {
                ImGui::Text("XYZ:   %.3f / %.3f / %.3f",
                    player->pos.x, player->pos.y, player->pos.z);
                ImGui::Text("Chunk: %d / %d / %d",
                    (int)player->pos.x >> 6,
                    (int)player->pos.y >> 6,
                    (int)player->pos.z >> 6);
                ImGui::Separator();
                double hDeg = player->hRot * (180.0 / 3.14159265358979);
                double vDeg = player->vRot * (180.0 / 3.14159265358979);
                const char *facing = "?";
                double ha = fmod(hDeg + 360.0, 360.0);
                if      (ha <  45.0 || ha >= 315.0) facing = "North (-Z)";
                else if (ha <  135.0)               facing = "East  (+X)";
                else if (ha <  225.0)               facing = "South (+Z)";
                else                                facing = "West  (-X)";
                ImGui::Text("Facing: %s", facing);
                ImGui::Text("Yaw:   %.1f deg  (%.3f rad)", hDeg, player->hRot);
                ImGui::Text("Pitch: %.1f deg  (%.3f rad)", vDeg, player->vRot);
                ImGui::Separator();
                ImGui::Text("VelFB: %.4f  VelLR: %.4f",
                    g_imgui_debug.velFB, g_imgui_debug.velLR);
                ImGui::Separator();
                const char *waterState = "Dry";
                if (g_imgui_debug.headInWater)      waterState = "Submerged";
                else if (g_imgui_debug.feetInWater) waterState = "Feet in water";
                ImGui::Text("Water: %s", waterState);
                ImGui::Text("Hotbar slot: %d", player->inventory.hotbarSelect);
            }
        }

        // --- World ---
        if (ImGui::CollapsingHeader("World")) {
            ImGui::Text("Name:      %s", world.path[0] ? world.path : "(none)");
            ImGui::Text("Seed:      %llu", (unsigned long long)world.seed);
            ImGui::Text("Time:      %llu", (unsigned long long)world.time);
            float dayFrac = (float)((world.time % 102944) / 102944.0);
            ImGui::Text("Day cycle: %.1f%%", dayFrac * 100.0f);
            ImGui::ProgressBar(dayFrac, ImVec2(-1, 6), "");
            ImGui::Text("Day/Night mode: %d", world.dayNightMode);
            ImGui::Text("World type:     %d", world.type);
        }

        // --- Block Target ---
        if (ImGui::CollapsingHeader("Block Target")) {
            if (g_imgui_debug.blockSelected) {
                ImGui::TextColored(ImVec4(0.4f,1,0.4f,1), "Targeting block");
                ImGui::Text("X=%d  Y=%d  Z=%d",
                    g_imgui_debug.blockX,
                    g_imgui_debug.blockY,
                    g_imgui_debug.blockZ);
            } else {
                ImGui::TextColored(ImVec4(1,0.5f,0.5f,1), "No block targeted");
            }
        }

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), s_renderer);
    SDL_RenderSetScale(s_renderer, (float)BUFFER_SCALE, (float)BUFFER_SCALE);
}

int imgui_wants_mouse(void) {
    return ImGui::GetIO().WantCaptureMouse;
}

int imgui_wants_keyboard(void) {
    return ImGui::GetIO().WantCaptureKeyboard;
}

} // extern "C"