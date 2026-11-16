#include <iostream>

#include "application.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    std::ios_base::sync_with_stdio(false);
    GameApp app("Text Editor Demo");
    return app.exec();
}
