#include "world.h"

#include <algorithm>
#include <array>
#include <cmath>

// The arena run: timed waves with enemies coming in all the while, shards they drop (money and XP),
// a shop between waves, bosses on some waves, a score. The server's (or the host's, or the only
// player's): clients see it in the snapshots.

namespace arena
{
    namespace
    {
        constexpr std::array<const char*, 3> ELEMENT_TAGS = {"fire", "ice", "volt"};
    }

    uint32_t ArenaWorld::runRandom()
    {
        // xorshift: the same seed plays the same run
        uint32_t x = run.seed ? run.seed : 1u;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        run.seed = x;
        return x;
    }

    std::vector<std::string> ArenaWorld::rollFirstSpells()
    {
        // The pool shuffled (the run's seed), then taken in order: first one of each element not offered yet, then any
        std::vector<std::string> pool = content->run.startChoices;
        for (size_t i = pool.size(); i > 1; --i)
            std::swap(pool[i - 1], pool[runRandom() % i]);

        const size_t count = content->run.startChoiceCount > 0 ? std::min(pool.size(), static_cast<size_t>(content->run.startChoiceCount)) : pool.size();
        std::vector<std::string> out;
        std::vector<std::string> elements;
        auto elementOf = [&](const std::string& id) {
            auto s = content->spells.find(id);
            return s != content->spells.end() ? s->second.recipe.element : std::string();
        };

        for (const auto& id : pool)
            if (out.size() < count and std::find(elements.begin(), elements.end(), elementOf(id)) == elements.end())
            {
                out.push_back(id);
                elements.push_back(elementOf(id));
            }
        for (const auto& id : pool)
            if (out.size() < count and std::find(out.begin(), out.end(), id) == out.end())
                out.push_back(id);
        return out;
    }

    std::vector<std::string> ArenaWorld::rollAffixes(int count)
    {
        std::vector<std::string> out;

        // By weight, each once
        for (int k = 0; k < count; ++k)
        {
            float total = 0.0f;
            for (const auto& [id, a] : content->affixes)
                if (std::find(out.begin(), out.end(), id) == out.end())
                    total += a.weight;
            if (total <= 0.0f)
                break;

            float roll = static_cast<float>(runRandom() % 100000) / 100000.0f * total;
            for (const auto& [id, a] : content->affixes)
            {
                if (std::find(out.begin(), out.end(), id) != out.end() or a.weight <= 0.0f)
                    continue;
                roll -= a.weight;
                if (roll < 0.0f)
                {
                    out.push_back(id);
                    break;
                }
            }
        }

        return out;
    }

    bool ArenaWorld::owns(const Fighter& f, const std::string& id) const
    {
        // Outside a run (and for enemies) everything is there
        return not inRun() or f.team != Team::Player or std::find(f.owned.begin(), f.owned.end(), id) != f.owned.end();
    }

    int ArenaWorld::xpFor(int level) const
    {
        return content->run.xpBase + content->run.xpStep * std::max(0, level - 1);
    }

    float ArenaWorld::waveDuration(int wave) const
    {
        const RunDef& def = content->run;
        const float t = def.waves > 1 ? static_cast<float>(wave - 1) / static_cast<float>(def.waves - 1) : 0.0f;
        return def.duration.x + (def.duration.y - def.duration.x) * t;
    }

    void ArenaWorld::sumBoons(Fighter& f) const
    {
        f.boon = Fighter::Boons{};

        if (not content)
            return;

        for (const auto& b : f.boons)
        {
            auto it = content->upgrades.find(b.id);

            if (it == content->upgrades.end() or it->second.forTag())
                continue;

            // Its numbers times its rarity's power; whole ones (dash charges, stacks) as they are, and a downside
            // (a negative number, more damage taken) as written: a rarer trade-off is better, not worse too
            const UpgradeDef& u = it->second;
            const float k = rarityPower(b.rarity);
            auto up = [k](float v) { return v > 0.0f ? v * k : v; };
            Fighter::Boons& o = f.boon;
            o.maxHealth += up(u.maxHealth);
            o.damage += up(u.damage);
            o.dashCharges += u.dashCharges;
            o.manaCost += up(u.manaCost);
            o.manaRegen += up(u.manaRegen);
            o.staminaRegen += up(u.staminaRegen);
            o.speed += up(u.speed);
            o.cooldown += up(u.cooldown);
            o.lifeOnHit += up(u.lifeOnHit);
            o.statusStacks += u.statusStacks;
            o.armor += up(u.armor);
            o.crit += up(u.crit);
            o.pickupRange += up(u.pickupRange);
            o.regen += up(u.regen);
            o.attackSpeed += up(u.attackSpeed);
            o.horde += up(u.horde);
            o.xpGain += up(u.xpGain);
            o.shardChance += up(u.shardChance);
            o.comboDamage += up(u.comboDamage);
            o.statusDuration += up(u.statusDuration);
            o.payoff.add(u.payoff, k);
            o.healthScale += up(u.healthScale);
            o.damageTaken += u.damageTaken > 0.0f ? u.damageTaken : u.damageTaken * k;
            o.hurtDamage += up(u.hurtDamage);
            o.airDamage += up(u.airDamage);
            o.executeDamage += up(u.executeDamage);
            o.revengeDamage += up(u.revengeDamage);
            o.killHeal += up(u.killHeal);

            // The triggers: the first item of each names them in the recap
            if (u.thorns > 0.0f)
            {
                o.thorns += u.thorns * k;
                if (o.thornsItem.empty())
                    o.thornsItem = u.id;
            }
            if (u.dodgeGround.any())
            {
                if (o.dodgeItem.empty())
                {
                    o.dodgeGround = u.dodgeGround;
                    o.dodgeGround.perSecond = 0.0f;
                    o.dodgeItem = u.id;
                }
                o.dodgeGround.radius = std::max(o.dodgeGround.radius, u.dodgeGround.radius);
                o.dodgeGround.duration = std::max(o.dodgeGround.duration, u.dodgeGround.duration);
                o.dodgeGround.perSecond += u.dodgeGround.perSecond * k;
            }
            if (u.proc.every > 0 and u.proc.damage > 0.0f)
            {
                if (o.procItem.empty())
                {
                    o.proc = u.proc;
                    o.proc.damage = 0.0f;
                    o.procItem = u.id;
                }
                o.proc.every = std::min(o.proc.every, u.proc.every);
                o.proc.chains = std::max(o.proc.chains, u.proc.chains);
                o.proc.damage += u.proc.damage * k;
            }
            if (u.landing.damage > 0.0f)
            {
                if (o.landingItem.empty())
                    o.landingItem = u.id;
                o.landing.radius = std::max(o.landing.radius, u.landing.radius);
                o.landing.damage += u.landing.damage * k;
            }

            // Bursts of the same status add up: their damage, the widest reach
            if (not u.burst.when.empty())
            {
                auto same = std::find_if(f.boon.bursts.begin(), f.boon.bursts.end(), [&](const UpgradeDef::Burst& b) { return b.when == u.burst.when; });
                if (same == f.boon.bursts.end())
                {
                    f.boon.bursts.push_back(u.burst);
                    f.boon.bursts.back().damage *= k;
                }
                else
                {
                    same->damage += u.burst.damage * k;
                    same->radius = std::max(same->radius, u.burst.radius);
                }
            }
        }

        // Stacked damage items help less and less (run.json damageFalloff)
        f.boon.damage = damageFalloff(f.boon.damage);
    }

    bool ArenaWorld::itemMaxed(const Fighter& f, const UpgradeDef& u) const
    {
        // The caps the game puts on them (armor 70%, cooldowns and costs 70% off for a stat item, 80% for a tag's, crit 100%)
        bool more = false;
        auto capped = [&](float amount, float has, float cap) {
            if (amount > 0.0f and has < cap - 1e-4f)
                more = true;
        };

        if (u.forTag())
        {
            const SpellTweak& b = u.boost;
            const SpellTweak has = tagBoost(f, {u.tag});
            capped(b.cooldown, has.cooldown, 0.8f);
            capped(b.cost, has.cost, 0.8f);
            more = more or b.damage > 0.0f or b.area > 0.0f or b.duration > 0.0f or b.speed > 0.0f or b.count > 0;
            return not more;
        }

        capped(u.armor, f.boon.armor, 0.7f);
        capped(u.cooldown, f.boon.cooldown, 0.7f);
        capped(u.manaCost, f.boon.manaCost, 0.7f);
        capped(u.crit, f.boon.crit, 1.0f);

        // Any other number always does a little more
        more = more or u.maxHealth > 0.0f or u.damage > 0.0f or u.dashCharges > 0 or u.manaRegen > 0.0f or u.staminaRegen > 0.0f or
               u.speed > 0.0f or u.lifeOnHit > 0.0f or u.statusStacks > 0 or u.pickupRange > 0.0f or u.regen > 0.0f or u.attackSpeed > 0.0f or
               u.horde > 0.0f or u.xpGain > 0.0f or u.shardChance > 0.0f or u.comboDamage > 0.0f or u.statusDuration > 0.0f or
               not u.payoff.against.empty() or u.burst.damage > 0.0f or u.healthScale > 0.0f or u.hurtDamage > 0.0f or u.airDamage > 0.0f or
               u.executeDamage > 0.0f or u.revengeDamage > 0.0f or u.thorns > 0.0f or u.killHeal > 0.0f or u.dodgeGround.perSecond > 0.0f or
               u.proc.damage > 0.0f or u.landing.damage > 0.0f;
        return not more;
    }

    float ArenaWorld::rarityPower(int rarity) const
    {
        const auto& rarities = content->run.rarities;
        return rarity >= 0 and rarity < static_cast<int>(rarities.size()) ? rarities[static_cast<size_t>(rarity)].power : 1.0f;
    }

    SpellRecipe ArenaWorld::spellAtLevel(const std::string& spellId, int level) const
    {
        auto it = content->spells.find(spellId);
        if (it == content->spells.end())
            return {};

        SpellRecipe r = it->second.recipe;
        for (int l = 2; l <= level and l - 2 < static_cast<int>(it->second.levels.size()); ++l)
            for (const auto& k : it->second.levels[static_cast<size_t>(l - 2)])
                r.keywords.push_back(k);

        return r;
    }

    SpellRecipe ArenaWorld::spellRecipe(const Fighter& f, int slot) const
    {
        if (not inRun())
            return slot >= 0 and slot < static_cast<int>(f.spells.size()) ? f.spells[static_cast<size_t>(slot)] : SpellRecipe{};

        if (slot < 0 or slot >= static_cast<int>(f.runSpells.size()))
            return {};

        return spellAt(f.runSpells[static_cast<size_t>(slot)], &f);
    }

    // ---- Growing: levels, branches, tags --------------------------------------------------------

    int ArenaWorld::branchAt(int writtenLevels) const
    {
        return std::max(content->run.branchLevel, 1 + writtenLevels + 1);
    }

    std::string ArenaWorld::offerTarget(const std::string& offer, int& branch)
    {
        const size_t hash = offer.rfind('#');
        branch = -1;

        if (hash == std::string::npos)
            return offer;

        branch = std::atoi(offer.c_str() + hash + 1);
        return offer.substr(0, hash);
    }

    const Fighter::RunSpell* ArenaWorld::runSkill(const Fighter& f, const std::string& abilityId) const
    {
        for (const auto& rs : f.runSkills)
            if (rs.id == abilityId)
                return &rs;
        return nullptr;
    }

    std::vector<std::string> ArenaWorld::spellTags(const SpellRecipe& recipe) const
    {
        std::vector<std::string> tags = {"spell", recipe.element};
        auto add = [&](const std::string& t) {
            if (std::find(tags.begin(), tags.end(), t) == tags.end())
                tags.push_back(t);
        };

        for (const auto& id : recipe.keywords)
        {
            auto k = content->keywords.find(id);
            if (k == content->keywords.end() or not k->second.form)
                continue;

            const std::string& shape = k->second.shape;
            if (shape == "bolt")
                add("projectile");
            else if (shape == "nova")
                add("area");
            else if (shape == "zone")
            {
                add("zone");
                add("area");
            }
            else if (shape == "beam")
                add("beam");
        }

        return tags;
    }

    std::vector<std::string> ArenaWorld::techniqueTags(const AbilityDef& a, int branch) const
    {
        std::vector<std::string> tags = {"technique"};
        auto add = [&](const std::string& t) {
            if (not t.empty() and std::find(tags.begin(), tags.end(), t) == tags.end())
                tags.push_back(t);
        };

        for (const auto& t : a.tags)
            add(t);
        if (a.attack.projectile.enabled)
            add("projectile");
        if (a.attack.element != Element::None)
            add(elementName(a.attack.element));

        if (branch >= 0 and branch < static_cast<int>(a.branches.size()))
        {
            const BranchDef& b = a.branches[static_cast<size_t>(branch)];
            for (const auto& t : b.tags)
                add(t);
            if (b.element != Element::None)
                add(elementName(b.element));
        }

        return tags;
    }

    SpellTweak ArenaWorld::tagBoost(const Fighter& f, const std::vector<std::string>& tags) const
    {
        SpellTweak t;

        for (const auto& b : f.boons)
        {
            auto it = content->upgrades.find(b.id);
            if (it == content->upgrades.end() or not it->second.forTag())
                continue;
            if (std::find(tags.begin(), tags.end(), it->second.tag) != tags.end())
                t.add(it->second.boost, rarityPower(b.rarity));
        }

        return t;
    }

    SpellRecipe ArenaWorld::spellAt(const Fighter::RunSpell& rs, const Fighter* f) const
    {
        auto it = content->spells.find(rs.id);
        if (it == content->spells.end())
            return {};

        const SpellDef& def = it->second;
        SpellRecipe r = spellAtLevel(rs.id, rs.level);
        const BranchDef* branch = rs.branch >= 0 and rs.branch < static_cast<int>(def.branches.size()) ? &def.branches[static_cast<size_t>(rs.branch)] : nullptr;

        // Past the written levels: the branch's keywords and boost, then a mastery boost for each other level
        int beyond = std::max(0, rs.level - def.maxLevel());
        if (branch)
        {
            r.keywords.insert(r.keywords.end(), branch->keywords.begin(), branch->keywords.end());
            r.tweak.add(branch->boost);
            r.payoff.add(branch->payoff);
            beyond = std::max(0, beyond - 1);
        }

        // Mastery and the tag items: their damage together, with diminishing returns
        SpellTweak extra;
        extra.add(content->run.mastery, static_cast<float>(beyond));

        if (f)
        {
            std::vector<std::string> tags = spellTags(r);
            if (branch)
                for (const auto& t : branch->tags)
                    if (std::find(tags.begin(), tags.end(), t) == tags.end())
                        tags.push_back(t);
            extra.add(tagBoost(*f, tags));
        }

        extra.damage = damageFalloff(extra.damage);
        r.tweak.add(extra);
        r.source = rs.id;
        return r;
    }

    SpellTweak ArenaWorld::techniqueBoost(const Fighter& f, const std::string& abilityId) const
    {
        SpellTweak t;
        auto it = content->abilities.find(abilityId);

        if (not grows() or f.team != Team::Player or it == content->abilities.end())
            return t;

        const AbilityDef& a = it->second;
        const Fighter::RunSpell* rs = runSkill(f, abilityId);
        const int level = rs ? rs->level : 1;
        const int branch = rs ? rs->branch : -1;

        // Its written levels, the branch, a mastery boost for each level past those
        const int written = std::min(level - 1, static_cast<int>(a.levels.size()));
        for (int l = 0; l < written; ++l)
            t.add(a.levels[static_cast<size_t>(l)]);

        int beyond = std::max(0, level - 1 - static_cast<int>(a.levels.size()));
        if (branch >= 0 and branch < static_cast<int>(a.branches.size()))
        {
            t.add(a.branches[static_cast<size_t>(branch)].boost);
            beyond = std::max(0, beyond - 1);
        }

        // Mastery and the tag items: their damage together, with diminishing returns
        SpellTweak extra;
        extra.add(content->run.mastery, static_cast<float>(beyond));
        extra.add(tagBoost(f, techniqueTags(a, branch)));
        extra.damage = damageFalloff(extra.damage);
        t.add(extra);
        return t;
    }

    void applyBoost(AttackDef& out, const SpellTweak& t)
    {
        const float damage = 1.0f + t.damage, area = 1.0f + t.area;

        out.damage *= damage;
        out.poiseDamage *= damage;
        out.projectile.damage *= damage;
        out.projectile.poiseDamage *= damage;
        out.heal.amount *= damage;
        out.healZone.perSecond *= damage;

        for (auto& box : out.hitboxes)
        {
            box.size.x *= area;
            box.size.z *= area;
        }
        out.heal.radius *= area;
        out.healZone.radius *= area;
        out.healZone.duration *= 1.0f + t.duration;
        out.projectile.speed *= 1.0f + t.speed;
        if (out.projectile.enabled)
            out.projectile.count = std::clamp(out.projectile.count + t.count, 1, 16);
    }

    void hasten(AttackDef& a, float speed)
    {
        if (speed <= 0.0f)
            return;

        const float k = 1.0f / (1.0f + speed);

        // As much less stamina as it is quicker: the same stamina a second, so attack speed is more damage, not a
        // faster way to run out
        a.staminaCost *= k;
        auto frames = [&](int& v, int least) {
            if (v >= 0)
                v = std::max(least, static_cast<int>(std::lround(static_cast<float>(v) * k)));
        };

        frames(a.startup, 1);
        frames(a.active, 1);
        frames(a.recovery, 0);
        frames(a.chainFrom, 0);
        frames(a.bufferFrom, 0);
        frames(a.dodgeCancelFrom, 0);
        frames(a.lungeFrom, 0);
        frames(a.lungeTo, 0);
        frames(a.moveUntil, 0);
        frames(a.iframeFrom, 0);
        frames(a.iframeTo, 0);
        frames(a.leapAt, 0);
        frames(a.turn.from, 0);
        frames(a.turn.to, 0);
        frames(a.charge.at, 0);
        frames(a.charge.frames, 0);
        a.lunge *= 1.0f + speed;
    }

    std::vector<std::string> ArenaWorld::attackTags(const Fighter& f, const AttackDef& attack) const
    {
        std::vector<std::string> tags = {"attack"};
        if (auto w = content->weapons.find(f.weapon); w != content->weapons.end())
        {
            if (not w->second.role.empty())
                tags.push_back(w->second.role);
            if (not w->second.type.empty())
                tags.push_back(w->second.type);
        }
        if (attack.projectile.enabled)
            tags.push_back("projectile");
        if (attack.element != Element::None)
            tags.push_back(elementName(attack.element));

        // A weapon's branch: its element, and the tags it adds
        if (const BranchDef* b = weaponBranch(f))
        {
            if (b->element != Element::None and attack.element == Element::None)
                tags.push_back(elementName(b->element));
            for (const auto& t : b->tags)
                if (std::find(tags.begin(), tags.end(), t) == tags.end())
                    tags.push_back(t);
        }
        return tags;
    }

    const BranchDef* ArenaWorld::weaponBranch(const Fighter& f) const
    {
        auto w = content->weapons.find(f.weapon);
        const Fighter::RunSpell* rs = grows() and f.team == Team::Player ? runSkill(f, f.weapon) : nullptr;

        if (w == content->weapons.end() or not rs or rs->branch < 0 or rs->branch >= static_cast<int>(w->second.branches.size()))
            return nullptr;

        return &w->second.branches[static_cast<size_t>(rs->branch)];
    }

    SpellTweak ArenaWorld::weaponBoost(const Fighter& f, const AttackDef& attack) const
    {
        SpellTweak t;
        auto it = content->weapons.find(f.weapon);

        if (not grows() or f.team != Team::Player or it == content->weapons.end())
            return t;

        const WeaponDef& w = it->second;
        const Fighter::RunSpell* rs = runSkill(f, w.id);
        const int level = rs ? rs->level : 1;

        // Its written levels, the branch, a mastery boost for each level past those
        const int written = std::min(level - 1, static_cast<int>(w.levels.size()));
        for (int l = 0; l < written; ++l)
            t.add(w.levels[static_cast<size_t>(l)]);

        int beyond = std::max(0, level - 1 - static_cast<int>(w.levels.size()));
        if (const BranchDef* b = weaponBranch(f))
        {
            t.add(b->boost);
            beyond = std::max(0, beyond - 1);
        }

        // Mastery and the tag items: their damage together, with diminishing returns
        SpellTweak extra;
        extra.add(content->run.mastery, static_cast<float>(beyond));
        extra.add(tagBoost(f, attackTags(f, attack)));
        extra.count = 0;   // more projectiles only from the weapon's own levels, not from items
        extra.damage = damageFalloff(extra.damage);
        t.add(extra);

        // The weapon's attacks take damage, area, projectile speed, attack speed and (from its own levels) projectiles
        t.cooldown = t.cost = t.duration = 0.0f;
        return t;
    }

    void applyBranch(AttackDef& out, const BranchDef* branch)
    {
        if (not branch)
            return;

        if (branch->element != Element::None)
        {
            out.element = branch->element;
            out.projectile.element = branch->element;
        }
        if (not branch->status.status.empty())
        {
            out.status = branch->status;
            out.projectile.status = branch->status;
        }
        out.payoff.add(branch->payoff);
        out.projectile.payoff.add(branch->payoff);
    }

    AttackDef ArenaWorld::boostedAttack(const AbilityDef& a, const SpellTweak& t, const BranchDef* branch) const
    {
        AttackDef out = a.attack;
        applyBoost(out, t);
        hasten(out, t.attackSpeed);
        applyBranch(out, branch);
        return out;
    }

    float ArenaWorld::priceScale(int wave) const
    {
        const RunDef& def = content->run;
        const float t = def.waves > 1 ? std::clamp(static_cast<float>(wave - 1) / static_cast<float>(def.waves - 1), 0.0f, 1.0f) : 0.0f;

        // Every player gets every shard, and more enemies come with more players: dearer to match
        const auto players = std::count_if(fighters.begin(), fighters.end(), [](const Fighter& f) { return f.team == Team::Player; });
        const float coop = 1.0f + def.coopPrice * static_cast<float>(std::max<std::ptrdiff_t>(0, players - 1));

        return (def.priceScale.x + (def.priceScale.y - def.priceScale.x) * t) * coop;
    }

    float ArenaWorld::enemyHealthScale(int wave) const
    {
        const DifficultyDef& d = difficulty();
        return d.health * (1.0f + (content->run.healthGrowth + d.healthGrowth) * static_cast<float>(std::max(0, wave - 1)));
    }

    float ArenaWorld::comboScale() const
    {
        return inRun() ? 1.0f + content->run.healthGrowth * static_cast<float>(std::max(0, run.wave - 1)) : 1.0f;
    }

    float ArenaWorld::enemyDamageScale(int wave) const
    {
        const DifficultyDef& d = difficulty();
        return d.damage * (1.0f + d.damageGrowth * static_cast<float>(std::max(0, wave - 1)));
    }

    float ArenaWorld::damageFalloff(float extra) const
    {
        const auto& d = content->run.damageFalloff;
        if (extra <= d.from)
            return extra;

        const float past = extra - d.from;
        return d.from + past / (1.0f + d.rate * past);
    }

    float ArenaWorld::teamHorde() const
    {
        float sum = 0.0f;
        for (const auto& f : fighters)
            if (f.team == Team::Player)
                sum += f.boon.horde;
        return sum;
    }

    float ArenaWorld::teamShardChance() const
    {
        float sum = 0.0f;
        for (const auto& f : fighters)
            if (f.team == Team::Player)
                sum += f.boon.shardChance;

        // Diminishing returns: a whole team of greed items does not multiply the shards
        return sum / (1.0f + content->run.shardFalloff * sum);
    }

    // ---- Starting, stopping ---------------------------------------------------------------------

    int ArenaWorld::difficultyChooser() const
    {
        const Fighter* chooser = nullptr;
        for (const auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            const bool host = f.owner == 0, chooserHost = chooser and chooser->owner == 0;
            if (not chooser or (host and not chooserHost) or (host == chooserHost and f.id < chooser->id))
                chooser = &f;
        }
        return chooser ? chooser->id : 0;
    }

    void ArenaWorld::chooseDifficulty(uint32_t seed)
    {
        const RunDef& def = content->run;
        if (def.levels() <= 1)
        {
            startRun(seed);
            return;
        }

        const int level = std::clamp(run.difficulty, 0, def.levels() - 1);
        const int open = run.levelsOpen;
        run = RunState{};
        run.seed = seed ? seed : 1u;
        run.difficulty = level;
        run.levelsOpen = open;
        run.phase = RunPhase::Difficulty;
        run.chooser = difficultyChooser();

        // Nobody to fight while it is chosen; whoever was down is back up
        removeEnemies();
        projectiles.clear();
        zones.clear();
        pickups.clear();
        pendingHits.clear();

        for (auto& f : fighters)
            if (f.team == Team::Player and not f.alive())
                respawn(f);
    }

    void ArenaWorld::startRun(uint32_t seed)
    {
        const RunDef& def = content->run;
        const int level = std::clamp(run.difficulty, 0, def.levels() - 1);
        const int open = run.levelsOpen;
        run = RunState{};
        run.seed = seed ? seed : 1u;
        run.difficulty = level;
        run.levelsOpen = open;
        run.waves = def.wavesAt(level);

        // A fresh arena (the run's own), nobody in it but the players, as new, with what a run starts with
        useRunArena = true;
        buildArena();
        projectiles.clear();
        zones.clear();
        pickups.clear();
        pendingHits.clear();
        removeEnemies();
        placePlayers();

        // inRun() is true from here: owns() asks what is owned
        run.phase = RunPhase::Fighting;

        for (auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            f.boons.clear();
            f.owned = def.startOwned;
            f.runSpells.clear();
            for (const auto& id : def.startSpells)
                if (f.runSpells.size() < SPELL_SLOTS)
                    f.runSpells.push_back({id, 1});

            if (content->weapons.count(def.startWeapon))
            {
                f.owned.push_back(def.startWeapon);
                f.weapon = def.startWeapon;
            }

            f.shards = f.xp = f.levelUps = f.rerolls = f.procHits = 0;
            f.levelRerolls = def.levelRerolls;
            f.banishes = def.levelBanishes;
            f.banished.clear();
            f.xpCarry = 0.0f;
            f.level = 1;
            f.shop.clear();
            f.levelOffer.clear();
            f.firstSpellOffer = rollFirstSpells();
            f.runSkills.clear();
            f.presents = f.banked = f.interest = 0;
            f.presentOffer.clear();
            f.stats = Fighter::RunStats{};
            f.ready = false;
            f.armed = not (def.chooseWeapon and content->weaponOrder.size() > 1);
            sumBoons(f);
            respawn(f);
        }

        event("run_start", arena().centre, 1.0f);

        // Each player picks a weapon first (when there is a choice), then a first spell (when there is one
        // to pick): the first wave waits for everyone
        if ((def.chooseWeapon and content->weaponOrder.size() > 1) or not def.startChoices.empty())
        {
            run.phase = RunPhase::Armory;
            run.wave = 1;
            return;
        }

        beginWave(1);
    }

    void ArenaWorld::chooseWeapon(Fighter& f, const std::string& id)
    {
        if (not content->weapons.count(id))
            return;

        // One weapon for the run: the one in hand before (the start kit's) goes
        f.owned.erase(std::remove_if(f.owned.begin(), f.owned.end(), [&](const std::string& o) { return content->weapons.count(o) > 0; }), f.owned.end());
        f.owned.push_back(id);
        f.weapon = id;
        f.armed = true;
        f.ready = content->run.startChoices.empty() or not f.runSpells.empty();
        event("weapon_pick", chest(f), 1.0f, f.id);
    }

    void ArenaWorld::chooseFirstSpell(Fighter& f, const std::string& id)
    {
        const auto& choices = content->run.startChoices;
        const auto& offered = f.firstSpellOffer.empty() ? choices : f.firstSpellOffer;
        if (std::find(offered.begin(), offered.end(), id) == offered.end())
            return;

        // Key 1's spell (one picked before is changed for it); the start kit's others stay after it
        f.runSpells.erase(std::remove_if(f.runSpells.begin(), f.runSpells.end(), [&](const Fighter::RunSpell& r) {
            return std::find(choices.begin(), choices.end(), r.id) != choices.end();
        }), f.runSpells.end());
        f.runSpells.insert(f.runSpells.begin(), {id, 1});
        if (f.runSpells.size() > SPELL_SLOTS)
            f.runSpells.resize(SPELL_SLOTS);

        f.ready = f.armed;
        event("weapon_pick", chest(f), 1.0f, f.id);
    }

    void ArenaWorld::stopRun()
    {
        const int level = run.difficulty, open = run.levelsOpen;
        const bool choosing = run.phase == RunPhase::Difficulty;
        run = RunState{};
        run.difficulty = level;
        run.levelsOpen = open;
        pickups.clear();

        for (auto& f : fighters)
        {
            f.boons.clear();
            f.runSpells.clear();
            f.owned.clear();
            f.shop.clear();
            f.levelOffer.clear();
            f.banished.clear();
            f.shards = f.xp = f.levelUps = f.levelRerolls = f.banishes = 0;
            f.level = 1;
            sumBoons(f);
        }

        // Back to the training arena
        if (useRunArena)
        {
            useRunArena = false;
            buildArena();
            projectiles.clear();
            zones.clear();
            placePlayers();

            for (auto& f : fighters)
                if (f.team == Team::Player)
                    respawn(f);
        }
        else if (choosing)
        {
            // Back from choosing the difficulty: the training ground as it was, its enemies back
            showArena(trainingArena);
        }
    }

    void ArenaWorld::skipToWave(int wave)
    {
        if (not inRun())
            return;

        // Still choosing the difficulty: the run starts at the one picked so far
        if (run.phase == RunPhase::Difficulty)
            startRun(run.seed);

        removeEnemies();
        run.bossId = 0;
        beginWave(std::clamp(wave, 1, std::max(1, run.waves)));
    }

    // ---- Waves ----------------------------------------------------------------------------------

    glm::vec3 ArenaWorld::edgePoint(float away)
    {
        const ArenaDef& where = arena();
        glm::vec3 best = where.centre;
        float bestDistance = -1.0f;

        // A few tries round the edge: the first far enough from the players, else the furthest
        for (int k = 0; k < 6; ++k)
        {
            const float a = static_cast<float>(runRandom() % 3600) * 0.1f * 0.0174533f;
            const float r = where.spawnRadius * (0.75f + 0.25f * static_cast<float>(runRandom() % 1000) / 1000.0f);
            glm::vec3 p = where.centre + glm::vec3(std::cos(a), 0.0f, std::sin(a)) * r;
            p.x = std::clamp(p.x, 2.0f, static_cast<float>(grid.width()) - 3.0f);
            p.z = std::clamp(p.z, 2.0f, static_cast<float>(grid.depth()) - 3.0f);

            // On the ground there
            const RayHit down = raycast(grid, content->blockTable, glm::vec3(p.x, static_cast<float>(grid.height()) - 1.5f, p.z), glm::vec3(0, -1, 0), static_cast<float>(grid.height()));
            p.y = down.hit ? static_cast<float>(down.cell.y) + 1.0f : where.centre.y;

            float nearest = 1e9f;
            for (const auto& f : fighters)
                if (f.team == Team::Player and f.alive())
                    nearest = std::min(nearest, glm::length(p - f.body.position));

            if (nearest > bestDistance)
            {
                bestDistance = nearest;
                best = p;
            }

            if (nearest >= away)
                break;
        }

        return best;
    }

    glm::vec3 ArenaWorld::groundNear(glm::vec3 p) const
    {
        const int x = static_cast<int>(std::floor(p.x)), z = static_cast<int>(std::floor(p.z));
        const auto& table = content->blockTable;

        // The nearest floor with room to stand, from a little above the point down, then up from there
        const int start = std::clamp(static_cast<int>(std::floor(p.y)) + 3, 1, grid.height() - 2);
        auto standable = [&](int y) { return grid.isSolid(table, x, y - 1, z) and not grid.isSolid(table, x, y, z) and not grid.isSolid(table, x, y + 1, z); };

        for (int y = start; y >= 1; --y)
            if (standable(y))
                return glm::vec3(p.x, static_cast<float>(y), p.z);

        for (int y = start + 1; y < grid.height() - 1; ++y)
            if (standable(y))
                return glm::vec3(p.x, static_cast<float>(y), p.z);

        return p;
    }

    void ArenaWorld::beginWave(int wave)
    {
        const RunDef& def = content->run;

        if (wave < 1 or wave > run.waves)
            return;

        run.wave = wave;
        run.phase = RunPhase::Fighting;
        run.timer = waveDuration(wave);
        run.pending.clear();
        spawnsDue.clear();
        zonesDue.clear();
        run.spawnDue = 0.0f;
        run.bossId = 0;
        pickups.clear();

        // A fresh arena for each wave after the first: what was broken is whole again, everyone back in place.
        // Another arena from this wave on: built whatever resetEachWave says, everyone at its spawn point
        const bool newArena = content->runArenaId(wave, false) != arenaId;

        if ((wave > 1 and def.resetEachWave) or newArena)
        {
            buildArena();
            projectiles.clear();
            zones.clear();
            pendingHits.clear();

            if (newArena)
                placePlayers();

            for (auto& f : fighters)
            {
                if (f.team != Team::Player or not f.alive())
                    continue;

                f.body.position = f.spawnPosition;
                f.body.velocity = glm::vec3(0.0f);
                f.push = glm::vec3(0.0f);
                f.yaw = f.spawnYaw;
            }

            event(newArena ? "arena_change" : "arena_reset", arena().centre, 1.0f);
        }

        // The ones down get back up for the new wave
        for (auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            f.ready = false;
            f.banked = f.shards;   // what earns the interest at the next shop

            if (not f.alive())
                respawn(f);
        }

        run.presentsDropped = 0;

        // A boss wave: it comes first, from far off, and the wave lasts until it falls
        auto boss = def.bosses.find(wave);
        run.bossWave = boss != def.bosses.end() and content->enemies.count(boss->second);

        if (run.bossWave)
        {
            run.pending.push_back({boss->second, edgePoint(10.0f), 2.0f, true, false});
            event("boss_warn", run.pending.back().position, 2.0f);
        }

        event("wave_start", arena().centre, static_cast<float>(wave));
    }

    void ArenaWorld::endWave()
    {
        const RunDef& def = content->run;

        // Whoever is left goes, the shards still lying about go to everyone (picked up by the nearest player)
        for (const auto& f : fighters)
            if (f.team == Team::Enemy and f.alive())
                event("despawn", chest(f), 1.0f);

        removeEnemies();
        run.pending.clear();
        spawnsDue.clear();   // what split as the wave ended does not come in after it
        run.bossId = 0;
        run.enemiesLeft = 0;

        for (const auto& p : pickups)
        {
            Fighter* nearest = nullptr;
            float best = 1e9f;

            for (auto& f : fighters)
                if (f.team == Team::Player and glm::length(f.body.position - p.position) < best)
                {
                    best = glm::length(f.body.position - p.position);
                    nearest = &f;
                }

            if (nearest and p.present)
            {
                ++nearest->presents;
                ++nearest->stats.presents;
            }
            else if (nearest)
                shareShards(*nearest, p.value);
        }

        pickups.clear();
        run.score += def.waveScore * run.wave;

        if (run.wave >= run.waves)
        {
            run.score += def.victoryScore;

            // Two players or more: a final duel first
            const auto players = std::count_if(fighters.begin(), fighters.end(), [](const Fighter& f) { return f.team == Team::Player; });
            if (def.duel.enabled and players >= 2)
            {
                beginDuel();
                return;
            }

            run.phase = RunPhase::Victory;
            event("run_victory", arena().centre, static_cast<float>(run.score));
            return;
        }

        // The shop: each player's own, its level ups first; nothing taken for a moment (no misclicks
        // carried over from the fight)
        run.phase = RunPhase::Shop;
        run.timer = def.intermission;

        for (auto& f : fighters)
        {
            if (f.team != Team::Player)
                continue;

            f.ready = false;
            f.rerolls = 0;

            // Interest: the shards kept through the last shop grow as the prices did, so what could be bought then still can
            f.interest = 0;
            if (def.interest and f.banked > 0 and run.wave > 1)
            {
                const float growth = priceScale(run.wave) / std::max(0.01f, priceScale(run.wave - 1)) - 1.0f;
                f.interest = std::max(0, static_cast<int>(std::ceil(static_cast<float>(f.banked) * growth - 1e-4f)));
                f.shards += f.interest;
            }

            rollShop(f);
            rollLevel(f);
            rollPresent(f);
        }

        event("wave_clear", arena().centre, static_cast<float>(run.wave));
    }

    void ArenaWorld::beginDuel()
    {
        const RunDef& def = content->run;

        run.phase = RunPhase::Duel;
        run.timer = def.duel.duration;
        run.warmup = def.duel.countdown;
        run.champion = 0;
        run.bossWave = false;
        run.bossId = 0;
        run.pending.clear();

        // The arena anew, nothing left in it but the players
        buildArena();
        projectiles.clear();
        zones.clear();
        pendingHits.clear();
        pickups.clear();
        removeEnemies();

        // Everyone back up, in a ring around the middle, facing it
        std::vector<Fighter*> players;
        for (auto& f : fighters)
            if (f.team == Team::Player)
                players.push_back(&f);

        for (size_t k = 0; k < players.size(); ++k)
        {
            Fighter& f = *players[k];
            const float angle = static_cast<float>(k) * 2.0f * 3.14159265f / static_cast<float>(players.size());
            f.spawnPosition = groundNear(arena().centre + glm::vec3(std::sin(angle), 0.0f, std::cos(angle)) * def.duel.radius);
            const glm::vec3 toCentre = arena().centre - f.spawnPosition;
            f.spawnYaw = glm::degrees(std::atan2(toCentre.x, toCentre.z));
            f.ready = false;
            f.statuses.clear();
            respawn(f);
        }

        event("duel_start", arena().centre, static_cast<float>(players.size()));
    }

    // ---- Shards ---------------------------------------------------------------------------------

    void ArenaWorld::spawnShards(const Fighter& from, int value)
    {
        if (value <= 0)
            return;

        // A few shards, popping out
        const int count = std::clamp(value, 1, 4);

        for (int k = 0; k < count; ++k)
        {
            Pickup p;
            p.id = nextPickup++;
            p.value = value / count + (k < value % count ? 1 : 0);
            p.position = chest(from);
            const float a = static_cast<float>(runRandom() % 628) * 0.01f;
            p.velocity = glm::vec3(std::cos(a) * 2.0f, 4.0f, std::sin(a) * 2.0f);
            pickups.push_back(p);
        }

        // Too many lying about: the oldest shards go into the next ones (presents stay as they are)
        while (pickups.size() > 160)
        {
            auto first = std::find_if(pickups.begin(), pickups.end(), [](const Pickup& p) { return not p.present; });
            auto next = first == pickups.end() ? first : std::find_if(first + 1, pickups.end(), [](const Pickup& p) { return not p.present; });
            if (next == pickups.end())
                break;
            next->value += first->value;
            pickups.erase(first);
        }
    }

    void ArenaWorld::shareShards(Fighter& picker, int value)
    {
        for (auto& f : fighters)
            if (f.team == Team::Player)
                collect(f, value, f.id == picker.id);
    }

    void ArenaWorld::collect(Fighter& f, int value, bool pickedUp)
    {
        f.shards += value;
        if (pickedUp)
            f.stats.shards += value;

        // More XP per shard comes in parts: what is short of a whole one waits for the next shard
        f.xpCarry += static_cast<float>(value) * (1.0f + std::max(0.0f, f.boon.xpGain));
        const int whole = static_cast<int>(f.xpCarry + 1e-4f);
        f.xpCarry = std::max(0.0f, f.xpCarry - static_cast<float>(whole));
        f.xp += whole;
        f.stats.xp += whole;

        while (f.xp >= xpFor(f.level))
        {
            f.xp -= xpFor(f.level);
            ++f.level;
            ++f.levelUps;
            event("level_up", chest(f), static_cast<float>(f.level), f.id);
        }
    }

    void ArenaWorld::stepPickups()
    {
        const float dt = STEP_SECONDS;

        for (size_t i = 0; i < pickups.size();)
        {
            Pickup& p = pickups[i];
            p.age += dt;

            // Who it goes to: the nearest player that has it in range (once it has popped out)
            if (p.target == 0 and p.age > 0.3f)
            {
                float best = 1e9f;

                for (const auto& f : fighters)
                {
                    if (f.team != Team::Player or not f.alive())
                        continue;

                    const float d = glm::length(chest(f) - p.position);

                    if (d <= content->run.pickupRange + f.boon.pickupRange and d < best)
                    {
                        best = d;
                        p.target = f.id;
                    }
                }
            }

            Fighter* to = p.target ? find(p.target) : nullptr;

            if (to and to->alive())
            {
                // Faster as it comes
                const glm::vec3 d = chest(*to) - p.position;
                const float distance = glm::length(d);

                if (distance < 0.6f)
                {
                    const int value = p.value;
                    const bool present = p.present;
                    const glm::vec3 at = p.position;
                    pickups.erase(pickups.begin() + static_cast<std::ptrdiff_t>(i));

                    if (present)
                    {
                        ++to->presents;
                        ++to->stats.presents;
                        event("present", at, 1.0f, to->id);
                    }
                    else
                    {
                        shareShards(*to, value);
                        event("pickup", at, static_cast<float>(value), to->id);
                    }
                    continue;
                }

                const float speed = 6.0f + 18.0f * std::min(1.0f, p.age);
                p.velocity = d / distance * speed;
                p.position += p.velocity * std::min(dt, distance / speed);
            }
            else
            {
                p.target = 0;

                // Popped out, then down to the ground and still
                p.velocity.y -= 18.0f * dt;
                p.velocity.x *= 0.92f;
                p.velocity.z *= 0.92f;
                glm::vec3 next = p.position + p.velocity * dt;
                const float foot = next.y - 0.15f;

                if (foot < 0.0f or grid.isSolid(content->blockTable, static_cast<int>(std::floor(next.x)), static_cast<int>(std::floor(foot)), static_cast<int>(std::floor(next.z))))
                {
                    next.y = std::floor(std::max(foot, 0.0f)) + 1.15f;
                    p.velocity = glm::vec3(0.0f);
                }

                p.position = next;
            }

            ++i;
        }
    }

    // ---- The shop and level ups -----------------------------------------------------------------

    std::vector<Fighter::ShopOffer> ArenaWorld::rollItems(const Fighter& f, int n, const std::vector<Fighter::ShopOffer>& kept, bool unlocks)
    {
        const RunDef& def = content->run;
        const float scale = priceScale(run.wave);

        auto offered = [&](const std::string& id) {
            return std::any_of(kept.begin(), kept.end(), [&](const Fighter::ShopOffer& o) { return o.id == id; });
        };
        auto taken = [&](const std::string& id) {
            return static_cast<int>(std::count_if(f.boons.begin(), f.boons.end(), [&](const Fighter::Boon& b) { return b.id == id; }));
        };

        // A tag item only when something of the player has its tag: it would do nothing otherwise
        std::vector<std::string> tags;
        for (const auto& rs : f.runSpells)
            for (const auto& t : spellTags(spellAt(rs)))
                tags.push_back(t);
        for (const auto& rs : f.runSkills)
            if (auto a = content->abilities.find(rs.id); a != content->abilities.end())
                for (const auto& t : techniqueTags(a->second, rs.branch))
                    tags.push_back(t);
        if (const Moveset* m = movesetOf(f))
            for (const auto& [name, attack] : m->attacks)
                for (const auto& t : attackTags(f, attack))
                    tags.push_back(t);

        // What can be offered: items not at their most, unlocks (a mod's: the base run has none) not owned, nothing twice
        struct Open { std::string id; int price; bool item; };
        std::vector<Open> open;

        for (const auto& [id, u] : content->upgrades)
        {
            if ((u.most > 0 and taken(id) >= u.most) or offered(id) or itemMaxed(f, u))
                continue;
            if (u.forTag() and std::find(tags.begin(), tags.end(), u.tag) == tags.end())
                continue;

            // An item for statuses or combos: only with what makes them (a fire spell for one against burning...)
            if (not u.needs.empty() and std::none_of(u.needs.begin(), u.needs.end(), [&](const std::string& t) { return std::find(tags.begin(), tags.end(), t) != tags.end(); }))
                continue;
            if (u.needsElements > 0 and std::count_if(ELEMENT_TAGS.begin(), ELEMENT_TAGS.end(), [&](const char* e) { return std::find(tags.begin(), tags.end(), e) != tags.end(); }) < u.needsElements)
                continue;
            open.push_back({id, u.price, true});
        }

        if (unlocks)
        {
            // The weapon was chosen at the start: no weapons, and only the skills of the one in hand
            const auto weapon = content->weapons.find(f.weapon);
            const std::string type = weapon != content->weapons.end() ? weapon->second.type : "";

            auto forSale = [&](const std::string& id) {
                if (def.chooseWeapon and content->weapons.count(id))
                    return false;

                if (auto a = content->abilities.find(id); a != content->abilities.end() and not a->second.weapons.empty())
                    return std::find(a->second.weapons.begin(), a->second.weapons.end(), type) != a->second.weapons.end();

                return true;
            };

            for (const auto& [id, price] : def.unlocks)
                if (not owns(f, id) and not offered(id) and forSale(id))
                    open.push_back({id, price, false});
        }

        for (size_t i = open.size(); i > 1; --i)
            std::swap(open[i - 1], open[runRandom() % i]);

        // Each item's rarity, among the ones it comes at: rarer ones come up more as the waves go on
        const int rarities = static_cast<int>(def.rarities.size());
        auto rollRarity = [&](const UpgradeDef& u) {
            auto weight = [&](int k) { return u.comesAt(def.rarities[static_cast<size_t>(k)].name) ? def.rarityWeight(k, run.wave) : 0.0f; };

            float total = 0.0f;
            int first = -1;
            for (int k = 0; k < rarities; ++k)
            {
                total += weight(k);
                if (first < 0 and u.comesAt(def.rarities[static_cast<size_t>(k)].name))
                    first = k;
            }
            if (total <= 0.0f)
                return std::max(0, first);

            float roll = static_cast<float>(runRandom() % 100000) / 100000.0f * total;
            for (int k = 0; k < rarities; ++k)
            {
                roll -= weight(k);
                if (roll < 0.0f and weight(k) > 0.0f)
                    return k;
            }
            return std::max(0, first);
        };

        std::vector<Fighter::ShopOffer> out;

        for (const auto& o : open)
        {
            if (static_cast<int>(out.size()) >= n)
                break;

            const int rarity = o.item ? rollRarity(content->upgrades.at(o.id)) : 0;
            const float rarityPrice = o.item and rarity < static_cast<int>(def.rarities.size()) ? def.rarities[static_cast<size_t>(rarity)].price : 1.0f;
            Fighter::ShopOffer offer;
            offer.id = o.id;
            offer.price = std::max(1, static_cast<int>(std::lround(static_cast<float>(o.price) * rarityPrice * scale)));
            offer.rarity = rarity;
            out.push_back(offer);
        }

        return out;
    }

    void ArenaWorld::rollShop(Fighter& f)
    {
        // Locked offers stay; the rest is rolled again
        std::vector<Fighter::ShopOffer> kept;
        for (const auto& o : f.shop)
            if (o.locked and not o.bought)
                kept.push_back(o);

        f.shop = kept;
        for (const auto& o : rollItems(f, content->run.shopSlots - static_cast<int>(kept.size()), kept, true))
            f.shop.push_back(o);
    }

    void ArenaWorld::rollPresent(Fighter& f)
    {
        f.presentOffer.clear();

        if (f.presents <= 0)
            return;

        f.presentOffer = rollItems(f, content->run.presentChoices, {}, false);

        // Nothing left to give
        if (f.presentOffer.empty())
            f.presents = 0;
    }

    void ArenaWorld::levelCandidates(Fighter& f, std::vector<std::string>& fresh, std::vector<std::vector<std::string>>& upgrades)
    {
        // What is new (a spell not had while a key is free, a technique of the weapon in hand not had) and what
        // grows (the next level of a spell, a technique or the weapon had: at its branch level, all its branches
        // together, as one choice). Nothing banished
        fresh.clear();
        upgrades.clear();
        auto banished = [&](const std::string& id) { return std::find(f.banished.begin(), f.banished.end(), id) != f.banished.end(); };

        auto grow = [&](const std::string& id, int level, int branch, int written, size_t branches) {
            if (banished(id))
                return;
            std::vector<std::string> choice;
            if (level + 1 == branchAt(written) and branches > 0 and branch < 0)
            {
                for (size_t b = 0; b < branches; ++b)
                    choice.push_back(id + "#" + std::to_string(b));
            }
            else
            {
                choice.push_back(id);
            }
            upgrades.push_back(choice);
        };

        for (const auto& entry : content->spells)
        {
            const std::string& id = entry.first;
            const SpellDef& sp = entry.second;
            auto has = std::find_if(f.runSpells.begin(), f.runSpells.end(), [&](const Fighter::RunSpell& r) { return r.id == id; });

            if (has == f.runSpells.end())
            {
                if (f.runSpells.size() < SPELL_SLOTS and not banished(id))
                    fresh.push_back(id);
            }
            else
            {
                grow(id, has->level, has->branch, static_cast<int>(sp.levels.size()), sp.branches.size());
            }
        }

        const auto weapon = content->weapons.find(f.weapon);
        const std::string type = weapon != content->weapons.end() ? weapon->second.type : "";

        // The weapon in hand: it starts at level 1, and grows like a technique
        if (weapon != content->weapons.end())
        {
            const Fighter::RunSpell* rs = runSkill(f, weapon->first);
            grow(weapon->first, rs ? rs->level : 1, rs ? rs->branch : -1, static_cast<int>(weapon->second.levels.size()), weapon->second.branches.size());
        }

        for (const auto& id : content->run.levelSkills)
        {
            auto a = content->abilities.find(id);
            if (a == content->abilities.end())
                continue;
            if (not a->second.weapons.empty() and std::find(a->second.weapons.begin(), a->second.weapons.end(), type) == a->second.weapons.end())
                continue;

            if (const Fighter::RunSpell* rs = runSkill(f, id))
                grow(id, rs->level, rs->branch, static_cast<int>(a->second.levels.size()), a->second.branches.size());
            else if (not banished(id))
                fresh.push_back(id);
        }

        for (size_t i = fresh.size(); i > 1; --i)
            std::swap(fresh[i - 1], fresh[runRandom() % i]);
        for (size_t i = upgrades.size(); i > 1; --i)
            std::swap(upgrades[i - 1], upgrades[runRandom() % i]);
    }

    void ArenaWorld::rollLevel(Fighter& f)
    {
        f.levelOffer.clear();

        if (f.levelUps <= 0)
            return;

        std::vector<std::string> fresh;
        std::vector<std::vector<std::string>> upgrades;
        levelCandidates(f, fresh, upgrades);

        // Nothing left to give: the level ups are spent
        if (fresh.empty() and upgrades.empty())
        {
            f.levelUps = 0;
            return;
        }

        // One thing had growing, the rest new while there is something new; whatever is short, the other fills
        std::vector<std::string> picks;
        size_t nextFresh = 0, nextUpgrade = 0;
        auto takeUpgrade = [&] {
            for (const auto& o : upgrades[nextUpgrade++])
                picks.push_back(o);
        };

        if (nextUpgrade < upgrades.size())
            takeUpgrade();
        for (int k = 1; k < content->run.levelChoices and nextFresh < fresh.size(); ++k)
            picks.push_back(fresh[nextFresh++]);
        while (static_cast<int>(picks.size()) < content->run.levelChoices and (nextFresh < fresh.size() or nextUpgrade < upgrades.size()))
        {
            if (nextUpgrade < upgrades.size())
                takeUpgrade();
            else
                picks.push_back(fresh[nextFresh++]);
        }

        f.levelOffer = picks;
    }

    void ArenaWorld::banishPick(Fighter& f, size_t index)
    {
        if (index >= f.levelOffer.size())
            return;

        // A branch is no pick to banish: it is what the spell or technique grows into
        int branch = -1;
        const std::string id = offerTarget(f.levelOffer[index], branch);
        if (branch >= 0)
            return;

        f.banished.push_back(id);
        --f.banishes;

        // In its place: another of the same kind (new or growing) not offered yet, else of the other kind, else none
        std::vector<std::string> fresh;
        std::vector<std::vector<std::string>> upgrades;
        levelCandidates(f, fresh, upgrades);
        auto offered = [&](const std::string& o) {
            return std::any_of(f.levelOffer.begin(), f.levelOffer.end(), [&](const std::string& x) {
                int b = -1;
                return offerTarget(x, b) == o;
            });
        };

        // It was new when nothing of it is had (not a spell or technique growing, not the weapon)
        const bool wasNew = std::none_of(f.runSpells.begin(), f.runSpells.end(), [&](const Fighter::RunSpell& r) { return r.id == id; }) and
                            runSkill(f, id) == nullptr and not content->weapons.count(id);

        std::string next;
        auto takeFresh = [&] {
            for (const auto& o : fresh)
                if (not offered(o))
                {
                    next = o;
                    return;
                }
        };
        // A growing one only as one card (a branch choice would take several)
        auto takeGrowing = [&] {
            for (const auto& u : upgrades)
                if (u.size() == 1 and not offered(u.front()))
                {
                    next = u.front();
                    return;
                }
        };

        if (wasNew)
        {
            takeFresh();
            if (next.empty())
                takeGrowing();
        }
        else
        {
            takeGrowing();
            if (next.empty())
                takeFresh();
        }

        if (next.empty())
            f.levelOffer.erase(f.levelOffer.begin() + static_cast<std::ptrdiff_t>(index));
        else
            f.levelOffer[index] = next;

        // Nothing at all left to pick: the level ups are spent
        if (f.levelOffer.empty())
            f.levelUps = 0;
    }

    void ArenaWorld::takeLevel(Fighter& f, const std::string& offer)
    {
        int branch = -1;
        const std::string id = offerTarget(offer, branch);

        if (content->spells.count(id))
        {
            auto has = std::find_if(f.runSpells.begin(), f.runSpells.end(), [&](const Fighter::RunSpell& r) { return r.id == id; });

            if (has != f.runSpells.end())
            {
                ++has->level;
                if (branch >= 0 and branch < static_cast<int>(content->spells.at(id).branches.size()))
                    has->branch = branch;
            }
            else if (f.runSpells.size() < SPELL_SLOTS)
            {
                f.runSpells.push_back({id, 1});
            }
        }
        else if (auto w = content->weapons.find(id); w != content->weapons.end())
        {
            // A weapon's levels go with the techniques: it starts at level 1, so its first level up is level 2
            auto has = std::find_if(f.runSkills.begin(), f.runSkills.end(), [&](const Fighter::RunSpell& r) { return r.id == id; });

            if (has == f.runSkills.end())
            {
                f.runSkills.push_back({id, 1});
                has = f.runSkills.end() - 1;
            }

            ++has->level;
            if (branch >= 0 and branch < static_cast<int>(w->second.branches.size()))
                has->branch = branch;
        }
        else if (content->abilities.count(id))
        {
            auto has = std::find_if(f.runSkills.begin(), f.runSkills.end(), [&](const Fighter::RunSpell& r) { return r.id == id; });

            if (has != f.runSkills.end())
            {
                ++has->level;
                if (branch >= 0 and branch < static_cast<int>(content->abilities.at(id).branches.size()))
                    has->branch = branch;
            }
            else
            {
                f.runSkills.push_back({id, 1});
            }

            if (not owns(f, id))
                f.owned.push_back(id);
        }
    }

    void ArenaWorld::takeBoon(Fighter& f, const Fighter::Boon& boon)
    {
        const float before = f.maxHealth;
        f.boons.push_back(boon);
        sumBoons(f);

        // More health: there at once; less (a trade-off): what is over the new most goes
        f.maxHealth = playerMaxHealth(f);
        const float more = f.maxHealth - before;
        f.health = std::min(f.maxHealth, f.health + std::max(0.0f, more));
    }

    int ArenaWorld::rerollPrice(const Fighter& f) const
    {
        const RunDef& def = content->run;
        // Dearer with more players, as the items are (priceScale)
        const auto players = std::count_if(fighters.begin(), fighters.end(), [](const Fighter& o) { return o.team == Team::Player; });
        const float coop = 1.0f + def.coopPrice * static_cast<float>(std::max<std::ptrdiff_t>(0, players - 1));
        return static_cast<int>(std::lround(static_cast<float>(def.rerollPrice + def.rerollStep * f.rerolls + run.wave / 2) * coop));
    }

    void ArenaWorld::shopPick(Fighter& f, int pick)
    {
        const int offers = static_cast<int>(f.shop.size());

        // The moment after a wave: nothing taken yet
        if (run.timer > 0.0f)
            return;

        // The level ups come first, then the presents: the shop waits for them
        const bool levelling = f.levelUps > 0 and not f.levelOffer.empty();
        const int picks = static_cast<int>(f.levelOffer.size());
        if (levelling and not ((pick >= InputFrame::LEVEL and pick < InputFrame::LEVEL + picks) or pick == InputFrame::LEVEL_REROLL or
                               (pick >= InputFrame::BANISH and pick < InputFrame::BANISH + picks)))
            return;

        if (levelling and pick == InputFrame::LEVEL_REROLL)
        {
            if (f.levelRerolls > 0)
            {
                --f.levelRerolls;
                rollLevel(f);
                event("level_reroll", chest(f), 1.0f, f.id);
            }
            return;
        }

        if (levelling and pick >= InputFrame::BANISH)
        {
            if (f.banishes > 0)
            {
                const size_t before = f.banished.size();
                banishPick(f, static_cast<size_t>(pick - InputFrame::BANISH));
                if (f.banished.size() > before)
                    event("banish", chest(f), 1.0f, f.id);
                if (f.levelUps <= 0)
                    rollPresent(f);
            }
            return;
        }

        const bool opening = not levelling and f.presents > 0 and not f.presentOffer.empty();
        if (opening and not (pick >= InputFrame::PRESENT and pick < InputFrame::PRESENT + static_cast<int>(f.presentOffer.size())))
            return;

        if (opening)
        {
            const auto& o = f.presentOffer[static_cast<size_t>(pick - InputFrame::PRESENT)];
            takeBoon(f, {o.id, o.rarity});
            --f.presents;
            rollPresent(f);
            event("buy", chest(f), 1.0f, f.id);
            return;
        }

        if (pick >= InputFrame::BUY and pick < InputFrame::BUY + offers)
        {
            auto& o = f.shop[static_cast<size_t>(pick - InputFrame::BUY)];

            if (o.bought or f.shards < o.price)
                return;

            f.shards -= o.price;
            o.bought = true;
            o.locked = false;
            ++f.stats.items;

            if (content->upgrades.count(o.id))
            {
                takeBoon(f, {o.id, o.rarity});
            }
            else
            {
                f.owned.push_back(o.id);

                // A weapon bought is in hand at once
                if (content->weapons.count(o.id))
                    f.weapon = o.id;
            }

            event("buy", chest(f), 1.0f, f.id);
        }
        else if (pick >= InputFrame::LOCK and pick < InputFrame::LOCK + offers)
        {
            auto& o = f.shop[static_cast<size_t>(pick - InputFrame::LOCK)];

            if (not o.bought)
                o.locked = not o.locked;
        }
        else if (pick == InputFrame::REROLL)
        {
            const int price = rerollPrice(f);

            if (f.shards < price)
                return;

            f.shards -= price;
            ++f.rerolls;
            rollShop(f);
        }
        else if (pick >= InputFrame::LEVEL and pick < InputFrame::LEVEL + static_cast<int>(f.levelOffer.size()))
        {
            takeLevel(f, f.levelOffer[static_cast<size_t>(pick - InputFrame::LEVEL)]);
            --f.levelUps;
            rollLevel(f);

            // Done levelling: the presents' picks next (their items may now reach what was just taken)
            if (f.levelUps <= 0)
                rollPresent(f);
            event("boon", chest(f), 1.0f, f.id);
        }
        else if (pick == InputFrame::START)
        {
            f.ready = not f.ready;
        }
    }

    // ---- Each step ------------------------------------------------------------------------------

    void ArenaWorld::stepRun(const std::map<int, InputFrame>& inputs)
    {
        const RunDef& def = content->run;
        const float dt = STEP_SECONDS;

        auto pickOf = [&](const Fighter& f) {
            auto it = inputs.find(f.id);
            return it != inputs.end() ? it->second.pick : 0;
        };

        // Anyone's START: a run from the start (its difficulty first)
        auto startAgain = [&]() {
            for (const auto& f : fighters)
            {
                if (f.team == Team::Player and pickOf(f) == InputFrame::START)
                {
                    chooseDifficulty(runRandom() ^ static_cast<uint32_t>(steps * 2654435761u + 1u));
                    return;
                }
            }
        };

        if (run.phase == RunPhase::Fighting or run.phase == RunPhase::Shop or run.phase == RunPhase::Duel)
            run.elapsed += dt;

        int players = 0, standing = 0;
        for (const auto& f : fighters)
            if (f.team == Team::Player)
            {
                ++players;
                standing += f.alive() ? 1 : 0;
            }

        switch (run.phase)
        {
            case RunPhase::Off:
                // Asked for (a dedicated server has no title screen)
                startAgain();
                break;

            case RunPhase::Fighting:
            {
                stepPickups();

                if (players > 0 and standing == 0)
                {
                    run.phase = RunPhase::Defeat;
                    pickups.clear();
                    event("run_defeat", arena().centre, static_cast<float>(run.wave));
                    break;
                }

                run.timer = std::max(0.0f, run.timer - dt);

                // The ones warned of come out, facing the middle
                for (size_t i = 0; i < run.pending.size();)
                {
                    auto& s = run.pending[i];
                    s.in -= dt;

                    if (s.in > 0.0f)
                    {
                        ++i;
                        continue;
                    }

                    glm::vec3 toCentre = arena().centre - s.position;
                    toCentre.y = 0.0f;
                    const int id = spawnEnemy(s.enemy, s.position, glm::length(toCentre) > 0.1f ? glm::degrees(std::atan2(toCentre.x, toCentre.z)) : 0.0f);

                    if (Fighter* e = find(id))
                    {
                        if (s.boss)
                            run.bossId = id;

                        // Tougher as the run goes on, and at the harder levels; a boss with more players too
                        e->maxHealth *= enemyHealthScale(run.wave);
                        if (s.boss)
                            e->maxHealth *= 1.0f + def.bossCoopHealth * static_cast<float>(std::max(0, players - 1));
                        e->health = e->maxHealth;

                        // An elite: tougher, harder hitting (applyHit), worth more, and with its affixes
                        if (s.elite)
                        {
                            e->elite = true;
                            e->affixes = rollAffixes(def.eliteAffixesAt(run.wave));
                            e->maxHealth *= 2.5f * (1.0f + affixSum(*e, &AffixDef::health));
                            e->health = e->maxHealth;
                            e->maxPoise *= 1.5f;
                            e->poise = e->maxPoise;
                        }
                    }

                    event(s.boss ? "boss_spawn" : "spawn", s.position, 1.0f, id);
                    run.pending.erase(run.pending.begin() + static_cast<std::ptrdiff_t>(i));
                }

                run.enemiesLeft = 0;
                for (const auto& f : fighters)
                    run.enemiesLeft += f.team == Team::Enemy and f.alive() and not f.gone ? 1 : 0;

                // More of them all the while, faster as the run goes on, with more players, at the harder levels
                // and with the players' horde items; half as many with a boss. The run ramps up over all its
                // written waves: a shorter level stops partway up
                const float t = def.waves > 1 ? static_cast<float>(run.wave - 1) / static_cast<float>(def.waves - 1) : 0.0f;
                const float coop = def.spawnScale * (1.0f + def.coopScale * static_cast<float>(std::max(0, players - 1))) * difficulty().spawn * (1.0f + teamHorde());
                const float rate = (def.spawnRate.x + (def.spawnRate.y - def.spawnRate.x) * t) * coop * (run.bossWave ? 0.5f : 1.0f);
                const int most = static_cast<int>(std::lround((static_cast<float>(def.maxAlive.x) + static_cast<float>(def.maxAlive.y - def.maxAlive.x) * t) * coop));

                run.spawnDue += rate * dt;

                while (run.spawnDue >= 1.0f)
                {
                    run.spawnDue -= 1.0f;

                    // None in the last moments of a wave, nor past the most there may be at once
                    if ((not run.bossWave and run.timer < 2.0f) or run.enemiesLeft + static_cast<int>(run.pending.size()) >= most)
                        continue;

                    // Who: one of the pools open at this wave, by weight
                    float total = 0.0f;
                    for (const auto& p : def.pools)
                        if (run.wave >= p.from and run.wave <= p.to and content->enemies.count(p.enemy))
                            total += p.weight;

                    if (total <= 0.0f)
                        break;

                    float roll = static_cast<float>(runRandom() % 10000) / 10000.0f * total;
                    std::string enemy;

                    for (const auto& p : def.pools)
                    {
                        if (run.wave < p.from or run.wave > p.to or not content->enemies.count(p.enemy))
                            continue;

                        enemy = p.enemy;
                        roll -= p.weight;

                        if (roll <= 0.0f)
                            break;
                    }

                    const bool elite = static_cast<float>(run.wave) >= def.eliteFrom and static_cast<float>(runRandom() % 1000) / 1000.0f < def.eliteChance + difficulty().eliteChance;
                    run.pending.push_back({enemy, edgePoint(6.0f), def.warn, false, elite});
                    event(elite ? "elite_warn" : "spawn_warn", run.pending.back().position, def.warn);
                }

                // The wave ends with its time, a boss wave with its boss
                const Fighter* boss = run.bossId ? find(run.bossId) : nullptr;
                const bool bossDown = run.bossWave and run.bossId != 0 and (not boss or not boss->alive());

                if ((not run.bossWave and run.timer <= 0.0f) or bossDown)
                    endWave();
                break;
            }

            case RunPhase::Difficulty:
            {
                // One player picks (the host's, else the first one in), among the levels open
                run.chooser = difficultyChooser();
                const Fighter* chooser = run.chooser ? find(run.chooser) : nullptr;
                const int pick = chooser ? pickOf(*chooser) : 0;
                const int level = pick - InputFrame::DIFFICULTY;

                if (pick == InputFrame::BACK)
                {
                    stopRun();
                }
                else if (level >= 0 and level < def.levels() and level < std::max(1, run.levelsOpen))
                {
                    run.difficulty = level;
                    event("difficulty_pick", arena().centre, static_cast<float>(level));
                    startRun(run.seed);
                }
                break;
            }

            case RunPhase::Armory:
            {
                bool all = players > 0;

                for (auto& f : fighters)
                {
                    if (f.team != Team::Player)
                        continue;

                    const int pick = pickOf(f) - InputFrame::WEAPON;
                    if (pick >= 0 and pick < static_cast<int>(content->weaponOrder.size()))
                        chooseWeapon(f, content->weaponOrder[static_cast<size_t>(pick)]);

                    const int spell = pickOf(f) - InputFrame::FIRST_SPELL;
                    const auto& offered = f.firstSpellOffer.empty() ? def.startChoices : f.firstSpellOffer;
                    if (spell >= 0 and spell < static_cast<int>(offered.size()))
                        chooseFirstSpell(f, offered[static_cast<size_t>(spell)]);

                    all = all and f.ready;
                }

                if (all)
                    beginWave(1);
                break;
            }

            case RunPhase::Shop:
            {
                run.timer = std::max(0.0f, run.timer - dt);
                bool all = players > 0;

                for (auto& f : fighters)
                {
                    if (f.team != Team::Player)
                        continue;

                    if (const int pick = pickOf(f))
                        shopPick(f, pick);

                    all = all and f.ready;
                }

                if (all)
                    beginWave(run.wave + 1);
                break;
            }

            case RunPhase::Duel:
            {
                // The countdown: nobody may hurt anybody yet (see hostile)
                if (run.warmup > 0.0f)
                {
                    run.warmup = std::max(0.0f, run.warmup - dt);
                    if (run.warmup <= 0.0f)
                        event("duel_fight", arena().centre, 1.0f);
                    break;
                }

                run.timer = std::max(0.0f, run.timer - dt);

                // One life each: the last one standing, or when the time is up the most health left
                const Fighter* last = nullptr;
                for (const auto& f : fighters)
                    if (f.team == Team::Player and f.alive())
                        if (not last or f.health / std::max(1.0f, f.maxHealth) > last->health / std::max(1.0f, last->maxHealth))
                            last = &f;

                if (standing <= 1 or run.timer <= 0.0f)
                {
                    run.champion = last ? last->id : 0;
                    run.phase = RunPhase::Victory;
                    event("duel_end", last ? last->body.position : arena().centre, static_cast<float>(run.champion));
                    event("run_victory", arena().centre, static_cast<float>(run.score));
                }
                break;
            }

            case RunPhase::Victory:
            case RunPhase::Defeat:
                startAgain();
                break;
        }
    }
}
