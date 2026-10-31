#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Core/spells.h"
#include "ui.h"

namespace arena
{
    /** The player's spells: the three on the spell keys, and the ones saved in the book (by name). */
    struct SpellbookData
    {
        std::vector<SpellRecipe> equipped;
        std::vector<std::pair<std::string, SpellRecipe>> saved;
    };

    /** Where the book is kept between runs: $XDG_DATA_HOME/voxelarena/spellbook.json (or ~/.local/share). */
    std::string spellbookPath();
    bool loadSpellbook(const std::string& path, SpellbookData& out);
    bool saveSpellbook(const std::string& path, const SpellbookData& book);

    /** A spell's name: saved in the book, or a ready-made one with that recipe, or its words. */
    std::string spellName(const Content& content, const SpellbookData& book, const SpellRecipe& recipe);

    /**
     * The spellbook screen (B, UI_SPEC.md section 7): pick a spell key, then build its spell from
     * parts (an element, forms, modifiers), with the mouse or the part keys. The recipe shows as a row
     * of blocks with a diagram of what it does; its cost, cooldown and damage update as it is built.
     * The book lists the saved and ready-made spells: a click puts one on the key being built.
     */
    class Spellbook
    {
    public:
        void setOpen(bool open);
        bool isOpen() const { return open; }

        /** Draws it. `keys`: the spell keys' names (bindings); `partKey`: a part's key name by group and index. */
        void draw(Ui& ui, const Content& content, const SpellbookData& book, bool paused, const std::vector<std::string>& keys,
                  const std::function<std::string(int group, int index)>& partKey);

        /** A click's action (Ui::hitAt). Returns true when the spells on the keys changed; `closed` on Close. */
        bool act(int action, const Content& content, SpellbookData& book, bool& closed);

        /** A part key: group 0 elements, 1 forms, 2 modifiers. Returns true when the spells changed. */
        bool part(int group, int index, const Content& content, SpellbookData& book);

        /** Keys while open: Backspace takes the last word off (or a letter of the name), Enter saves, Delete clears. */
        bool backspace(SpellbookData& book);
        bool enter(const Content& content, SpellbookData& book);
        bool clear(SpellbookData& book);
        void type(const std::string& text);

        int editing = 0;          // the spell key being built
        std::string name;         // for saving
        bool naming = false;      // the name field has the keyboard
        std::string message;      // the last thing that happened ("saved")
        /** What may be built with (an arena run's: only what was bought); unset: everything. */
        std::function<bool(const std::string&)> owned;

        // Click actions (ERASE rather than DELETE: windows.h defines DELETE as a macro)
        static constexpr int SLOT = 100, ELEMENT = 200, FORM = 300, MODIFIER = 400, REMOVE = 500, CLEAR = 501, NAME = 502, SAVE = 503, CLOSE = 504,
                             PREV = 505, NEXT = 506, LOAD = 1000, ERASE = 2000;

    private:
        /** The parts in the order shown (and keyed): elements, forms, modifiers. */
        void order(const Content& content);
        bool add(const std::string& keyword, const Content& content, SpellbookData& book);

        bool open = false;
        std::vector<std::string> elements, forms, modifiers;

        struct Entry
        {
            std::string name, id;   // id: a ready-made spell's
            SpellRecipe recipe;
            int saved = -1;         // index in the book's saved list
        };
        std::vector<Entry> entries;
        int page = 0;

        // The mana the recipe cost before its last change
        std::string lastRecipe;
        float lastMana = -1.0f, manaChange = 0.0f;
    };
}
