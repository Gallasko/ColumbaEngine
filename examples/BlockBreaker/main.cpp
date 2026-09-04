#include <iostream>

#include "application.h"

int main(int argc, char *argv[])
{
    // Decouple C++ and C stream for faster runtime
    std::ios_base::sync_with_stdio(false);

    // The game script can be overridden from the command line; the default
    // path assumes the executable is run from the repository root.
    std::string scriptPath = "examples/BlockBreaker/main.pg";

    if (argc > 1)
    {
        scriptPath = argv[1];
    }

    GameApp app("Block Breaker", scriptPath);

    return app.exec();
}
