#include "desktop/sdl_compat.h"
extern "C" {
#include "fzero_menu.h"
#include "fzero_dlss.h"
}
#include "recomp_runtime_ui.h"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#if SNESRECOMP_SDL3
#include "backends/imgui_impl_sdl3.h"
#include "third_party/imgui-sdl3/imgui_impl_sdlrenderer3.h"
#else
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
SDL_Window *window;
SDL_Renderer *renderer;
ImGuiContext *context;
RecompRuntimeUi *menu;
RecompRuntimeUiItem items[13];
FzeroVideoSettings *settings;
bool *enabled, *compare, vulkan;
int *running;
const char *path;
char status[64];
const char *const styles[] = {"Default", "Natural", "Cinematic", "3", "4", "5", "6"};
const char *const presets[] = {"0", "1", "2", "3"};

int param_index(const RecompRuntimeUiItem *item) {
    for (int i = 0; i < FZERO_DLSS_PARAM_COUNT; ++i)
        if (!std::strcmp(item->key, fzero_dlss_params[i].key)) return i;
    return -1;
}
int get(void *, const RecompRuntimeUiItem *item, int *out) {
    int i = param_index(item);
    if (i >= 0) *out = settings->dlss_params[i];
    else if (!std::strcmp(item->key, "enabled")) *out = *enabled;
    else if (!std::strcmp(item->key, "compare")) *out = *compare;
    else return 0;
    return 1;
}
int set(void *, const RecompRuntimeUiItem *item, int value) {
    std::fprintf(stderr, "[menu] %s=%d\n", item->key, value);
    int i = param_index(item);
    if (i >= 0) {
        if (!FzeroDlssSetParam(settings, i, value)) return 0;
        FzeroDlssConfigure(settings);
    } else if (!std::strcmp(item->key, "enabled")) {
        if (value) { FzeroDlssConfigure(settings); *enabled = FzeroDlssStart(); }
        else { FzeroDlssStop(); *enabled = false; }
        settings->dlss = *enabled;
    } else if (!std::strcmp(item->key, "compare")) *compare = value != 0;
    else return 0;
    return 1;
}
void save(void *);
int action(void *, const RecompRuntimeUiItem *item) {
    if (!std::strcmp(item->key, "defaults")) {
        FzeroDlssDefaults(settings);
        FzeroDlssConfigure(settings);
        save(nullptr);
    } else if (!std::strcmp(item->key, "quit")) *running = 0;
    else if (!std::strcmp(item->key, "resume")) recomp_runtime_ui_close(menu);
    else return 0;
    return 1;
}
int available(void *, const RecompRuntimeUiItem *item) {
    return std::strcmp(item->section, "DLSS5") || vulkan;
}
void save(void *) {
    if (!FzeroVideoSave(settings, path)) std::fprintf(stderr, "[menu] Could not save neural settings\n");
}
}

extern "C" bool FzeroMenuInit(void *w, void *r, FzeroVideoSettings *s,
                              bool *e, bool *c, int *run, const char *p, bool vk) {
    window = static_cast<SDL_Window *>(w); renderer = static_cast<SDL_Renderer *>(r);
    settings = s; enabled = e; compare = c; running = run; path = p; vulkan = vk;
    context = ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImFontConfig font; font.SizePixels = 17;
    ImGui::GetIO().Fonts->AddFontDefault(&font);
#if SNESRECOMP_SDL3
    bool ok = renderer ? ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)
                       : ImGui_ImplSDL3_InitForOpenGL(window, SDL_GL_GetCurrentContext());
    ok = ok && (renderer ? ImGui_ImplSDLRenderer3_Init(renderer) : ImGui_ImplOpenGL3_Init("#version 330 core"));
#else
    bool ok = renderer ? ImGui_ImplSDL2_InitForSDLRenderer(window, renderer)
                       : ImGui_ImplSDL2_InitForOpenGL(window, SDL_GL_GetCurrentContext());
    ok = ok && (renderer ? ImGui_ImplSDLRenderer2_Init(renderer) : ImGui_ImplOpenGL3_Init("#version 330 core"));
#endif
    if (!ok) return false;
    items[0] = {"enabled", "DLSS5", "Enabled", nullptr, RECOMP_RUNTIME_UI_BOOL, 0, 1, 1};
    items[1] = {"compare", "DLSS5", "Split comparison", nullptr, RECOMP_RUNTIME_UI_BOOL, 0, 1, 1};
    for (int i = 0; i < FZERO_DLSS_PARAM_COUNT; ++i) {
        const auto &param = fzero_dlss_params[i];
        auto &item = items[i + 2];
        item = {param.key, "DLSS5", param.label, nullptr, RECOMP_RUNTIME_UI_INT,
                param.minimum, param.maximum, i >= 2 && i <= 5 ? 5 : 1};
        if (i == FZERO_DLSS_STYLE || i == FZERO_DLSS_PRESET) {
            item.type = RECOMP_RUNTIME_UI_CHOICE;
            item.choices = i == FZERO_DLSS_STYLE ? styles : presets;
            item.choice_count = i == FZERO_DLSS_STYLE ? 7 : 4;
        }
        if (i >= FZERO_DLSS_MASK) item.type = RECOMP_RUNTIME_UI_BOOL;
    }
    items[10] = {"defaults", "DLSS5", "Reset defaults", nullptr, RECOMP_RUNTIME_UI_ACTION};
    items[11] = {"resume", "Game", "Resume", nullptr, RECOMP_RUNTIME_UI_ACTION};
    items[12] = {"quit", "Game", "Quit game", nullptr, RECOMP_RUNTIME_UI_ACTION};
    RecompRuntimeUiConfig config{};
    config.title = "F-Zero"; config.subtitle = status; config.theme = "snes";
    config.items = items; config.item_count = 13;
    config.accept_label = "Enter"; config.back_label = "Esc";
    config.callbacks.get_value = get; config.callbacks.set_value = set;
    config.callbacks.run_action = action; config.callbacks.is_enabled = available;
    config.callbacks.save = save;
    menu = recomp_runtime_ui_create(&config);
    return menu != nullptr;
}
extern "C" bool FzeroMenuOpen(void) { return menu && recomp_runtime_ui_is_open(menu); }
extern "C" bool FzeroMenuEvent(const void *ptr) {
    if (!menu) return false;
    auto event = static_cast<const SDL_Event *>(ptr);
#if SNESRECOMP_SDL3
    ImGui_ImplSDL3_ProcessEvent(event);
#else
    ImGui_ImplSDL2_ProcessEvent(event);
#endif
    if (event->type == SDL_KEYDOWN) {
        const auto key = SNESRECOMP_SDL_EVENT_KEY(*event);
        const auto mod = SNESRECOMP_SDL_EVENT_MOD(*event);
        if (std::getenv("FZERO_MENU_TRACE")) std::fprintf(stderr, "[menu] key=%u mod=%u open=%d\n", (unsigned)key, (unsigned)mod, FzeroMenuOpen());
        if (key == SDLK_ESCAPE || ((mod & KMOD_CTRL) && key == SDLK_F10)) {
            if (!event->key.repeat) {
                if (FzeroMenuOpen()) recomp_runtime_ui_close(menu);
                else recomp_runtime_ui_open(menu);
            }
            return true;
        }
        if ((mod & KMOD_CTRL) && (key == SDLK_F8 || key == SDLK_F9)) return false;
        if (FzeroMenuOpen()) {
            RecompRuntimeUiInput input;
            switch (key) {
            case SDLK_UP: input = RECOMP_RUNTIME_UI_INPUT_UP; break;
            case SDLK_DOWN: input = RECOMP_RUNTIME_UI_INPUT_DOWN; break;
            case SDLK_LEFT: input = RECOMP_RUNTIME_UI_INPUT_LEFT; break;
            case SDLK_RIGHT: input = RECOMP_RUNTIME_UI_INPUT_RIGHT; break;
            case SDLK_RETURN: input = RECOMP_RUNTIME_UI_INPUT_ACCEPT; break;
            case SDLK_BACKSPACE: input = RECOMP_RUNTIME_UI_INPUT_BACK; break;
            default: return true;
            }
            recomp_runtime_ui_handle_input(menu, input, 1, event->key.repeat);
            return true;
        }
    }
    return FzeroMenuOpen() && (event->type == SDL_KEYUP || event->type == SDL_TEXTINPUT);
}
extern "C" void FzeroMenuDraw(void) {
    if (!menu) return;
    std::snprintf(status, sizeof(status), "DLSS5: %s", FzeroDlssStatus());
#if SNESRECOMP_SDL3
    if (renderer) ImGui_ImplSDLRenderer3_NewFrame(); else ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
#else
    if (renderer) ImGui_ImplSDLRenderer2_NewFrame(); else ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
#endif
    ImGui::NewFrame();
    recomp_runtime_ui_render_imgui(menu);
    ImGui::Render();
#if SNESRECOMP_SDL3
    if (renderer) ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
#else
    if (renderer) ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
#endif
    else ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
extern "C" void FzeroMenuShutdown(void) {
    if (!context) return;
    if (menu) { recomp_runtime_ui_close(menu); recomp_runtime_ui_destroy(menu); menu = nullptr; }
#if SNESRECOMP_SDL3
    if (renderer) ImGui_ImplSDLRenderer3_Shutdown(); else ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
#else
    if (renderer) ImGui_ImplSDLRenderer2_Shutdown(); else ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
#endif
    ImGui::DestroyContext(context); context = nullptr;
}
