#include "spells.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace arena
{
    namespace
    {
        std::string number(float v, int decimals = 1)
        {
            std::ostringstream o;
            o.setf(std::ios::fixed);
            o.precision(decimals);
            o << v;
            std::string s = o.str();

            // 12.0 reads as 12
            if (s.find('.') != std::string::npos)
            {
                while (s.back() == '0')
                    s.pop_back();
                if (s.back() == '.')
                    s.pop_back();
            }

            return s;
        }

        const KeywordDef* keyword(const Content& content, const std::string& id)
        {
            auto it = content.keywords.find(id);
            return it == content.keywords.end() ? nullptr : &it->second;
        }
    }

    Spell compileSpell(const Content& content, const SpellRecipe& recipe)
    {
        Spell s;
        s.recipe = recipe;

        if (not elementFromName(recipe.element, s.element) or s.element == Element::None or not content.elements.count(s.element))
        {
            s.error = "Start with an element: fire, ice or volt.";
            return s;
        }

        const ElementDef& element = content.elements.at(s.element);
        s.status = element.status;
        s.mana = element.mana;
        s.cooldown = 0.2f;

        if (recipe.keywords.empty())
        {
            s.error = "Add a form: Bolt, Zone, Nova or Beam.";
            return s;
        }

        if (recipe.keywords.size() > MAX_KEYWORDS)
        {
            s.error = "At most " + std::to_string(MAX_KEYWORDS) + " keywords.";
            return s;
        }

        // Each form, with the cost of its keywords: added up, then scaled by its modifiers
        std::vector<float> stageMana, stageScale;

        for (const auto& id : recipe.keywords)
        {
            const KeywordDef* k = keyword(content, id);

            if (not k)
            {
                s.error = "Unknown keyword \"" + id + "\".";
                return s;
            }

            s.cooldown += k->cooldown;

            if (k->form)
            {
                SpellStage st;
                st.form = *k;
                st.speed = k->speed;
                st.gravity = k->gravity;
                st.radius = k->radius;
                st.range = k->range;
                st.duration = k->duration;
                st.damage = k->damage;
                st.poiseDamage = k->poiseDamage;
                st.knockback = k->knockback;
                st.terrainDamage = k->terrainDamage;
                s.stages.push_back(st);
                stageMana.push_back(k->mana);
                stageScale.push_back(1.0f);
                continue;
            }

            // A modifier: on the last form before it
            if (s.stages.empty())
            {
                s.error = k->name + " needs a form before it.";
                return s;
            }

            SpellStage& st = s.stages.back();

            if (not k->forms.empty() and std::find(k->forms.begin(), k->forms.end(), st.form.shape) == k->forms.end())
            {
                s.error = k->name + " can't be used on " + st.form.name + ".";
                return s;
            }

            st.count *= std::max(1, k->count);
            st.duration *= k->durationScale;
            st.range *= k->rangeScale;
            st.radius *= k->radiusScale;
            st.damage *= k->damageScale;
            st.poiseDamage *= k->damageScale;
            st.chains += k->chains;
            stageMana.back() += k->mana;
            stageScale.back() *= k->manaScale;

            if (st.count > MAX_COPIES)
            {
                s.error = "At most " + std::to_string(MAX_COPIES) + " of one form.";
                return s;
            }
        }

        // A form is paid once per copy of it: two bolts that each leave a zone pay for two zones
        int copies = 1;

        for (size_t i = 0; i < s.stages.size(); ++i)
        {
            s.mana += stageMana[i] * stageScale[i] * static_cast<float>(copies);
            copies *= s.stages[i].count;

            if (copies > MAX_COPIES * 2)
            {
                s.error = "Too many copies (" + std::to_string(copies) + ").";
                return s;
            }
        }

        // A run's boosts (tag items, mastery, a branch), on every form of it; more projectiles: more of its first bolt
        const SpellTweak& t = recipe.tweak;
        if (not t.empty())
        {
            for (auto& st : s.stages)
            {
                if (t.count != 0 and st.form.shape == "bolt")
                {
                    st.count = std::clamp(st.count + t.count, 1, MAX_COPIES);
                    break;
                }
            }

            for (auto& st : s.stages)
            {
                st.damage *= 1.0f + t.damage;
                st.poiseDamage *= 1.0f + t.damage;
                st.radius *= 1.0f + t.area;
                st.duration *= 1.0f + t.duration;
                st.speed *= 1.0f + t.speed;
            }

            s.mana *= std::max(0.2f, 1.0f - t.cost);
            s.cooldown *= std::max(0.2f, 1.0f - t.cooldown);
        }

        return s;
    }

    bool canAdd(const Content& content, const SpellRecipe& recipe, const std::string& id)
    {
        SpellRecipe more = recipe;
        more.keywords.push_back(id);
        return compileSpell(content, more).valid();
    }

    std::string recipeText(const Content& content, const SpellRecipe& recipe)
    {
        std::string out;
        Element e = Element::None;

        if (elementFromName(recipe.element, e) and content.elements.count(e))
            out = content.elements.at(e).name;
        else
            out = recipe.element.empty() ? "?" : recipe.element;

        for (const auto& id : recipe.keywords)
        {
            const KeywordDef* k = keyword(content, id);
            out += " > " + (k ? k->name : id);
        }

        return out;
    }

    std::string describe(const Content&, const Spell& spell)
    {
        if (not spell.valid())
            return spell.error;

        // One plain sentence a part: "Bolt: 12 damage. Area: 6 damage per second for 4 s, 2.2 m wide."
        std::string out;

        for (const SpellStage& st : spell.stages)
        {
            const std::string& shape = st.form.shape;
            const bool many = st.count > 1;
            const std::string each = many ? " each" : "";
            std::string part;

            if (shape == "bolt")
                part = (many ? std::to_string(st.count) + " bolts: " : "Bolt: ") + number(st.damage) + " damage" + each;
            else if (shape == "zone")
                part = (many ? std::to_string(st.count) + " areas: " : "Area: ") + number(st.damage) + " damage per second for " + number(st.duration) + " s, " +
                       number(st.radius * 2.0f) + " m wide";
            else if (shape == "nova")
                part = (many ? std::to_string(st.count) + " explosions: " : "Explosion: ") + number(st.damage) + " damage" + each + ", " + number(st.radius * 2.0f) + " m wide";
            else if (shape == "beam")
                part = (many ? std::to_string(st.count) + " beams: " : "Beam: ") + number(st.damage) + " damage" + each + ", " + number(st.range, 0) + " m long";
            else
                part = st.form.name;

            if (st.chains > 0)
                part += ", jumps to " + std::to_string(st.chains) + (st.chains == 1 ? " more enemy" : " more enemies");

            out += (out.empty() ? "" : " ") + part + ".";
        }

        return out;
    }

    bool parseRecipe(const Content& content, const std::vector<std::string>& words, SpellRecipe& out, std::string& error)
    {
        out = SpellRecipe{};

        if (words.empty())
        {
            error = "Start with an element: fire, ice or volt.";
            return false;
        }

        out.element = words[0];

        for (size_t i = 1; i < words.size(); ++i)
        {
            std::string id = words[i];

            if (id.find(':') == std::string::npos)
            {
                // A short name: base's, or the first keyword of any mod with that name
                id = "base:" + words[i];

                if (not content.keywords.count(id))
                    for (const auto& [kid, k] : content.keywords)
                        if (kid.substr(kid.find(':') + 1) == words[i])
                            id = kid;
            }

            out.keywords.push_back(id);
        }

        const Spell s = compileSpell(content, out);
        error = s.error;
        return s.valid();
    }
}
