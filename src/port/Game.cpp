#include <libultraship.h>

#include <fast/interpreter.h>
#include "Engine.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/emscripten.h>
#include "web/WebUtils.h"
static constexpr const char* kWebPersistPath = "/idbfs";
#endif

extern "C" {
#include "audio/external.h"
#include "game/game_init.h"
#include "sm64.h"
}

void alloc_pool() {
    static u64 pool[1024 * 1024 * 4];
    main_pool_init(pool, pool + sizeof(pool) / sizeof(pool[0]));
    gEffectsMemoryPool = mem_pool_init(0x4000, MEMORY_POOL_LEFT);
}

extern "C" void exec_display_list(SPTask* spTask) {
    GameEngine::ProcessGfxCommands((Gfx*)spTask->task.t.data_ptr);
}

void push_frame() {
    GameEngine::StartAudioFrame();
    GameEngine::Instance->StartFrame();
    thread5_iteration();
    GameEngine::EndAudioFrame();
}

#ifdef _WIN32
int SDL_main(int argc, char** argv) {
#else
int main(int argc, char* argv[]) {
#endif

#ifdef __EMSCRIPTEN__
    WebCache_Mount(kWebPersistPath);
    WebCache_Load();
#endif

    GameEngine::Create(argc, argv);
    alloc_pool();
    audio_init();
    sound_init();
    thread5_game_loop();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(push_frame, 0, 1);
#else
    while (WindowIsRunning()) {
        push_frame();
    }
    GameEngine::Instance->Destroy();
#endif

    return 0;
}