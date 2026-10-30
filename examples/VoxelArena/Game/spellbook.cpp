#include "stdafx.h"

#include "spellbook.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <cmath>

#include "Core/json.h"

namespace arena
{
    namespace
    {
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

    namespace
    {
        using namespace ui;

        const Colour MODIFIER_FILL = hex(0x2A2340);
        const Colour SLOT_DASH = hex(0x4A4F5C);
        const Colour DOT = hex(0x22252D);

        std::string localName(const std::string& id)
        {
            const size_t colon = id.find(':');
            return colon == std::string::npos ? id : id.substr(colon + 1);
        }

        /** A part's few words under its name. */
        std::string note(const std::string& part)
        {
            static const std::map<std::string, std::string> notes = {
                {"fire", "burns"}, {"ice", "chills, freezes"}, {"volt", "shocks, breaks stone"},
                {"bolt", "a shot"}, {"zone", "an area"}, {"nova", "a burst"}, {"beam", "a strike"},
                {"multiply", "two of it"}, {"extend", "longer"}, {"enlarge", "bigger"}, {"chain", "jumps on"}, {"empower", "harder"},
            };
            auto it = notes.find(localName(part));
            return it != notes.end() ? it->second : "";
        }

        Colour faded(Colour c, float a) { c.a *= a; return c; }

        const KeywordDef* findKeyword(const Content& content, const std::string& id)
        {
            auto it = content.keywords.find(id);
            return it != content.keywords.end() ? &it->second : nullptr;
        }
    }

    void Spellbook::setOpen(bool o)
    {
        open = o;
        naming = false;
    }

    void Spellbook::order(const Content& content)
    {
        elements = {"fire", "ice", "volt"};
        forms.clear();
        modifiers.clear();

        // The order of keywords.json's base parts first, then any a mod adds
        static const std::vector<std::string> formOrder = {"bolt", "zone", "nova", "beam"};
        static const std::vector<std::string> modOrder = {"multiply", "extend", "enlarge", "chain", "empower"};

        auto rank = [](const std::vector<std::string>& order, const std::string& id) {
            auto it = std::find(order.begin(), order.end(), localName(id));
            return it == order.end() ? 100 : static_cast<int>(it - order.begin());
        };

        for (const auto& [id, k] : content.keywords)
            (k.form ? forms : modifiers).push_back(id);

        std::stable_sort(forms.begin(), forms.end(), [&](const std::string& a, const std::string& b) { return rank(formOrder, a) < rank(formOrder, b); });
        std::stable_sort(modifiers.begin(), modifiers.end(), [&](const std::string& a, const std::string& b) { return rank(modOrder, a) < rank(modOrder, b); });
    }

    void Spellbook::draw(Ui& ui, const Content& content, const SpellbookData& book, bool paused, const std::vector<std::string>& keys,
                         const std::function<std::string(int, int)>& partKey)
    {
        if (not open)
            return;

        order(content);

        editing = std::clamp(editing, 0, std::max(0, static_cast<int>(book.equipped.size()) - 1));
        const SpellRecipe empty;
        const SpellRecipe& r = book.equipped.empty() ? empty : book.equipped[static_cast<size_t>(editing)];
        const Spell spell = compileSpell(content, r);

        // The mana's last change, as the recipe changes
        {
            const std::string now = recipeText(content, r);
            if (now != lastRecipe)
            {
                manaChange = lastMana >= 0.0f and spell.valid() ? spell.mana - lastMana : 0.0f;
                lastRecipe = now;
                lastMana = spell.valid() ? spell.mana : -1.0f;
            }
        }

        ui.section("spellbook");
        ui.anchor(Anchor::TopLeft);
        ui.rectPx(0, 0, ui.width(), ui.height(), withAlpha(INK, 0.58f));

        ui.anchor(Anchor::Centre);
        ui.raised(64, 32, 1152, 656, PANEL, 4.0f);

        // ---- The header
        float hx = 90.0f + ui.text(90, 50, "Spellbook", Font::Display, 40, TEXT) + 14.0f;
        if (paused)
        {
            const float tw = ui.measure("PAUSED", Font::Display, 16, 1.0f) + 12.0f;
            ui.rect(hx, 62, tw, 20, LINE);
            ui.text(hx + 6, 63, "PAUSED", Font::Display, 16, TEXT, Align::Left, 1.0f);
            hx += tw + 14.0f;
        }
        ui.text(hx, 66, "Pick a key, then an element, forms and modifiers. A modifier changes the form before it.", Font::Text500, 12, MUTED);

        {
            const float cw = 22.0f + 8.0f + ui.measure("Close", Font::Text600, 13) + 20.0f;
            const float cx = 1190.0f - cw;
            const bool over = ui.hover(cx, 57, cw, 30);
            ui.box(cx, 57, cw, 30, CARD, 2.0f, over ? GOLD : LINE);
            ui.keycap(cx + 6, 61, 22, "B", GOLD, GOLD_INK, 16, 0.0f);
            ui.text(cx + 36, 64, "Close", Font::Text600, 13, TEXT);
            ui.hit(cx, 57, cw, 30, CLOSE);
        }

        // ---- The spell keys
        for (size_t i = 0; i < book.equipped.size() and i < 3; ++i)
        {
            const float x = 90.0f + static_cast<float>(i) * 369.0f, y = 104.0f, w = 361.0f, h = 40.0f;
            const SpellRecipe& ri = book.equipped[i];
            const Spell si = compileSpell(content, ri);
            const bool current = static_cast<int>(i) == editing;
            const bool over = ui.hover(x, y, w, h);

            ui.box(x, y, w, h, CARD, 2.0f, current ? GOLD : (over ? TEXT_DIM : INK));
            ui.keycap(x + 8, y + 8, 24, i < keys.size() ? keys[i] : std::to_string(i + 1), current ? GOLD : TEXT, INK, 20, 0.0f);
            ui.text(x + 42, y + 11, spellName(content, book, ri), Font::Text700, 14, ri.element.empty() ? MUTED : elementText(ri.element));

            if (current)
                ui.text(x + w - 12, y + 10, "EDITING", Font::Display, 18, GOLD, Align::Right, 1.0f);
            else if (si.valid())
                ui.text(x + w - 12, y + 13, std::to_string(static_cast<int>(std::lround(si.mana))) + " mana", Font::Text600, 11, MANA_TEXT, Align::Right);

            ui.hit(x, y, w, h, SLOT + static_cast<int>(i));
        }

        // ---- The recipe: blocks in a row
        ui.box(90, 158, 784, 112, WELL);
        {
            // Wider than the well: the blocks narrow
            float natural = 96.0f;
            for (const auto& k : r.keywords)
            {
                const KeywordDef* def = findKeyword(content, k);
                natural += def and def->form ? 6.0f + 18.0f + 6.0f + 96.0f : 6.0f + 72.0f;
            }
            natural += 6.0f + 72.0f;
            const float squeeze = std::clamp((784.0f - 32.0f) / natural, 0.55f, 1.0f);
            const float formW = std::floor(96.0f * squeeze), modW = std::floor(72.0f * squeeze);

            float x = 106.0f;

            if (r.element.empty())
            {
                ui.dashed(x, 184, formW, 60, 2, 6, SLOT_DASH);
                ui.text(x + formW * 0.5f, 205, "ELEMENT", Font::Display, 16, FAINT, Align::Centre, 1.0f);
            }
            else
            {
                const Colour el = elementColour(r.element);
                ui.box(x, 184, formW, 60, el);
                ui.icon(r.element, x + 8, 192, 2.0f, INK);
                Element e = Element::None;
                const std::string shown = elementFromName(r.element, e) and content.elements.count(e) ? content.elements.at(e).name : r.element;
                ui.text(x + 8, 212, shown, Font::Display, 22, INK);
            }
            x += formW;

            for (const auto& k : r.keywords)
            {
                const KeywordDef* def = findKeyword(content, k);
                const std::string label = def ? def->name : k;

                if (def and def->form)
                {
                    x += 6.0f;
                    ui.text(x + 9, 196, ">", Font::Display, 28, FAINT, Align::Centre);
                    x += 18.0f + 6.0f;
                    ui.box(x, 184, formW, 60, CARD);
                    ui.rect(x + 2, 186, formW - 4, 5, TEXT);
                    ui.icon(localName(k), x + 8, 197, 2.0f, KIND_FORM);
                    ui.text(x + 8, 215, label, Font::Display, 22, TEXT);
                    x += formW;
                }
                else
                {
                    x += 6.0f;
                    ui.box(x, 190, modW, 48, MODIFIER_FILL);
                    ui.rect(x + 2, 192, modW - 4, 4, KIND_FORM);
                    ui.text(x + 8, 203, label, Font::Display, 18, TEXT);
                    x += modW;
                }
            }

            if (r.keywords.size() < MAX_KEYWORDS)
            {
                x += 6.0f;
                ui.dashed(x, 190, modW, 48, 2, 6, SLOT_DASH);
                ui.text(x + modW * 0.5f, 199, "+", Font::Display, 28, FAINT, Align::Centre);
            }
        }

        // ---- The readout (or what a hovered part would do)
        std::string preview;
        SpellRecipe previewRecipe;

        ui.raised(890, 158, 300, 112, CARD, 3.0f);
        {
            const bool ok = spell.valid();
            ui.text(904, 170, "MANA", Font::Text700, 11, MUTED, Align::Left, 1.5f);
            ui.text(1004, 170, "COOLDOWN", Font::Text700, 11, MUTED, Align::Left, 1.5f);
            ui.text(1104, 170, "DAMAGE", Font::Text700, 11, MUTED, Align::Left, 1.5f);

            if (ok)
            {
                const float mw = ui.text(904, 184, std::to_string(static_cast<int>(std::lround(spell.mana))), Font::Display, 36, MANA_TEXT);
                if (std::fabs(manaChange) >= 0.5f)
                    ui.text(904 + mw + 6, 196, (manaChange > 0.0f ? "+" : "") + std::to_string(static_cast<int>(std::lround(manaChange))), Font::Display, 18, GOOD);

                std::ostringstream cd;
                cd.setf(std::ios::fixed);
                cd.precision(1);
                cd << spell.cooldown << "s";
                ui.text(1004, 184, cd.str(), Font::Display, 36, TEXT);

                if (not spell.stages.empty())
                {
                    const SpellStage& st = spell.stages.front();
                    const float dw = ui.text(1104, 184, std::to_string(static_cast<int>(std::lround(st.damage))), Font::Display, 36, TEXT);
                    if (st.count > 1)
                        ui.text(1104 + dw + 4, 196, "x" + std::to_string(st.count), Font::Display, 18, TEXT_DIM);
                }
            }
            else
            {
                for (float cx : {904.0f, 1004.0f, 1104.0f})
                    ui.text(cx, 184, "-", Font::Display, 36, FAINT);
            }
        }

        // ---- The parts
        auto tiles = [&](int group, const std::vector<std::string>& parts, float y, const std::string& label) {
            ui.text(90, y + 17, label, Font::Text700, 11, MUTED, Align::Left, 1.5f);

            for (size_t i = 0; i < parts.size(); ++i)
            {
                const std::string& id = parts[i];
                const float x = 176.0f + static_cast<float>(i) * 104.0f;

                // What it is, and whether it may go next
                std::string name, reason;
                Colour band;
                bool inUse = false, bought = true, allowed = true;

                if (group == 0)
                {
                    Element e = Element::None;
                    name = elementFromName(id, e) and content.elements.count(e) ? content.elements.at(e).name : id;
                    band = elementColour(id);
                    inUse = r.element == id;
                    bought = not owned or owned("element:" + id);
                }
                else
                {
                    const KeywordDef* def = findKeyword(content, id);
                    name = def ? def->name : id;
                    band = group == 1 ? KIND_FORM : KIND_MODIFIER;
                    inUse = std::find(r.keywords.begin(), r.keywords.end(), id) != r.keywords.end();
                    bought = not owned or owned(id);

                    SpellRecipe probe = r;
                    if (probe.element.empty())
                        probe.element = "fire";
                    probe.keywords.push_back(id);
                    const Spell next = compileSpell(content, probe);
                    allowed = next.valid();
                    reason = next.error;
                }

                const bool usable = allowed and bought;
                const float a = usable ? 1.0f : 0.45f;
                const bool over = ui.hover(x, y, 98, 48);

                if (over and usable)
                {
                    previewRecipe = r;
                    if (group == 0)
                        previewRecipe.element = id;
                    else
                        previewRecipe.keywords.push_back(id);
                    preview = describe(content, compileSpell(content, previewRecipe));
                }

                Colour border = INK;
                if (over and usable)
                    border = GOLD;
                else if (inUse)
                    border = group == 0 ? band : faded(GOLD, 0.8f);

                ui.box(x, y, 98, 48, faded(CARD, usable ? 1.0f : 0.7f), 2.0f, faded(border, a));
                ui.rect(x + 2, y + 2, 94, 4, faded(band, a));

                const std::string key = partKey ? partKey(group, static_cast<int>(i)) : "";
                if (not key.empty())
                {
                    ui.box(x + 98 - 22, y + 10, 16, 16, faded(TEXT_DIM, a), 0.0f);
                    ui.text(x + 98 - 14, y + 10, key, Font::Display, 15, faded(INK, a), Align::Centre);
                }

                // A long name gets smaller rather than run under the key
                const float nameSize = ui.measure(name, Font::Display, 22) > (key.empty() ? 80.0f : 60.0f) ? 17.0f : 22.0f;
                ui.text(x + 10, y + 9 + (22.0f - nameSize) * 0.5f, name, Font::Display, nameSize, faded(TEXT, a));

                if (not bought)
                    ui.text(x + 10, y + 32, "not bought", Font::Text600, 10, WARNING);
                else if (not allowed)
                {
                    const auto lines = ui.wrap(reason, Font::Text600, 10, 80);
                    ui.text(x + 10, y + 32, lines.empty() ? "" : lines[0] + (lines.size() > 1 ? "\xE2\x80\xA6" : ""), Font::Text600, 10, faded(MUTED, 1.0f));
                }
                else
                    ui.text(x + 10, y + 32, note(id), Font::Text600, 10, MUTED);

                if (usable)
                    ui.hit(x, y, 98, 48, (group == 0 ? ELEMENT : (group == 1 ? FORM : MODIFIER)) + static_cast<int>(i));
            }
        };

        tiles(0, elements, 284, "ELEMENT");
        tiles(1, forms, 342, "FORMS");
        tiles(2, modifiers, 400, "MODIFIERS");

        // The readout's sentence: what it does, or would do with the part under the mouse
        {
            const std::string sentence = not preview.empty() ? preview : (spell.valid() ? describe(content, spell) : spell.error);
            const Colour c = not preview.empty() ? GOLD : (spell.valid() ? TEXT_DIM : WARNING);
            ui.paragraph(904, 228, 272, sentence, Font::Text500, 13, c, 17.0f, 2);
        }

        // ---- The diagram
        ui.box(890, 284, 300, 180, WELL);
        {
            for (float gy = 296.0f; gy < 284.0f + 176.0f; gy += 24.0f)
                for (float gx = 902.0f; gx < 890.0f + 296.0f; gx += 24.0f)
                    ui.rect(gx, gy, 2, 2, DOT);

            const Spell& shown = not preview.empty() ? compileSpell(content, previewRecipe) : spell;
            const Colour el = elementColour(shown.recipe.element.empty() ? r.element : shown.recipe.element);

            const float midY = 374.0f;
            ui.rect(904, midY - 8, 16, 16, TEXT);
            ui.text(912, midY + 12, "YOU", Font::Display, 16, MUTED, Align::Centre);

            if (shown.valid() and not shown.stages.empty())
            {
                const float room = 1176.0f - 928.0f;
                const float step = std::min(90.0f, room / static_cast<float>(shown.stages.size()));
                std::vector<float> ys = {midY};
                float x = 928.0f;

                for (const auto& st : shown.stages)
                {
                    // The copies of this stage, from each copy of the one before
                    std::vector<float> next;
                    const int copies = std::min(st.count, 3);
                    for (float y0 : ys)
                        for (int c = 0; c < copies; ++c)
                            next.push_back(y0 + (static_cast<float>(c) - 0.5f * static_cast<float>(copies - 1)) * 16.0f * (st.form.shape == "zone" ? 1.6f : 1.0f));
                    if (next.size() > 6)
                        next.resize(6);

                    const std::string& shape = st.form.shape;
                    const float len = step - 8.0f;
                    const float big = std::clamp(st.radius / 2.2f, 0.7f, 1.6f);

                    for (float y : next)
                    {
                        const float cy = std::clamp(y, 300.0f, 446.0f);

                        if (shape == "bolt")
                        {
                            // A trail of squares growing to the shot
                            for (int k = 0; k < 4; ++k)
                            {
                                const float size = 4.0f + static_cast<float>(k) * 4.0f / 3.0f;
                                const float px = x + (len - 10.0f) * static_cast<float>(k) / 3.0f;
                                ui.rect(std::round(px), std::round(cy - size * 0.5f), std::round(size), std::round(size), el);
                            }
                        }
                        else if (shape == "beam")
                        {
                            ui.rect(x, cy - 2, len, 4, el);
                            ui.rect(x + len - 6, cy - 5, 6, 10, el);
                        }
                        else if (shape == "zone")
                        {
                            const float zw = std::min(len, 44.0f * big), zh = 22.0f * big;
                            ui.rect(x + (len - zw) * 0.5f + 2.0f, cy - zh * 0.5f + 2.0f, zw - 4.0f, zh - 4.0f, faded(el, 0.28f));
                            ui.frame(x + (len - zw) * 0.5f, cy - zh * 0.5f, zw, zh, 2.0f, el);
                        }
                        else if (shape == "nova")
                        {
                            const float rad = 14.0f * big;
                            const float ncx = x + len * 0.5f;
                            for (int k = 0; k < 8; ++k)
                            {
                                const float ang = static_cast<float>(k) * 3.14159265f / 4.0f;
                                ui.rect(std::round(ncx + std::cos(ang) * rad - 3), std::round(cy + std::sin(ang) * rad - 3), 6, 6, el);
                            }
                        }

                        if (st.chains > 0)
                            for (int k = 0; k < st.chains; ++k)
                                ui.rect(x + len - 4 + static_cast<float>(k) * 6.0f, cy - 12 - static_cast<float>(k) * 5.0f, 4, 4, el);
                    }

                    ui.text(x + len * 0.5f, 292, st.form.name, Font::Display, 16, TEXT_DIM, Align::Centre);
                    if (st.count > 1)
                        ui.text(x + len * 0.5f, 440, "x" + std::to_string(st.count), Font::Display, 16, el, Align::Centre);

                    ys = next;
                    x += step;
                }
            }
        }

        // ---- The actions
        {
            float x = 90.0f;
            auto button = [&](const std::string& key, const std::string& label, int action, bool enabled, bool gold) {
                const float kw = ui.measure(key, Font::Display, 15) + 10.0f;
                const float w = 6.0f + kw + 8.0f + ui.measure(label, Font::Text600, 13) + 12.0f;
                const bool over = enabled and ui.hover(x, 478, w, 32);
                const float a = enabled ? 1.0f : 0.45f;

                if (gold)
                    ui.raised(x, 478, w, 32, faded(GOLD, a), enabled ? 3.0f : 0.0f, 2.0f, over ? TEXT : INK);
                else
                    ui.box(x, 478, w, 32, CARD, 2.0f, over ? GOLD : LINE);

                ui.box(x + 6, 483, kw, 22, gold ? INK : faded(TEXT_DIM, a), 0.0f);
                ui.text(x + 6 + kw * 0.5f, 485, key, Font::Display, 15, gold ? GOLD : INK, Align::Centre);
                ui.text(x + 6 + kw + 8, 486, label, Font::Text600, 13, faded(gold ? GOLD_INK : TEXT, a));

                if (enabled)
                    ui.hit(x, 478, w, 32, action);
                x += w + 10.0f;
            };

            button("Backspace", "Remove last", REMOVE, not r.keywords.empty(), false);
            button("Del", "Clear", CLEAR, not r.element.empty() or not r.keywords.empty(), false);

            ui.text(x + 4, 489, "NAME", Font::Text700, 11, MUTED, Align::Left, 1.5f);
            x += 4.0f + ui.measure("NAME", Font::Text700, 11, 1.5f) + 10.0f;
            const bool over = ui.hover(x, 478, 200, 32);
            ui.box(x, 478, 200, 32, WELL, 2.0f, naming ? GOLD : (over ? TEXT_DIM : LINE));
            ui.text(x + 10, 486, ui.fit(name.empty() and not naming ? recipeText(content, r) : name + (naming ? "_" : ""), Font::Text600, 13, 180.0f), Font::Text600, 13,
                    name.empty() and not naming ? FAINT : TEXT);
            ui.hit(x, 478, 200, 32, NAME);
            x += 210.0f;

            button("Enter", "Save to book", SAVE, spell.valid(), true);

            ui.text(1190, 486, message, Font::Text600, 12, GOOD, Align::Right);
        }

        // ---- The book: saved spells first, then the ready-made ones
        entries.clear();
        for (size_t i = 0; i < book.saved.size(); ++i)
            entries.push_back({book.saved[i].first, "", book.saved[i].second, static_cast<int>(i)});
        {
            std::vector<Entry> ready;
            for (const auto& [id, s] : content.spells)
                ready.push_back({s.name, s.id, s.recipe, -1});
            std::sort(ready.begin(), ready.end(), [](const Entry& a, const Entry& b) { return a.name < b.name; });
            entries.insert(entries.end(), ready.begin(), ready.end());
        }

        constexpr int PER_PAGE = 10;
        const int pages = std::max(1, (static_cast<int>(entries.size()) + PER_PAGE - 1) / PER_PAGE);
        page = std::clamp(page, 0, pages - 1);

        ui.text(90, 524, "BOOK", Font::Text700, 11, MUTED, Align::Left, 1.5f);
        if (pages > 1)
        {
            const float px = 90.0f + ui.measure("BOOK", Font::Text700, 11, 1.5f) + 14.0f;
            ui.text(px, 522, std::to_string(page + 1) + " / " + std::to_string(pages), Font::Text600, 11, FAINT);
            const float ax = px + ui.measure("10 / 10", Font::Text600, 11) + 10.0f;
            ui.icon("arrow_left", ax, 521, 2.0f, page > 0 ? TEXT_DIM : FAINT);
            ui.icon("arrow_right", ax + 22, 521, 2.0f, page + 1 < pages ? TEXT_DIM : FAINT);
            ui.hit(ax - 3, 518, 20, 20, PREV);
            ui.hit(ax + 19, 518, 20, 20, NEXT);
        }

        for (int k = 0; k < PER_PAGE; ++k)
        {
            const int index = page * PER_PAGE + k;
            if (index >= static_cast<int>(entries.size()))
                break;

            const Entry& e = entries[static_cast<size_t>(index)];
            const float x = 90.0f + static_cast<float>(k % 5) * 221.0f, y = 544.0f + static_cast<float>(k / 5) * 50.0f;
            const bool over = ui.hover(x, y, 216, 44);
            const bool here = e.recipe == r;

            ui.box(x, y, 216, 44, CARD, 2.0f, over ? GOLD : (here ? TEXT_DIM : INK));
            ui.rect(x + 2, y + 2, 4, 40, elementColour(e.recipe.element));
            ui.text(x + 14, y + 7, e.name, Font::Text700, 13, TEXT);
            ui.text(x + 14, y + 26, recipeText(content, e.recipe), Font::Text500, 10, MUTED);
            ui.hit(x, y, 216, 44, LOAD + index);

            if (e.saved >= 0)
            {
                const bool overX = ui.hover(x + 216 - 22, y + 6, 16, 16);
                ui.text(x + 216 - 14, y + 4, "x", Font::Display, 18, overX ? WARNING : FAINT, Align::Centre);
                ui.hit(x + 216 - 22, y + 6, 16, 16, DELETE + e.saved);
            }
        }
    }

    bool Spellbook::add(const std::string& id, const Content& content, SpellbookData& book)
    {
        if (book.equipped.empty())
            return false;

        SpellRecipe& r = book.equipped[static_cast<size_t>(editing)];
        SpellRecipe probe = r;
        if (probe.element.empty())
            probe.element = "fire";

        if (not canAdd(content, probe, id) or (owned and not owned(id)))
            return false;

        r.keywords.push_back(id);
        return true;
    }

    bool Spellbook::part(int group, int index, const Content& content, SpellbookData& book)
    {
        order(content);
        message.clear();

        if (book.equipped.empty() or index < 0)
            return false;

        if (group == 0)
        {
            if (index >= static_cast<int>(elements.size()) or (owned and not owned("element:" + elements[static_cast<size_t>(index)])))
                return false;
            book.equipped[static_cast<size_t>(editing)].element = elements[static_cast<size_t>(index)];
            return true;
        }

        const auto& list = group == 1 ? forms : modifiers;
        return index < static_cast<int>(list.size()) and add(list[static_cast<size_t>(index)], content, book);
    }

    bool Spellbook::act(int action, const Content& content, SpellbookData& book, bool& closed)
    {
        closed = false;

        if (not open or book.equipped.empty() or action == 0)
            return false;

        message.clear();

        if (action == CLOSE)
        {
            closed = true;
            return false;
        }

        if (action >= DELETE)
        {
            const int i = action - DELETE;
            if (i >= 0 and i < static_cast<int>(book.saved.size()))
            {
                message = "removed \"" + book.saved[static_cast<size_t>(i)].first + "\" from the book";
                book.saved.erase(book.saved.begin() + i);
            }
            return false;
        }

        if (action >= LOAD)
        {
            const int i = action - LOAD;
            if (i >= 0 and i < static_cast<int>(entries.size()))
            {
                book.equipped[static_cast<size_t>(editing)] = entries[static_cast<size_t>(i)].recipe;
                return true;
            }
            return false;
        }

        switch (action)
        {
            case REMOVE: return backspace(book);
            case CLEAR: return clear(book);
            case NAME: naming = not naming; return false;
            case SAVE: enter(content, book); return false;
            case PREV: --page; return false;
            case NEXT: ++page; return false;
            default: break;
        }

        if (action >= MODIFIER)
            return part(2, action - MODIFIER, content, book);
        if (action >= FORM)
            return part(1, action - FORM, content, book);
        if (action >= ELEMENT)
            return part(0, action - ELEMENT, content, book);
        if (action >= SLOT)
        {
            editing = std::clamp(action - SLOT, 0, static_cast<int>(book.equipped.size()) - 1);
            naming = false;
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

    bool Spellbook::clear(SpellbookData& book)
    {
        if (book.equipped.empty())
            return false;

        book.equipped[static_cast<size_t>(editing)] = SpellRecipe{};
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
