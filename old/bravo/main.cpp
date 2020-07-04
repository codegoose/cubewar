#define CG_EXPOSE_BOOT
#define CG_EXPOSE_HANDLES
#define CG_EXPOSE_GL
#define CG_EXPOSE_SDL
#define CG_EXPOSE_CFG

#include <subsystem.h>
#include <shadernaut.h>

#include <iostream>

void on_startup() {
    std::cout << "STARTUP" << std::endl;
}

void on_shutdown() {
    std::cout << "SHUTDOWN" << std::endl;
}

void on_update(double delta) {
    glClearColor(0.2, 0.2, 0.2, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(goose::sys::sdl_window);
}

int main() {
    goose::sys::on_startup = on_startup;
    goose::sys::on_update = on_update;
    goose::sys::on_shutdown = on_shutdown;
    if (!goose::sys::boot("Bravo", "bravo")) return 0;
    std::atexit(goose::sys::kill);
    auto programs = shadernaut::launch("glsl/");
    while (goose::sys::update());
}