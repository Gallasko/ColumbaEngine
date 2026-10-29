#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Core/spells.h"
#include "hud.h"

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
     * The spellbook screen (B): pick a spell key, then build its spell with the mouse: an element,
     * forms and modifiers. Its cost and what it does show as it is built. The book lists the
     * ready-made spells and the saved ones: a click puts one on the key being built.
     */
    class Spellbook
    {
    public:
        void init(Hud* hud);

        void setOpen(bool open);
        bool isOpen() const { return open; }

        /** Draws it as it is now. */
        void refresh(const Content& content, const SpellbookData& book);

        /** A click at screen pixels. Returns true when the spells on the keys changed. */
        bool click(float x, float y, const Content& content, SpellbookData& book);

        /** Keys while open: Backspace takes the last word off (or a letter of the name), Enter saves. */
        bool backspace(SpellbookData& book);
        bool enter(const Content& content, SpellbookData& book);
        void type(const std::string& text);

        int editing = 0;          // the spell key being built
        std::string name;         // for saving
        bool naming = false;      // the name field has the keyboard
        std::string message;      // the last thing that happened ("saved")
        /** What may be built with (an arena run's: only what was bought); unset: everything. */
        std::function<bool(const std::string&)> owned;

    private:
        enum class Act { None, Slot, Element, Keyword, RemoveLast, Clear, Save, Name, Load, Delete };

        struct Button
        {
            pg::_unique_id back = 0, label = 0;
            float x = 0, y = 0, w = 0, h = 0;
            Act act = Act::None;
            std::string value;    // element name, keyword id, spell id
            int index = -1;       // slot, saved entry
            bool shown = false;
        };

        Button& button(size_t i);
        void setButton(Button& b, float x, float y, float w, float h, const std::string& label, bool enabled, bool selected, Act act, const std::string& value = "", int index = -1);

        Hud* hud = nullptr;
        bool open = false;
        float px = 0, py = 0, pw = 0, ph = 0;   // the panel
        pg::_unique_id panel = 0, title = 0, recipe = 0, cost = 0, description = 0, hint = 0, labels = 0;
        std::vector<Button> buttons;
        size_t used = 0;
    };
}
