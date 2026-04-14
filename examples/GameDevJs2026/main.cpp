#include <iostream>

#include "application.h"

int main(int argc, char *argv[])
{
    std::ios_base::sync_with_stdio(false);

    GameApp app("GameDevJs2026");

    return app.exec();
}
