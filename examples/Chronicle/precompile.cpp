#include <cstdio>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"

#include "rulevm.h"

// Chronicle's rules compiled as the game compiles them, for a build that ships the .pgc instead
// of the sources (the web build, given this tool's path in CHRONICLE_RULES_COMPILER):
//
//   ChronicleRules <rules dir> <out dir> [--no-analysis]
//
// No window and no scene: the EntitySystem is there for its script registry, as in the tests.
int main(int argc, char* argv[])
{
    if (argc < 3 or argc > 4 or (argc == 4 and std::string(argv[3]) != "--no-analysis"))
    {
        std::fprintf(stderr, "usage: ChronicleRules <rules dir> <out dir> [--no-analysis]\n");
        return 2;
    }

    pg::EntitySystem ecs;

    // O3, with the whole-program analysis, is what the shipped rules are compiled with; off only for timing
    ecs.setVMOptimizationLevel(argc == 3 ? pg::VmOptimizationLevel::O3 : pg::VmOptimizationLevel::O2);
    std::vector<std::string> errors;

    if (not chronicle::precompileRules(&ecs, argv[1], argv[2], errors))
    {
        for (const auto& e : errors)
            std::fprintf(stderr, "%s\n", e.c_str());

        return 1;
    }

    return 0;
}
