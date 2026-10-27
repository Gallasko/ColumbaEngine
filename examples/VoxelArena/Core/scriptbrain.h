#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "content.h"
#include "world.h"

namespace pg
{
    struct VM;
    struct ObjFunction;
}

namespace arena
{
    /**
     * Enemy behaviour written in pgscript.
     *
     * Each script gets its own sandboxed VM: math, algorithm, string and random, plus print.
     * No file, ecs or engine module, so a mod script cannot touch the disk or the world.
     *
     * A script defines `fun think(me)`. Before each call the game fills `me` with what the
     * enemy sees (see docs in mods/base/scripts/brute.pg); the script answers by setting
     * me.move, me.attack, me.strafe, me.speed and me.face. Any other field it sets on `me`
     * is kept for the next call, so a script can remember things per enemy.
     */
    class ScriptBrain : public Brain
    {
    public:
        ScriptBrain();
        ~ScriptBrain() override;

        /** Compiles every script of the content. Errors go to report; a broken script leaves its enemies standing still. */
        void load(const Content& content, ContentReport& report);

        bool think(const std::string& scriptId, const BrainView& view, BrainOrder& order, std::string& error) override;

        /** Calls made and time spent in scripts since the last reset, for the debug overlay. */
        struct Stats
        {
            uint64_t calls = 0;
            double seconds = 0.0;
        };
        Stats stats;

        /** Forgets the per-enemy tables of fighters that no longer exist. */
        void forget(const std::vector<int>& liveIds);

        /** Where compiled bytecode is written while compiling. */
        std::string cacheDir;

    private:
        struct Script;

        std::map<std::string, std::unique_ptr<Script>> scripts;
    };
}
