#pragma once

#include <memory>
#include <string>
#include <vector>

#include "content.h"

namespace arena
{
    /** One form of a spell with what its modifiers made of it. */
    struct SpellStage
    {
        KeywordDef form;   // a copy: a spell still flying outlives a content reload
        int count = 1;
        float speed = 0.0f, gravity = 0.0f, radius = 0.0f, range = 0.0f, duration = 0.0f;
        float damage = 0.0f, poiseDamage = 0.0f, knockback = 0.0f, terrainDamage = 0.0f;
        int chains = 0;
    };

    /**
     * A recipe made into a spell: its stages in order, its cost and cooldown. `error` is set when the
     * recipe does not make a spell (a modifier with no form before it, an unknown word...).
     */
    struct Spell
    {
        SpellRecipe recipe;
        Element element = Element::None;
        StatusHit status;
        std::vector<SpellStage> stages;
        float mana = 0.0f;
        float cooldown = 0.0f;
        std::string error;

        bool valid() const { return error.empty(); }
    };

    /** The most keywords a spell may have, and the most copies of a single form. */
    constexpr size_t MAX_KEYWORDS = 8;
    constexpr int MAX_COPIES = 8;

    Spell compileSpell(const Content& content, const SpellRecipe& recipe);

    /** Whether adding `keyword` to the recipe still makes a spell (for the spellbook's buttons). */
    bool canAdd(const Content& content, const SpellRecipe& recipe, const std::string& keyword);

    /** "Fire > Bolt > Zone" */
    std::string recipeText(const Content& content, const SpellRecipe& recipe);

    /** A line on what it does: "2 bolts (12 damage), each leaving a zone (2.2 m, 7.0 s, 6/s)" */
    std::string describe(const Content& content, const Spell& spell);

    /** Recipe from words: "fire bolt multiply zone" (short keyword names resolve in base). */
    bool parseRecipe(const Content& content, const std::vector<std::string>& words, SpellRecipe& out, std::string& error);
}
