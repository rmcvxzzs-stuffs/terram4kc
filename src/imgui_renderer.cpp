#include "imgui_renderer.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

extern "C" {
#include "terrain.h"
#include "player.h"

extern World  world;
extern Player *player;
extern int    g_debug_mode;
}

extern "C" ImguiDebugState g_imgui_debug = { 0 };

extern "C" {

void imgui_init(SDL_Window *window, SDL_Renderer *renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().Alpha = 0.85f;
    ImGui::GetStyle().WindowRounding = 4.0f;

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
    if (g_debug_mode) {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(290, 420), ImGuiCond_Once);
        ImGui::Begin("TerraM4KC Debug");

        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("FPS: %d", g_imgui_debug.fps);
            float ft = g_imgui_debug.fps > 0 ? 1000.0f / g_imgui_debug.fps : 0.0f;
            ImGui::Text("Frame time: %.2f ms", ft);
            ImGui::ProgressBar(g_imgui_debug.fps / 60.0f, ImVec2(-1, 0), "");
        }

        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Pos:   X=%.2f  Y=%.2f  Z=%.2f",
                player->pos.x, player->pos.y, player->pos.z);
            ImGui::Text("Chunk: X=%d  Y=%d  Z=%d",
                (int)player->pos.x >> 6,
                (int)player->pos.y >> 6,
                (int)player->pos.z >> 6);
            ImGui::Separator();
            double hDeg = player->hRot * (180.0 / 3.14159265);
            double vDeg = player->vRot * (180.0 / 3.14159265);
            ImGui::Text("Yaw:   %.1f deg  (%.3f rad)", hDeg, player->hRot);
            ImGui::Text("Pitch: %.1f deg  (%.3f rad)", vDeg, player->vRot);
            ImGui::Separator();
            ImGui::Text("LookH: X=%.3f  Y=%.3f", player->vectorH.x, player->vectorH.y);
            ImGui::Text("LookV: X=%.3f  Y=%.3f", player->vectorV.x, player->vectorV.y);
            ImGui::Separator();
            ImGui::Text("Hotbar: slot %d", player->inventory.hotbarSelect);
        }

        if (ImGui::CollapsingHeader("World")) {
            ImGui::Text("Time:          %d", world.time);
            ImGui::Text("Seed:          %d", world.seed);
            ImGui::Text("Day/Night:     %d", world.dayNightMode);
        }

        if (ImGui::CollapsingHeader("Block Target")) {
            if (g_imgui_debug.blockSelected) {
                ImGui::TextColored(ImVec4(0.4f,1,0.4f,1), "Targeting block");
                ImGui::Text("X=%d  Y=%d  Z=%d",
                    g_imgui_debug.blockX,
                    g_imgui_debug.blockY,
                    g_imgui_debug.blockZ);
            } else {
                ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "No block selected");
            }
        }

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), NULL);
}

int imgui_wants_mouse(void) {
    return ImGui::GetIO().WantCaptureMouse;
}

int imgui_wants_keyboard(void) {
    return ImGui::GetIO().WantCaptureKeyboard;
}

} // extern "C"