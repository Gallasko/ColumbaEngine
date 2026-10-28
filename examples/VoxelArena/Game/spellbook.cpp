#include "stdafx.h"

#include "spellbook.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Core/json.h"

namespace arena
{
    namespace
    {
        const glm::vec4 PANEL(14, 16, 26, 235);
        const glm::vec4 BUTTON(42, 48, 66, 240);
        const glm::vec4 SELECTED(92, 112, 168, 250);
        const glm::vec4 DISABLED(26, 28, 36, 210);
        const glm::vec4 TEXT(236, 238, 242, 255);

        constexpr float PANEL_Z = 6.0f, BUTTON_Z = 7.0f, LABEL_Z = 8.0f;

        std::string quoted(const std::string& s)
        {
            std::string out = "\"";
            for (char c : s)
            {
                if (c == '"' or c == '\\')
                    out += '\\';
                out += c;
            }
            return out + "\"";
        }

        std::string wordsJson(const SpellRecipe& r)
        {
            std::string out = "[" + quoted(r.element);
            for (const auto& k : r.keywords)
                out += ", " + quoted(k);
            return out + "]";
        }

        bool recipeFrom(const Json& j, SpellRecipe& out)
        {
            if (not j.isArray() or j.array.empty() or not j.array[0].isString())
                return false;

            out = SpellRecipe{};
            out.element = j.array[0].string;
            for (size_t i = 1; i < j.array.size(); ++i)
                if (j.array[i].isString())
                    out.keywords.push_back(j.array[i].string);
            return true;
        }

        std::string number(float v, int decimals)
        {
            std::ostringstream o;
            o.setf(std::ios::fixed);
            o.precision(decimals);
            o << v;
            return o.str();
        }

        const char* elementColour(const std::string& element)
        {
            if (element == "fire") return "\\c{255,170,100,255}";
            if (element == "ice") return "\\c{170,220,255,255}";
            if (element == "volt") return "\\c{255,240,140,255}";
            return "\\c{220,226,236,255}";
        }
    }

    // ---- The book on disk -------------------------------------------------------------------------

    std::string spellbookPath()
    {
        std::string base;

        if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg and *xdg)
            base = xdg;
        else if (const char* home = std::getenv("HOME"); home and *home)
            base = std::string(home) + "/.local/share";
        else
            base = ".";

        return base + "/voxelarena/spellbook.json";
    }

    bool loadSpellbook(const std::string& path, SpellbookData& out)
    {
        std::ifstream file(path);

        if (not file)
            return false;

        std::stringstream text;
        text << file.rdbuf();
        const JsonParseResult parsed = parseJson(text.str());

        if (not parsed.ok or not parsed.value.isObject())
            return false;

        SpellbookData book;

        if (const Json* equipped = parsed.value.find("equipped"))
        {
            for (const auto& e : equipped->array)
            {
                SpellRecipe r;
                recipeFrom(e, r);
                book.equipped.push_back(r);
            }
        }

        if (const Json* saved = parsed.value.find("saved"))
        {
            for (const auto& e : saved->array)
            {
                SpellRecipe r;
                const Json* recipe = e.find("recipe");

                if (recipe and recipeFrom(*recipe, r))
                    book.saved.emplace_back(e.str("name", "spell"), r);
            }
        }

        out = book;
        return true;
    }

    bool saveSpellbook(const std::string& path, const SpellbookData& book)
    {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

        std::ofstream file(path, std::ios::trunc);

        if (not file)
            return false;

        file << "{\n    \"equipped\": [";
        for (size_t i = 0; i < book.equipped.size(); ++i)
            file << (i ? ", " : "") << wordsJson(book.equipped[i]);
        file << "],\n    \"saved\": [";
        for (size_t i = 0; i < book.saved.size(); ++i)
            file << (i ? "," : "") << "\n        { \"name\": " << quoted(book.saved[i].first) << ", \"recipe\": " << wordsJson(book.saved[i].second) << " }";
        file << "\n    ]\n}\n";

        return static_cast<bool>(file);
    }

    std::string spellName(const Content& content, const SpellbookData& book, const SpellRecipe& recipe)
    {
        if (recipe.element.empty() and recipe.keywords.empty())
            return "(empty)";

        for (const auto& [name, r] : book.saved)
            if (r == recipe)
                return name;

        for (const auto& [id, s] : content.spells)
            if (s.recipe == recipe)
                return s.name;

        return recipeText(content, recipe);
    }

    // ---- The screen -------------------------------------------------------------------------------

    void Spellbook::init(Hud* h)
    {
        hud = h;
        pw = 1020.0f;
        ph = 610.0f;
        px = (hud->width() - pw) * 0.5f;
        py = (hud->height() - ph) * 0.5f;

        panel = hud->makeQuad(px, py, PANEL_Z, pw, ph, PANEL);
        title = hud->makeText(px + 24, py + 16, LABEL_Z, Hud::FONT, "Spellbook", 1.25f, TEXT);
        hint = hud->makeText(px + 24, py + ph - 34, LABEL_Z, Hud::MONO, "", 0.8f, glm::vec4(170, 174, 184, 255));
        recipe = hud->makeText(px + 24, py + 118, LABEL_Z, Hud::FONT, "", 1.05f, TEXT);
        cost = hud->makeText(px + 24, py + 150, LABEL_Z, Hud::MONO, "", 0.85f, TEXT);
        description = hud->makeText(px + 24, py + 174, LABEL_Z, Hud::MONO, "", 0.8f, glm::vec4(200, 204, 214, 255));
        labels = hud->makeText(px + 24, py + 218, LABEL_Z, Hud::MONO,
                               "Element\n\n\nForms\n\n\nModifiers", 0.85f, glm::vec4(150, 156, 170, 255));

        for (auto id : {panel, title, hint, recipe, cost, description, labels})
            hud->show(id, false);

        // Every button made now: an entity made while the screen is drawn has no components yet that frame
        button(63);
    }

    Spellbook::Button& Spellbook::button(size_t i)
    {
        while (buttons.size() <= i)
        {
            Button b;
            b.back = hud->makeQuad(0, 0, BUTTON_Z, 10, 10, BUTTON);
            b.label = hud->makeText(0, 0, LABEL_Z, Hud::FONT, "", 0.85f, TEXT);
            hud->show(b.back, false);
            hud->show(b.label, false);
            buttons.push_back(b);
        }

        return buttons[i];
    }

    void Spellbook::setButton(Button& b, float x, float y, float w, float h, const std::string& label, bool enabled, bool selected, Act act, const std::string& value, int index)
    {
        b.x = x;
        b.y = y;
        b.w = w;
        b.h = h;
        b.act = enabled ? act : Act::None;
        b.value = value;
        b.index = index;
        b.shown = true;

        hud->place(b.back, x, y, w, h, true);
        hud->color(b.back, selected ? SELECTED : (enabled ? BUTTON : DISABLED));
        hud->place(b.label, x + 10.0f, y + h * 0.5f - 11.0f, w, h, true);
        hud->text(b.label, (enabled ? "" : std::string("\\c{110,114,124,255}")) + label);
    }

    void Spellbook::setOpen(bool o)
    {
        open = o;
        naming = false;

        if (not open)
        {
            for (auto id : {panel, title, hint, recipe, cost, description, labels})
                hud->show(id, false);

            for (auto& b : buttons)
            {
                b.shown = false;
                hud->show(b.back, false);
                hud->show(b.label, false);
            }
        }
    }

    void Spellbook::refresh(const Content& content, const SpellbookData& book)
    {
        if (not open or not hud)
            return;

        for (auto id : {panel, title, hint, recipe, cost, description, labels})
            hud->show(id, true);

        used = 0;
        editing = std::clamp(editing, 0, static_cast<int>(book.equipped.size()) - 1);
        const SpellRecipe empty;
        const SpellRecipe& r = book.equipped.empty() ? empty : book.equipped[static_cast<size_t>(editing)];
        const Spell spell = compileSpell(content, r);

        // The spell keys
        for (size_t i = 0; i < book.equipped.size(); ++i)
        {
            const Spell s = compileSpell(content, book.equipped[i]);
            const std::string label = std::string(elementColour(book.equipped[i].element)) + "[" + std::to_string(i + 1) + "] " + spellName(content, book, book.equipped[i]) +
                                      (s.valid() ? "\\c{170,174,184,255}  " + number(s.mana, 0) + " mana" : "");
            setButton(button(used++), px + 24 + static_cast<float>(i) * 326.0f, py + 60, 314, 40, label, true, static_cast<int>(i) == editing, Act::Slot, "", static_cast<int>(i));
        }

        // The spell being built
        hud->text(recipe, std::string(elementColour(r.element)) + recipeText(content, r));
        hud->text(cost, spell.valid() ? "Mana " + number(spell.mana, 0) + "    Cooldown " + number(spell.cooldown, 1) + " s" : "\\c{255,130,120,255}" + spell.error);
        hud->text(description, spell.valid() ? describe(content, spell) : "");

        // What can be added: with no element yet, judged as if it had one
        SpellRecipe probe = r;
        if (probe.element.empty())
            probe.element = "fire";

        const float rowX = px + 150.0f;
        float x = rowX;

        for (const char* e : {"fire", "ice", "volt"})
        {
            Element el = Element::None;
            elementFromName(e, el);
            const std::string name = content.elements.count(el) ? content.elements.at(el).name : e;
            setButton(button(used++), x, py + 210, 130, 36, std::string(elementColour(e)) + name, true, r.element == e, Act::Element, e);
            x += 140.0f;
        }

        float formX = rowX, modX = rowX;

        for (const auto& [id, k] : content.keywords)
        {
            const bool ok = canAdd(content, probe, id);

            if (k.form)
            {
                setButton(button(used++), formX, py + 268, 130, 36, k.name, ok, false, Act::Keyword, id);
                formX += 140.0f;
            }
            else
            {
                setButton(button(used++), modX, py + 326, 130, 36, k.name, ok, false, Act::Keyword, id);
                modX += 140.0f;
            }
        }

        // Editing
        const float actY = py + 384;
        setButton(button(used++), px + 24, actY, 150, 36, "Remove last", not r.keywords.empty(), false, Act::RemoveLast);
        setButton(button(used++), px + 184, actY, 100, 36, "Clear", not r.element.empty() or not r.keywords.empty(), false, Act::Clear);
        setButton(button(used++), px + 294, actY, 300, 36, "Name: " + name + (naming ? "_" : ""), true, naming, Act::Name);
        setButton(button(used++), px + 604, actY, 170, 36, "Save to book", spell.valid(), false, Act::Save);

        // The book: ready-made spells, then the saved ones (a click puts it on the key being built)
        int k = 0;
        auto place = [&](const std::string& label, Act act, const std::string& value, int index, bool deletable) {
            const int col = k % 4, row = k / 4;
            const float bx = px + 24 + static_cast<float>(col) * 246.0f, by = py + 446 + static_cast<float>(row) * 38.0f;

            if (row >= 3)
                return;

            setButton(button(used++), bx, by, deletable ? 204.0f : 236.0f, 32, label, true, false, act, value, index);

            if (deletable)
                setButton(button(used++), bx + 208, by, 28, 32, "x", true, false, Act::Delete, "", index);
            ++k;
        };

        std::vector<std::pair<std::string, const SpellDef*>> ready;
        for (const auto& [id, s] : content.spells)
            ready.emplace_back(s.name, &s);
        std::sort(ready.begin(), ready.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& [n, s] : ready)
            place(std::string(elementColour(s->recipe.element)) + n, Act::Load, s->id, -1, false);
        for (size_t i = 0; i < book.saved.size(); ++i)
            place(std::string(elementColour(book.saved[i].second.element)) + book.saved[i].first, Act::Load, "", static_cast<int>(i), true);

        hud->text(hint, message.empty() ? "Pick a key, then click an element, forms and modifiers. A modifier changes the form before it.  B / Esc: close"
                                        : message);

        // The rest hidden
        for (size_t i = used; i < buttons.size(); ++i)
        {
            buttons[i].shown = false;
            hud->show(buttons[i].back, false);
            hud->show(buttons[i].label, false);
        }
    }

    bool Spellbook::click(float x, float y, const Content& content, SpellbookData& book)
    {
        if (not open or book.equipped.empty())
            return false;

        for (const auto& b : buttons)
        {
            if (not b.shown or b.act == Act::None or x < b.x or x > b.x + b.w or y < b.y or y > b.y + b.h)
                continue;

            SpellRecipe& r = book.equipped[static_cast<size_t>(editing)];
            message.clear();

            switch (b.act)
            {
                case Act::Slot:
                    editing = b.index;
                    naming = false;
                    return false;

                case Act::Element:
                    r.element = b.value;
                    return true;

                case Act::Keyword:
                {
                    SpellRecipe probe = r;
                    if (probe.element.empty())
                        probe.element = "fire";
                    if (not canAdd(content, probe, b.value))
                        return false;
                    r.keywords.push_back(b.value);
                    return true;
                }

                case Act::RemoveLast:
                    if (not r.keywords.empty())
                        r.keywords.pop_back();
                    return true;

                case Act::Clear:
                    r = SpellRecipe{};
                    return true;

                case Act::Name:
                    naming = not naming;
                    return false;

                case Act::Save:
                    enter(content, book);
                    return false;

                case Act::Load:
                    if (b.index >= 0 and b.index < static_cast<int>(book.saved.size()))
                        r = book.saved[static_cast<size_t>(b.index)].second;
                    else if (auto s = content.spells.find(b.value); s != content.spells.end())
                        r = s->second.recipe;
                    return true;

                case Act::Delete:
                    if (b.index >= 0 and b.index < static_cast<int>(book.saved.size()))
                    {
                        message = "removed \"" + book.saved[static_cast<size_t>(b.index)].first + "\" from the book";
                        book.saved.erase(book.saved.begin() + b.index);
                    }
                    return false;

                case Act::None:
                    break;
            }
        }

        return false;
    }

    bool Spellbook::backspace(SpellbookData& book)
    {
        if (naming)
        {
            if (not name.empty())
                name.pop_back();
            return false;
        }

        if (book.equipped.empty() or book.equipped[static_cast<size_t>(editing)].keywords.empty())
            return false;

        book.equipped[static_cast<size_t>(editing)].keywords.pop_back();
        return true;
    }

    bool Spellbook::enter(const Content& content, SpellbookData& book)
    {
        if (book.equipped.empty())
            return false;

        const SpellRecipe& r = book.equipped[static_cast<size_t>(editing)];

        if (not compileSpell(content, r).valid())
        {
            message = "not a spell yet: " + compileSpell(content, r).error;
            return false;
        }

        const std::string n = name.empty() ? recipeText(content, r) : name;

        // Same name: replaced
        for (auto& [saved, recipe] : book.saved)
        {
            if (saved == n)
            {
                recipe = r;
                message = "\"" + n + "\" updated in the book";
                name.clear();
                naming = false;
                return false;
            }
        }

        book.saved.emplace_back(n, r);
        message = "\"" + n + "\" saved to the book";
        name.clear();
        naming = false;
        return false;
    }

    void Spellbook::type(const std::string& text)
    {
        if (not naming)
            return;

        for (char c : text)
            if (c >= 32 and c < 127 and name.size() < 24)
                name += c;
    }
}
