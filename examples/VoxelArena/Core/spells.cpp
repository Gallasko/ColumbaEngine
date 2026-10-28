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
            s.error = "a spell starts with an element: fire, ice or volt";
            return s;
        }

        const ElementDef& element = content.elements.at(s.element);
        s.status = element.status;
        s.mana = element.mana;
        s.cooldown = 0.2f;

        if (recipe.keywords.empty())
        {
            s.error = "add a form: bolt, zone, nova or beam";
            return s;
        }

        if (recipe.keywords.size() > MAX_KEYWORDS)
        {
            s.error = "at most " + std::to_string(MAX_KEYWORDS) + " keywords";
            return s;
        }

        // Each form, with the cost of its keywords: added up, then scaled by its modifiers
        std::vector<float> stageMana, stageScale;

        for (const auto& id : recipe.keywords)
        {
            const KeywordDef* k = keyword(content, id);

            if (not k)
            {
                s.error = "no keyword \"" + id + "\"";
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
                s.error = k->name + " needs a form before it to change";
                return s;
            }

            SpellStage& st = s.stages.back();

            if (not k->forms.empty() and std::find(k->forms.begin(), k->forms.end(), st.form.shape) == k->forms.end())
            {
                s.error = k->name + " does not go on a " + st.form.name;
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
                s.error = "at most " + std::to_string(MAX_COPIES) + " of a " + st.form.name;
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
                s.error = "too many copies in all (" + std::to_string(copies) + ")";
                return s;
            }
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

        std::string out;

        for (size_t i = 0; i < spell.stages.size(); ++i)
        {
            const SpellStage& st = spell.stages[i];
            const std::string& shape = st.form.shape;
            const std::string many = st.count > 1 ? std::to_string(st.count) + " " : "";
            const std::string s = st.count > 1 ? "s" : "";

            if (i > 0)
                out += shape == "zone" ? ", leaving " : (shape == "nova" ? ", bursting into " : ", then ");

            if (shape == "bolt")
                out += (many.empty() ? "a bolt" : many + "bolts") + " (" + number(st.damage) + " damage)";
            else if (shape == "zone")
                out += (many.empty() ? "a zone" : many + "zones") + " (" + number(st.radius) + " m, " + number(st.duration) + " s, " + number(st.damage) + "/s)";
            else if (shape == "nova")
                out += (many.empty() ? "a nova" : many + "novas") + " (" + number(st.radius) + " m, " + number(st.damage) + " damage)";
            else if (shape == "beam")
                out += (many.empty() ? "a beam" : many + "beams") + " (" + number(st.range, 0) + " m, " + number(st.damage) + " damage)";
            else
                out += many + st.form.name + s;

            if (st.chains > 0)
                out += " that jumps to " + std::to_string(st.chains) + " more";
        }

        return out;
    }

    bool parseRecipe(const Content& content, const std::vector<std::string>& words, SpellRecipe& out, std::string& error)
    {
        out = SpellRecipe{};

        if (words.empty())
        {
            error = "an element first: fire, ice or volt";
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
