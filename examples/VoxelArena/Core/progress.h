#pragma once

#include "content.h"
#include "world.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace arena
{
    /**
     * A player's progress from run to run: what they did in all their runs (the stats the goals count, see
     * AchievementDef::stat) and the goals reached. Each player has their own, on their own machine: in co-op,
     * each one's runs count for them, and each one's locks hold for them only.
     */
    struct Progress
    {
        std::map<std::string, float> stats;
        std::set<std::string> done;       // the goals reached (achievement ids)
        bool unlockAll = false;           // the console's "unlock all": nothing locked, the goals left as they are
    };

    /** Where it is kept: <userDataDir>/progress.json */
    std::string progressPath();

    bool loadProgress(const std::string& path, Progress& out);
    bool saveProgress(const std::string& path, const Progress& p);

    /**
     * Adds a player's run to the stats: what its fighter did (Fighter::stats), the waves cleared, and the win
     * (won: a Victory) with its weapon and its level.
     */
    void countRun(Progress& p, const Fighter& player, const RunState& run, bool won);

    /** The goals just reached by the stats (and now marked done), in the screen's order. */
    std::vector<std::string> reachGoals(Progress& p, const Content& content);

    /** What is still locked: everything a goal unlocks, when none of the goals unlocking it is reached. */
    std::set<std::string> lockedContent(const Progress& p, const Content& content);

    /** How far a goal is: its stat now (not above its goal). */
    float goalProgress(const Progress& p, const AchievementDef& a);
}
