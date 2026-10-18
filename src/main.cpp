// SPDX-License-Identifier: GPL-3.0-or-later
#include <ondra/options.hpp>
#include <ondra/files.hpp>
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
void sdlCheck(bool ok, const char* operation) {
    if (!ok) throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}
// Rebuild the entire matrix each event: releasing one Shift/Alt/Ctrl does not
// release the other one, and losing focus cannot leave an emulated key stuck.
void keyboard(ondra::Machine& m) {
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    m.releaseKeys();
    struct Mapping { SDL_Scancode code; unsigned row, bit; };
    const Mapping map[] = {
        {SDL_SCANCODE_Q,0,4},{SDL_SCANCODE_T,0,3},{SDL_SCANCODE_W,0,2},{SDL_SCANCODE_E,0,1},{SDL_SCANCODE_R,0,0},
        {SDL_SCANCODE_A,1,4},{SDL_SCANCODE_G,1,3},{SDL_SCANCODE_S,1,2},{SDL_SCANCODE_D,1,1},{SDL_SCANCODE_F,1,0},
        {SDL_SCANCODE_LALT,2,4},{SDL_SCANCODE_RALT,2,4},{SDL_SCANCODE_V,2,3},{SDL_SCANCODE_Z,2,2},{SDL_SCANCODE_X,2,1},{SDL_SCANCODE_C,2,0},
        {SDL_SCANCODE_SPACE,3,0},{SDL_SCANCODE_LSHIFT,4,4},{SDL_SCANCODE_RSHIFT,4,4},{SDL_SCANCODE_TAB,4,1},{SDL_SCANCODE_EQUALS,4,2},
        {SDL_SCANCODE_RETURN,5,4},{SDL_SCANCODE_KP_ENTER,5,4},{SDL_SCANCODE_H,5,3},{SDL_SCANCODE_L,5,2},{SDL_SCANCODE_K,5,1},{SDL_SCANCODE_J,5,0},
        {SDL_SCANCODE_P,6,4},{SDL_SCANCODE_Y,6,3},{SDL_SCANCODE_O,6,2},{SDL_SCANCODE_I,6,1},{SDL_SCANCODE_U,6,0},
        {SDL_SCANCODE_LCTRL,7,4},{SDL_SCANCODE_RCTRL,7,4},{SDL_SCANCODE_B,7,3},{SDL_SCANCODE_UP,7,2},{SDL_SCANCODE_M,7,1},{SDL_SCANCODE_N,7,0},
        {SDL_SCANCODE_RIGHT,8,4},{SDL_SCANCODE_DOWN,8,2},{SDL_SCANCODE_LEFT,8,1},
        {SDL_SCANCODE_KP_0,9,4},{SDL_SCANCODE_KP_2,9,3},{SDL_SCANCODE_KP_8,9,2},{SDL_SCANCODE_KP_4,9,1},{SDL_SCANCODE_KP_6,9,0}
    };
    for (const Mapping& k : map) if (keys[k.code]) m.key(k.row, k.bit, true);
}
struct Sdl {
    Sdl() { SDL_SetMainReady(); sdlCheck(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) == 0, "SDL_Init"); }
    ~Sdl() { SDL_Quit(); }
};
struct Audio {
    SDL_AudioDeviceID id;
    explicit Audio(bool enabled) : id(0) {
        if (!enabled) return;
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { std::cerr << "Audio: " << SDL_GetError() << '\n'; return; }
        SDL_AudioSpec wanted = {}, obtained = {};
        wanted.freq = 44100; wanted.format = AUDIO_S16SYS; wanted.channels = 1; wanted.samples = 1024;
        id = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
        if (!id) std::cerr << "Audio: " << SDL_GetError() << '\n';
        else SDL_PauseAudioDevice(id, 0);
    }
    ~Audio() { if (id) SDL_CloseAudioDevice(id); }
};
void dropFile(ondra::Machine& m, const std::string& path) {
    const std::string ext = ondra::extension(path);
    if (ext == ".osn") m.loadSnapshot(path);
    else if (ext == ".bin") m.loadBinary(path);
    else m.tape().load(path);
}
}
int main(int argc, char** argv) {
    try {
        const ondra::Options o = ondra::parseOptions(argc, argv);
        if (o.help) { ondra::printHelp(); return 0; }
        if (o.debug) throw std::runtime_error("Use ondra-headless --debug for the terminal debugger");
        ondra::Machine machine; ondra::configure(machine, o);
        Sdl sdl;
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
            SDL_CreateWindow("Ondra SPO 186", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                320 * o.scale, 256 * o.scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI), SDL_DestroyWindow);
        sdlCheck(window.get() != NULL, "SDL_CreateWindow");
        SDL_SetWindowMinimumSize(window.get(), 320, 256);
        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
            SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED), SDL_DestroyRenderer);
        if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
        sdlCheck(renderer.get() != NULL, "SDL_CreateRenderer");
        sdlCheck(SDL_RenderSetLogicalSize(renderer.get(), 320, 256) == 0, "SDL_RenderSetLogicalSize");
        std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture(
            SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 256), SDL_DestroyTexture);
        sdlCheck(texture.get() != NULL, "SDL_CreateTexture");
        Audio audio(!o.mute);
        bool quit = false, paused = false, full = false, focused = true;
        unsigned frames = 0;
        auto deadline = std::chrono::steady_clock::now();
        ondra::printHelp();
        while (!quit && (!o.frames || frames < o.frames)) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) quit = true;
                if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) { machine.releaseKeys(); focused = false; }
                if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) focused = true;
                if ((e.type == SDL_KEYUP || e.type == SDL_KEYDOWN) && focused) keyboard(machine);
                if (e.type == SDL_DROPFILE) {
                    const std::string path(e.drop.file); SDL_free(e.drop.file);
                    try { dropFile(machine, path); }
                    catch (const std::exception& error) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cannot load file", error.what(), window.get()); }
                }
                if (e.type != SDL_KEYDOWN || e.key.repeat) continue;
                try {
                    switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_F1: ondra::printHelp(); break;
                    case SDL_SCANCODE_F2: paused = !paused; break;
                    case SDL_SCANCODE_F3: paused = true; machine.step(); ondra::printRegisters(machine); break;
                    case SDL_SCANCODE_F4: machine.nmi(); break;
                    case SDL_SCANCODE_F5: machine.reset((e.key.keysym.mod & KMOD_SHIFT) != 0); break;
                    case SDL_SCANCODE_F6: machine.setTurbo(!machine.turbo()); break;
                    case SDL_SCANCODE_F8: machine.saveSnapshot("ondra.osn"); break;
                    case SDL_SCANCODE_F9: machine.loadSnapshot("ondra.osn"); break;
                    case SDL_SCANCODE_F10: quit = true; break;
                    case SDL_SCANCODE_F11: full = !full; sdlCheck(SDL_SetWindowFullscreen(window.get(), full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0, "Fullscreen"); break;
                    case SDL_SCANCODE_F12: machine.screenshot("screenshot.ppm"); break;
                    case SDL_SCANCODE_HOME: machine.tape().rewind(); break;
                    default: break;
                    }
                } catch (const std::exception& error) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ondra", error.what(), window.get()); }
            }
            if (quit) break;
            if (!paused) {
                if (frames == o.loadAfter) ondra::loadProgram(machine, o);
                if (!machine.runFrame()) paused = true;
                else ++frames;
                if (audio.id) {
                    const auto& samples = machine.sound().finishFrame();
                    if (SDL_GetQueuedAudioSize(audio.id) > 44100 * sizeof(std::int16_t) / 5) SDL_ClearQueuedAudio(audio.id);
                    sdlCheck(SDL_QueueAudio(audio.id, samples.data(), static_cast<Uint32>(samples.size() * sizeof(std::int16_t))) == 0, "SDL_QueueAudio");
                }
            } else if (audio.id) SDL_ClearQueuedAudio(audio.id);
            const auto& pixels = machine.pixels();
            sdlCheck(SDL_UpdateTexture(texture.get(), NULL, pixels.data(), 320 * sizeof(std::uint32_t)) == 0, "SDL_UpdateTexture");
            SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
            SDL_RenderClear(renderer.get()); SDL_RenderCopy(renderer.get(), texture.get(), NULL, NULL); SDL_RenderPresent(renderer.get());
            const std::string title = std::string("Ondra SPO 186 | ") + (paused ? "paused" : "running") + (machine.turbo() ? " | 4 MHz" : " | 2 MHz")
                + ((machine.portA0() & 1) ? "" : " | GREEN") + ((machine.portA0() & 2) ? "" : " | YELLOW")
                + (machine.tape().motor() ? " | TAPE" : "");
            SDL_SetWindowTitle(window.get(), title.c_str());
            deadline += std::chrono::milliseconds(20);
            const auto now = std::chrono::steady_clock::now();
            if (deadline < now - std::chrono::milliseconds(100)) deadline = now;
            std::this_thread::sleep_until(deadline);
        }
        ondra::finish(machine, o);
        if (o.checkBoot && (!machine.videoEnabled() || std::count(machine.pixels().begin(), machine.pixels().end(), 0xffffffffu) == 0))
            throw std::runtime_error("ROM boot check failed");
        return 0;
    } catch (const std::exception& e) { std::cerr << "ondra: " << e.what() << '\n'; return 1; }
}
