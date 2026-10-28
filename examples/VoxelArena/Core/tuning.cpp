#include "tuning.h"

#include <cmath>

namespace arena
{
    void Tuning::forEachNumber(const std::function<void(const std::string&, float&)>& f)
    {
        f("player.width", player.width);
        f("player.height", player.height);
        f("player.walkSpeed", player.walkSpeed);
        f("player.sprintSpeed", player.sprintSpeed);
        f("player.groundAccel", player.groundAccel);
        f("player.groundDecel", player.groundDecel);
        f("player.turnAccel", player.turnAccel);
        f("player.airAccel", player.airAccel);
        f("player.airDecel", player.airDecel);
        f("player.gravity", player.gravity);
        f("player.fallGravityScale", player.fallGravityScale);
        f("player.jumpCutGravityScale", player.jumpCutGravityScale);
        f("player.maxFallSpeed", player.maxFallSpeed);
        f("player.jumpHeight", player.jumpHeight);
        f("player.coyoteTime", player.coyoteTime);
        f("player.jumpBuffer", player.jumpBuffer);
        f("player.stepHeight", player.stepHeight);
        f("player.stepSmoothing", player.stepSmoothing);
        f("player.mantleHeight", player.mantleHeight);
        f("player.cornerCorrection", player.cornerCorrection);
        f("player.landingGrace", player.landingGrace);
        f("player.overspeedDecel", player.overspeedDecel);
        f("player.hardLandingSpeed", player.hardLandingSpeed);
        f("player.heavyHitPoise", player.heavyHitPoise);
        f("player.hardLandingMoveSpeed", player.hardLandingMoveSpeed);
        f("player.landingRollWindow", player.landingRollWindow);
        f("player.turnSpeed", player.turnSpeed);
        f("player.health", player.health);
        f("player.poise", player.poise);
        f("player.respawnSeconds", player.respawnSeconds);
        f("player.modelScale", player.modelScale);
        f("player.hand.x", player.hand.x);
        f("player.hand.y", player.hand.y);
        f("player.hand.z", player.hand.z);

        f("air.jumpHeight", air.jumpHeight);
        f("air.wallJumpPush", air.wallJumpPush);
        f("air.wallJumpHeight", air.wallJumpHeight);
        f("air.wallReach", air.wallReach);
        f("air.dashSpeed", air.dashSpeed);
        f("air.dashMinHeight", air.dashMinHeight);
        f("air.fastFallGravityScale", air.fastFallGravityScale);
        f("air.fastFallMaxSpeed", air.fastFallMaxSpeed);

        f("dash.recharge", dash.recharge);

        f("mana.max", mana.max);
        f("mana.regen", mana.regen);
        f("mana.regenDelay", mana.regenDelay);

        f("elements.burnSeconds", elements.burnSeconds);
        f("elements.spreadSeconds", elements.spreadSeconds);
        f("elements.iceLifetime", elements.iceLifetime);

        f("stamina.max", stamina.max);
        f("stamina.regen", stamina.regen);
        f("stamina.regenDelay", stamina.regenDelay);
        f("stamina.sprintCost", stamina.sprintCost);
        f("stamina.minToAct", stamina.minToAct);
        f("stamina.exhaustedRegenScale", stamina.exhaustedRegenScale);

        f("dodge.speed", dodge.speed);
        f("dodge.endSpeed", dodge.endSpeed);

        f("camera.distance", camera.distance);
        f("camera.minDistance", camera.minDistance);
        f("camera.maxDistance", camera.maxDistance);
        f("camera.zoomStep", camera.zoomStep);
        f("camera.height", camera.height);
        f("camera.shoulder", camera.shoulder);
        f("camera.sensitivity", camera.sensitivity);
        f("camera.minPitch", camera.minPitch);
        f("camera.maxPitch", camera.maxPitch);
        f("camera.followSharpness", camera.followSharpness);
        f("camera.pullInSharpness", camera.pullInSharpness);
        f("camera.pullOutSharpness", camera.pullOutSharpness);
        f("camera.collisionRadius", camera.collisionRadius);
        f("camera.fov", camera.fov);

        f("firstPerson.eyeHeight", firstPerson.eyeHeight);
        f("firstPerson.minPitch", firstPerson.minPitch);
        f("firstPerson.maxPitch", firstPerson.maxPitch);
        f("firstPerson.fov", firstPerson.fov);
        f("firstPerson.weapon.x", firstPerson.weapon.x);
        f("firstPerson.weapon.y", firstPerson.weapon.y);
        f("firstPerson.weapon.z", firstPerson.weapon.z);
        f("firstPerson.rollDip", firstPerson.rollDip);
        f("firstPerson.viewModelScale", firstPerson.viewModelScale);

        f("feel.shakeDecay", feel.shakeDecay);
        f("feel.shakeOffset", feel.shakeOffset);
        f("feel.shakeAngle", feel.shakeAngle);
        f("feel.shakeFrequency", feel.shakeFrequency);
        f("feel.hitstopScale", feel.hitstopScale);
        f("feel.knockbackFriction", feel.knockbackFriction);
        f("feel.landingShake", feel.landingShake);

        f("terrain.gravity", terrain.gravity);
        f("terrain.crushDamage", terrain.crushDamage);
        f("terrain.crushPerBlock", terrain.crushPerBlock);
        f("terrain.crushMax", terrain.crushMax);
        f("terrain.crushPoise", terrain.crushPoise);
        f("terrain.crushShake", terrain.crushShake);

        f("combat.inputBuffer", combat.inputBuffer);
        f("combat.aimAssistAngle", combat.aimAssistAngle);
        f("combat.aimAssistRange", combat.aimAssistRange);
        f("combat.maxAttackPitch", combat.maxAttackPitch);
        f("combat.dashAttackWindow", combat.dashAttackWindow);
    }

    void Tuning::forEachInt(const std::function<void(const std::string&, int&)>& f)
    {
        f("player.hitstunFrames", player.hitstunFrames);
        f("player.hardLandingFrames", player.hardLandingFrames);
        f("player.hitstunDodgeFrom", player.hitstunDodgeFrom);

        f("air.jumps", air.jumps);
        f("air.wallJumpLockFrames", air.wallJumpLockFrames);
        f("air.dashFrames", air.dashFrames);
        f("air.dashes", air.dashes);
        f("dash.charges", dash.charges);

        f("dodge.frames", dodge.frames);
        f("dodge.iframeStart", dodge.iframeStart);
        f("dodge.iframeEnd", dodge.iframeEnd);
        f("dodge.recovery", dodge.recovery);
        f("dodge.attackFrom", dodge.attackFrom);
        f("dodge.jumpFrom", dodge.jumpFrom);

        f("feel.hitFlashFrames", feel.hitFlashFrames);
    }

    bool Tuning::set(const std::string& path, float value)
    {
        bool found = false;

        forEachNumber([&](const std::string& p, float& v) {
            if (p == path)
            {
                v = value;
                found = true;
            }
        });

        forEachInt([&](const std::string& p, int& v) {
            if (p == path)
            {
                v = static_cast<int>(std::lround(value));
                found = true;
            }
        });

        if (path == "dodge.inAir")
        {
            dodge.inAir = value != 0.0f;
            found = true;
        }

        return found;
    }

    bool Tuning::get(const std::string& path, float& out)
    {
        bool found = false;

        forEachNumber([&](const std::string& p, float& v) {
            if (p == path)
            {
                out = v;
                found = true;
            }
        });

        forEachInt([&](const std::string& p, int& v) {
            if (p == path)
            {
                out = static_cast<float>(v);
                found = true;
            }
        });

        return found;
    }

    namespace
    {
        /** Walks "a.b.c" down the tree. A vector component ("player.hand.x") reads index 0..2 of a list. */
        const Json* lookup(const Json& tree, const std::string& path, int& component)
        {
            component = -1;
            const Json* node = &tree;
            size_t start = 0;

            while (start <= path.size())
            {
                const size_t dot = path.find('.', start);
                const std::string key = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);

                if (node->isArray() and key.size() == 1 and key[0] >= 'x' and key[0] <= 'z')
                {
                    component = key[0] - 'x';
                    return node;
                }

                node = node->find(key);

                if (not node)
                    return nullptr;

                if (dot == std::string::npos)
                    break;

                start = dot + 1;
            }

            return node;
        }
    }

    Tuning readTuning(const Json& tree, const std::string& file, ContentReport& report)
    {
        Tuning t;

        auto readNumber = [&](const std::string& path, double& out) {
            int component = -1;
            const Json* v = lookup(tree, path, component);

            if (not v)
            {
                report.warn(file, 0, "tuning has no \"" + path + "\", using the built-in fallback");
                return false;
            }

            if (component >= 0)
            {
                if (component >= static_cast<int>(v->array.size()) or not v->array[component].isNumber())
                {
                    report.error(file, v->line, "\"" + path + "\" must be a list of 3 numbers");
                    return false;
                }

                out = v->array[component].number;
                return true;
            }

            if (not v->isNumber())
            {
                report.error(file, v->line, "\"" + path + "\" must be a number");
                return false;
            }

            out = v->number;
            return true;
        };

        t.forEachNumber([&](const std::string& path, float& value) {
            double v;
            if (readNumber(path, v))
                value = static_cast<float>(v);
        });

        t.forEachInt([&](const std::string& path, int& value) {
            double v;
            if (readNumber(path, v))
                value = static_cast<int>(std::lround(v));
        });

        if (const Json* dodge = tree.find("dodge"))
            t.dodge.inAir = dodge->flag("inAir", t.dodge.inAir);

        if (const Json* player = tree.find("player"))
        {
            t.playerModel = player->str("model", t.playerModel);
            t.startWeapon = player->str("weapon", t.startWeapon);

            if (const Json* list = player->find("spells"))
                for (const auto& e : list->array)
                    if (e.isString())
                        t.playerSpells.push_back(e.string);

            if (const Json* list = player->find("abilities"))
            {
                t.abilities.clear();

                // A slot is an id, "" (empty), or a list of ids (the first the weapon allows)
                for (const auto& e : list->array)
                {
                    std::vector<std::string> slot;

                    if (e.isString() and not e.string.empty())
                        slot.push_back(e.string);
                    else if (e.isArray())
                        for (const auto& id : e.array)
                            if (id.isString())
                                slot.push_back(id.string);

                    t.abilities.push_back(slot);
                }
            }
        }

        return t;
    }
}
