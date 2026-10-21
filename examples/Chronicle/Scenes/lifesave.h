#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "UI/eventlog.h"   // LogEntry
#include "UI/prefabspec.h" // RecordList
#include "Core/walk.h"     // WalkState, WalkRecipe, WalkQueue
#include "Core/settings.h"

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
        size_t savedAt = 0;            // The wall clock it was last written at, in seconds since 1970 (a size_t: the serializer's one wide integer on every platform): what was written while the chronicle stayed shut is counted from it. A life never written, or a save from before, reads 0

        // The town is the world's too: what a life gave Bellmoor is kept when it ends, and the next
        // one is born into it. A save from before the town reads an unknown town
        bool townKnown = false;        // A life has explored the town: its page is open, for every later life as well
        std::unordered_map<std::string, int> town;              // Place id -> its level; a place not raised yet has no key
        std::unordered_map<std::string, std::string> raisedBy;  // Place id -> the life that raised it last and the world's year then: his name, a middle dot, "YEAR 4"
        std::unordered_map<std::string, int> fund;              // Place id -> the coin lives left to it at their death, counted toward its next level; "<place>.<thing>" -> what they left it of a thing its next level asks (firewood, stone)
        std::vector<std::string> told;                          // The guide's steps that are said once in a world (achievements.pg's `world`), by id: said, in this life or one before
        std::vector<std::string> seen;                          // The places a life has had on its Town page: the page grows with age and class, and what it has shown it keeps showing

        // What walks outside the town have taught is the world's as well: a zone studied, a beast
        // watched or fought, what it was seen to drop. A save from before the walks reads none
        std::unordered_map<std::string, int> knowledge;         // "visit.<zone>", "study.<zone>", "observe.<mob>", "fights.<mob>", "drop.<mob>.<item>", "low.<mob>", "high.<mob>"

        // The stash is the world's as well: what a life held of the basics at its death (berries,
        // water, firewood, stone, clay), up to the most the town keeps of each, for the lives after
        // it to draw. A save from before the stash reads an empty one
        std::unordered_map<std::string, int> stash;             // Basic resource id -> what is kept of it

        // His walks are his own: the one he is on, the records he keeps of those he made by hand,
        // and the queue that walks one of them again. A new life has none
        WalkState walk;                // The walk under way: he is on site, its screen is the page
        std::vector<WalkRecipe> recipes;                        // The records kept, in the order they were walked
        WalkQueue queue;               // The form of the Adventure page, and the queue running
        std::string walking;           // The record being walked again while its months run, "" for a walk made by hand or none
        bool lastWalk = false;         // The queue stops once the walk under way is home
        int recorded = 0;              // The records made in this life, a replaced one counted: the next one's id
        int walksByHand = 0;           // What the player did in this life, for the analytics: the walks he made himself,
        int walksByRecord = 0;         // and the ones a record made for him
        std::unordered_map<std::string, int> walkedAt;          // The same by place: zone -> the walks he made there himself,
        std::unordered_map<std::string, int> replayedAt;        // and the ones a record made
        int carried = 0;               // The walks he was carried home from
        int queued = 0;                // The longest queue he set going
        int fights = 0;                // The fights he came to, a walk by a record's not counted: no fight is played there,
        int fightsWon = 0;             // how they ended,
        int fightsLost = 0;
        int fightsFled = 0;
        int fightsAuto = 0;            // and the ones that ended in the hands of his stance

        // What a point of Vitality is worth in this save: 5, the points a fight counts in (a boy
        // has 40). A save written before had 1 (a boy had 8): it is brought to 5 when it is read
        static constexpr int VitalityPoints = 5;
        int vitalityIn = VitalityPoints;

        std::string running;           // The activity at work, "" for none
        int monthsIn = 0;              // Months already spent in it
        std::string setAside;          // The work he left to answer an event of his life, "" for none: it goes on once the answer is done
        int setAsideMonths = 0;        // The months it had run
        bool boardUpFront = true;      // A work that feeds him gives its months' rations as he begins it. A save from before read false: the work under way never gave them, and is given them once on load
        bool coursesTaught = true;     // The Collegium's spells come from its courses (its term is a story). A save from before read false: a scholar of it learned Firebolt with the class, and is given the fire course on load
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
        // on the Town page. And, the same way, what the world knows ("know.<key>") and what the
        // stash keeps of each basic ("stash.<id>").
        pg::ElementMap character() const;

        // Whether a key of what a script hands back is the world's and not a stat of his: the
        // town's, what is known of the lands outside it ("know.<key>"), or what the stash keeps
        // ("stash.<id>").
        static bool ofTheTown(const std::string& key);

        // A key of what a script hands back, when it is the world's: true, and it is not to be
        // written among his stats. Coming to know the town is the one thing a script does to the
        // town this way (`townKnown`); its levels and its funds are the Life screen's to write.
        // What is known is the scripts' to write: a walk teaches. So is what the stash keeps: a draw
        // takes from it.
        bool takeTown(const std::string& key, int value);

        // The levels of every place of the town, added up.
        int townLevels() const;

        // The terms completed as the rule scripts read them.
        pg::ElementMap terms() const;

        // How many terms he has done in this life, of every activity.
        int termsDone() const;

        // What he holds for the first time: every holding of the rules (resources.pg's `rows`)
        // he has any of, or that they show from the first (`always`: held at 0 then), and the
        // ledger does not show yet. Added to `resources`, and returned.
        std::vector<LifeResource> holdEarned(const pg::RecordList& rows);

        // Where the life stands, in one short line of JSON for the analytics: his age, the world's
        // month, which life this is, his path, the work at hand, how much he has done, what the
        // player pressed to get there, the step of the guide he is past, his walks and his fights.
        // `named` are the works the analytics follow one by one (the scene knows them from the
        // rules: his sixth year's and the end of a life's): each one he has done is written with
        // how often, "did.<id>". "v" is the build the digest is of. Nothing of his name.
        std::string digest(const std::vector<std::string>& named = {}) const;

        // The build a digest says it is of: changed with what a digest means (its guide's steps,
        // the works it names), so that two builds' rows can be told apart
        static constexpr const char* DigestBuild = "0.8";

        // The file holds the life, and with `settings` the player's settings beside it (the game's
        // own save: one file for everything). Without, the life alone (an export). Whatever else the
        // file held is gone: the two objects are written from memory, never carried over from it
        bool save(const std::string& path, const Settings* settings = nullptr) const;
        bool load(const std::string& path);

        // The version of the save's own format, written with every save. A save read from a file
        // keeps the one it was written with in `version`: 0 for a save from before the field.
        // 2: the guide has the outskirts' steps, and `guide` counts them (the scene renumbers a
        // save of 1 or 0)
        static constexpr int SaveVersion = 2;
        int version = SaveVersion;

        // What a file offered as a save is (the Options page's Load): one of this build, or what
        // stands in the way. A save from before the version field is not taken from outside the game
        enum class Opened { Ok, NotASave, Newer };

        // Reads `path` into `out` without touching anything else; `out` is left as it was unless Ok
        static Opened open(const std::string& path, LifeSave& out);
    };

    // The design mockup's life: Aldren of Bellmoor at 17.5, the page of the Main Life screen, as
    // the balance's good Warrior stands the month he swears to the Keep.
    LifeSave firstLife();

    // A life at its first frame, the first of a world or a later one: 6 years old, 4 of each
    // part and 35 of Vitality, nothing held and nothing earned, one line in the log. His sixth
    // year (activitytable.pg) is what brings him to 7 with his first rations and his first coin.
    LifeSave freshLife();

    // The same boy the day he is seven, for the dev flags and the tests: what a sixth year leaves
    // him at the least (6 of each part, 40 of Vitality, a year of rations), none of it lived: no
    // term done, no coin, the town not known. The works of his sixth year are closed to him.
    LifeSave boyOfSeven();
}
