// VoxelArenaMod: makes, checks and changes mods for Voxel Arena. No window, no network: it only
// reads and writes files in the mods folders.
//
//   VoxelArenaMod                               check every mod, and say how to use the tool
//   VoxelArenaMod new <mod> ["Name"]            a new mod in the player's mods folder
//   VoxelArenaMod add <mod> <kind> <name> [id]  a copy of a definition (a weapon, an enemy...) in a mod
//   VoxelArenaMod patch <mod> <id>              a patch of another mod's definition, every value in comments
//   VoxelArenaMod check [mod]                   what is wrong in the mods
//   VoxelArenaMod show <id>                     a definition as the game reads it, after every mod
//   VoxelArenaMod list [kind]                   what there is
//   VoxelArenaMod mods | on <mod> | off <mod>   the mods, and turning them on or off
//
// --mods <dir> reads the game's mods from another folder, as the game's option does.

#include <cctype>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "Core/content.h"
#include "Core/modkit.h"
#include "Core/paths.h"
#include "Core/scriptbrain.h"

#ifndef VOXELARENA_MODS_DIR
    #define VOXELARENA_MODS_DIR nullptr
#endif

namespace fs = std::filesystem;
using namespace arena;

namespace
{
    ModSources sources;

    void help()
    {
        std::cout << "VoxelArenaMod: make, check and change mods for Voxel Arena.\n"
                     "\n"
                     "Your mods are in " << userModsDir() << "\n"
                     "\n"
                     "  VoxelArenaMod new <mod> [\"Name\"]              make a new mod\n"
                     "  VoxelArenaMod add <mod> <kind> <name> [id]    add a copy of something to a mod (the base\n"
                     "                                                game's own when no id is given)\n"
                     "  VoxelArenaMod patch <mod> <id>                change something of another mod, like base:brute\n"
                     "  VoxelArenaMod check [mod]                     find mistakes in the mods\n"
                     "  VoxelArenaMod show <id>                       what something is, after every mod\n"
                     "  VoxelArenaMod list [kind]                     what there is\n"
                     "  VoxelArenaMod mods                            the mods, on and off\n"
                     "  VoxelArenaMod on <mod>, off <mod>             turn a mod on or off\n"
                     "\n"
                     "Kinds: ";

        for (size_t i = 0; i < modkit::kinds().size(); ++i)
            std::cout << (i ? ", " : "") << (std::string(modkit::kinds()[i].name) == "upgrade" ? "item" : modkit::kinds()[i].name);

        std::cout << ".\n"
                     "For example: VoxelArenaMod new mymod, then VoxelArenaMod add mymod weapon halberd base:sword\n"
                     "Then start the game (or press F5 in it) to play with your mod.\n";
    }

    /** The game's content, its enemy scripts compiled too: their mistakes are a mod's. */
    Content load()
    {
        Content content = loadContent(sources);
        ScriptBrain brain;
        brain.load(content, content.report);
        return content;
    }

    /** A path in a mods folder, from that folder: mymod/weapons/halberd.json. */
    std::string shortPath(const std::string& path)
    {
        for (const auto& dir : sources.dirs)
        {
            std::error_code ec;
            const auto rel = fs::relative(path, dir, ec);

            if (not ec and not rel.empty() and rel.generic_string().compare(0, 2, "..") != 0)
                return rel.generic_string();
        }

        return path;
    }

    std::string said(const ContentMessage& m)
    {
        return shortPath(m.file) + (m.line > 0 ? ":" + std::to_string(m.line) : "") + ": " + m.text;
    }

    /** A loaded mod by id; nullptr, and why, when it is not. */
    const ModInfo* modNamed(const Content& content, const std::string& id)
    {
        if (const ModInfo* m = content.mod(id))
            return m;

        for (const auto& m : content.modsOff)
            if (m.id == id)
            {
                std::cout << "The mod " << id << " is turned off: turn it on first (VoxelArenaMod on " << id << ").\n";
                return nullptr;
            }

        std::cout << "There is no mod " << id << ". VoxelArenaMod mods lists them.\n";
        return nullptr;
    }

    /** What the tool did, as a sentence; then, when it worked, what to do next. */
    int report(const modkit::Done& done, const std::string& next = "")
    {
        std::string message = done.message;

        if (done.ok and not message.empty())
            message[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(message[0])));

        std::cout << (done.ok ? "" : "Could not do it: ") << message << ".\n";

        if (done.ok and not next.empty())
            std::cout << next << "\n";

        return done.ok ? 0 : 1;
    }

    int check(const std::string& id)
    {
        const Content content = load();
        const ModInfo* mod = nullptr;

        if (not id.empty() and not (mod = modNamed(content, id)))
            return 1;

        const auto problems = modkit::check(content, mod);

        for (const auto& e : problems.errors)
            std::cout << "mistake: " << said(e) << "\n";

        for (const auto& w : problems.warnings)
            std::cout << "warning: " << said(w) << "\n";

        const std::string what = mod ? "The mod " + mod->id : "The mods (" + std::to_string(content.mods.size()) + ")";

        if (problems.errors.empty() and problems.warnings.empty())
            std::cout << what << ": no mistakes found.\n";
        else
            std::cout << what << ": " << problems.errors.size() << (problems.errors.size() == 1 ? " mistake, " : " mistakes, ") << problems.warnings.size()
                      << (problems.warnings.size() == 1 ? " warning" : " warnings") << ". A mistake leaves that part out of the game; a warning is"
                      << " something that does nothing or looks wrong.\n";

        return problems.errors.empty() ? 0 : 1;
    }

    int show(const std::string& id)
    {
        const Content content = load();
        const auto found = modkit::find(content, id);

        if (not found.def)
        {
            std::cout << (found.others.empty() ? "Nothing is called " + id + ". VoxelArenaMod list shows what there is.\n" : id + " could be:\n");
            for (const auto& o : found.others)
                std::cout << "  " << o << "\n";
            return 1;
        }

        const RawDef& def = *found.def;
        std::cout << def.id << ", " << (found.kind->name == std::string("upgrade") ? "a shop item" : std::string("a ") + found.kind->name) << " of the mod " << def.mod
                  << " (" << shortPath(def.file) << ")\n";

        for (const auto& by : def.patchedBy)
            std::cout << "patched by " << by << "\n";

        std::cout << modkit::pretty(def.data) << "\n";
        return 0;
    }

    int list(const std::string& kindName)
    {
        const Content content = load();

        if (kindName.empty())
        {
            for (const auto& k : modkit::kinds())
            {
                const auto defs = content.raw.find(k.raw);
                std::cout << "  " << (k.name == std::string("upgrade") ? "item" : k.name) << ": " << (defs == content.raw.end() ? 0 : defs->second.size()) << "\n";
            }

            std::cout << "VoxelArenaMod list <kind> names them.\n";
            return 0;
        }

        const modkit::Kind* kind = modkit::findKind(kindName);

        if (not kind)
        {
            std::cout << "There is no kind " << kindName << ".\n";
            return 2;
        }

        if (const auto defs = content.raw.find(kind->raw); defs != content.raw.end())
            for (const auto& [id, def] : defs->second)
                std::cout << "  " << id << (def.data.find("name") ? "  " + def.data.str("name", "") : "") << "\n";

        return 0;
    }

    int mods()
    {
        const Content content = load();

        for (const auto& m : content.mods)
            std::cout << "  on   " << m.id << " " << m.version << "  " << m.name << "  (" << m.dir << ")\n";

        for (const auto& m : content.modsOff)
            std::cout << "  off  " << m.id << " " << m.version << "  " << m.name << "  (" << m.dir << ")\n";

        std::cout << "Your mods go in " << userModsDir() << "\n";
        return 0;
    }
}

int main(int argc, char* argv[])
{
    std::string gameMods;
    std::vector<std::string> args;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];

        if (a == "--mods" and i + 1 < argc)
            gameMods = argv[++i];
        else
            args.push_back(a);
    }

    sources = playerModSources(gameMods, VOXELARENA_MODS_DIR);

    auto need = [&](size_t count, const char* usage) {
        if (args.size() >= count)
            return true;
        std::cout << "Use: VoxelArenaMod " << usage << "\n";
        return false;
    };

    const std::string command = args.empty() ? "" : args[0];

    if (command.empty())
    {
        help();
        std::cout << "\n";
        const int result = check("");

#ifdef _WIN32
        // Opened with a double click: the window would close before anything is read
        std::cout << "\nPress Enter to close.";
        std::cin.get();
#endif

        return result;
    }

    if (command == "help" or command == "--help" or command == "-h")
    {
        help();
        return 0;
    }

    if (command == "new")
    {
        if (not need(2, "new <mod> [\"Name\"]"))
            return 2;

        return report(modkit::newMod(userModsDir(), args[1], args.size() > 2 ? args[2] : ""),
                      "Next: VoxelArenaMod add " + args[1] + " weapon <name> (or enemy, spell, item...) to start from a copy.");
    }

    if (command == "add")
    {
        if (not need(4, "add <mod> <kind> <name> [id to copy]"))
            return 2;

        const modkit::Kind* kind = modkit::findKind(args[2]);

        if (not kind)
        {
            std::cout << "There is no kind " << args[2] << ". VoxelArenaMod help lists them.\n";
            return 2;
        }

        const Content content = load();
        const ModInfo* mod = modNamed(content, args[1]);

        if (not mod)
            return 1;

        return report(modkit::addDefinition(content, *mod, *kind, args[3], args.size() > 4 ? args[4] : ""),
                      "Change it as you like, then VoxelArenaMod check " + mod->id + " finds mistakes.");
    }

    if (command == "patch")
    {
        if (not need(3, "patch <mod> <id>"))
            return 2;

        const Content content = load();
        const ModInfo* mod = modNamed(content, args[1]);
        return mod ? report(modkit::writePatch(content, *mod, args[2]), "Remove the // in front of what you want to change.") : 1;
    }

    if (command == "check")
        return check(args.size() > 1 ? args[1] : "");

    if (command == "show")
        return need(2, "show <id>") ? show(args[1]) : 2;

    if (command == "list")
        return list(args.size() > 1 ? args[1] : "");

    if (command == "mods")
        return mods();

    if (command == "on" or command == "off")
    {
        if (not need(2, "on <mod>, or off <mod>"))
            return 2;

        const Content content = load();
        bool known = false;

        for (const auto* list : {&content.mods, &content.modsOff})
            for (const auto& m : *list)
                known = known or m.id == args[1];

        if (not known)
        {
            std::cout << "There is no mod " << args[1] << ". VoxelArenaMod mods lists them.\n";
            return 1;
        }

        return report(modkit::setModOn(sources.settings, args[1], command == "on"));
    }

    std::cout << "There is no command " << command << ".\n\n";
    help();
    return 2;
}
