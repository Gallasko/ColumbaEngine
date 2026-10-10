#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "UI/eventlog.h"   // LogEntry
#include "UI/prefabspec.h" // RecordList

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
        std::string rate;              // For a resource the rules do not follow; theirs wins
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
        std::string aim;               // The path he is headed for ("warrior"), set when he enters one: the stat lines' thresholds
        float age = 7.0f;
        int world = 0;                 // Months since the world's Year 0. It runs on from life to life: a death does not rewind it
        int lives = 1;                 // The lives lived in this world, this one counted. A save from before the count reads 1

        // The town is the world's too: what a life gave Bellmoor is kept when it ends, and the next
        // one is born into it. A save from before the town reads an unknown town
        bool townKnown = false;        // A life has explored the town: its page is open, for every later life as well
        std::unordered_map<std::string, int> town;              // Place id -> its level; a place not raised yet has no key
        std::unordered_map<std::string, std::string> raisedBy;  // Place id -> the life that raised it last and the world's year then: his name, a middle dot, "YEAR 4"
        std::unordered_map<std::string, int> fund;              // Place id -> the coin lives left to it at their death, counted toward its next level
        std::vector<std::string> told;                          // The guide's steps that are said once in a world (achievements.pg's `world`), by id: said, in this life or one before
        std::vector<std::string> seen;                          // The places a life has had on its Town page: the page grows with age and class, and what it has shown it keeps showing

        std::string running;           // The activity at work, "" for none
        int monthsIn = 0;              // Months already spent in it
        std::unordered_map<std::string, int> stats;   // Parts, skills, flags and coin, by key
        std::vector<std::string> parts;                // The stat keys shown as parts ("str", ...)
        std::vector<LifeResource> skills;              // The skills, in the order shown
        std::vector<LifeResource> resources;           // The ledger, in the order first earned
        std::vector<LogEntry> log;
        std::unordered_map<std::string, int> done;    // The terms completed, by activity
        int picks = 0;                 // What the player did in this life, for the analytics: the tiles he chose in the list,
        int begun = 0;                 // the works he began,
        int atOnce = 0;                // the things he did on the spot,
        int skips = 0;                 // and the months he passed by the button, at nothing
        int guide = 0;                 // The last step of a first life's guide that ended (achievements.pg's `order`), 0 before the first
        int guideSkipped = 0;          // The step being said when the player skipped the rest of the guide, 0 when he did not
        std::vector<std::string> achieved;             // The deeds already reached, by id

        // The character as the rule scripts read it: every stat, and the town with them, which is
        // not his but which the rules read the same way: "town_known" (1 or 0), "town.<place>" its
        // level, "fund.<place>" the coin left to it and "seen.<place>" (1) for a place already
        // on the Town page.
        pg::ElementMap character() const;

        // Whether a key of what a script hands back is the town's and not a stat of his.
        static bool ofTheTown(const std::string& key);

        // A key of what a script hands back, when it is the town's: true, and it is not to be
        // written among his stats. Coming to know the town is the one thing a script does to it
        // this way (`townKnown`); its levels and its funds are the Life screen's to write.
        bool takeTown(const std::string& key, int value);

        // The levels of every place of the town, added up.
        int townLevels() const;

        // The terms completed as the rule scripts read them.
        pg::ElementMap terms() const;

        // How many terms he has done in this life, of every activity.
        int termsDone() const;

        // What he holds for the first time: every holding of the rules (resources.pg's `rows`)
        // he has any of and the ledger does not show yet. Added to `resources`, and returned.
        std::vector<LifeResource> holdEarned(const pg::RecordList& rows);

        // Where the life stands, in one short line of JSON for the analytics: his age, the world's
        // month, which life this is, his path, the work at hand, how much he has done, what the
        // player pressed to get there, and the step of the guide he is past. Nothing of his name.
        std::string digest() const;

        bool save(const std::string& path) const;
        bool load(const std::string& path);
    };

    // The design mockup's life: Aldren of Bellmoor at 17.5, the page of the Main Life screen, as
    // the balance's good Warrior stands the month he swears to the Keep.
    LifeSave firstLife();

    // A life at its first frame. A later life (`first` false): 7 years old, a year of rations and
    // nothing earned, one line in the log. The first life of a world: a month before 7 with nothing
    // held at all, so that its first task, a month long, brings him to 7 and his first rations.
    LifeSave freshLife(bool first = false);
}
