#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "UI/eventlog.h"   // LogEntry

namespace chronicle
{
    // A row the ledger shows: a resource the character holds, or a skill.
    struct LifeResource
    {
        std::string group;             // "purse"
        std::string groupLabel;        // "PURSE" ("" for the skills, which have no heading)
        std::string id;                // "coin"; when it names a stat, the value is the stat's
        std::string glyph;
        std::string name;
        std::string value;             // As shown, for a resource that is not a stat
        std::string rate;
        int tone = 0;                  // LedgerTone
        bool muted = false;
    };

    // Everything a life keeps between sessions: who, how old, what he is, what he does, what he
    // holds and what has happened. Nothing from the rule scripts is saved: they are run again.
    struct LifeSave
    {
        std::string name;
        std::string profession;
        std::string origin;
        std::string aim;               // The path he is headed for ("squire"): the stat lines' thresholds
        float age = 7.0f;
        std::string running;           // The activity at work, "" for none
        int monthsIn = 0;              // Months already spent in it
        std::unordered_map<std::string, int> stats;   // Parts, skills, flags and coin, by key
        std::vector<std::string> parts;                // The stat keys shown as parts ("str", ...)
        std::vector<LifeResource> skills;              // The skills, in the order shown
        std::vector<LifeResource> resources;           // The ledger, in the order first earned
        std::vector<LogEntry> log;

        // The character as the rule scripts read it: every stat.
        pg::ElementMap character() const;

        bool save(const std::string& path) const;
        bool load(const std::string& path);
    };

    // The design mockup's life: Aldren of Bellmoor at 17.5, the page of the Main Life screen.
    LifeSave firstLife();

    // The true first frame of the game: 7 years old, nothing earned, one line in the log.
    LifeSave freshLife();
}
