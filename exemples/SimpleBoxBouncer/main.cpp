#include <iostream>

#include "application.h"

//[TODO] Variant using operator* dereferencing to recast to the original type
int main(int argc, char *argv[])
{
    // Decouple C++ and C stream for faster runtime
    std::ios_base::sync_with_stdio(false);
    // auto fileSink = pg::Logger::registerSink<pg::FileSink>();

    GameApp app("Simple app");

    return app.exec();
}


