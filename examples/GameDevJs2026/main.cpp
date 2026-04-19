#include <iostream>

#include "application.h"

int main(int argc, char *argv[])
{
    std::ios_base::sync_with_stdio(false);

    // static: must survive Emscripten's emscripten_set_main_loop stack unwinding
    static GameApp app("GameDevJs2026");

    return app.exec();
}
