#pragma once

#include <functional>
#include <string>
#include <vector>

#include "content.h"

namespace arena
{
    /**
     * Every gameplay number, read from the merged tuning.json of the loaded mods.
     * The values below are only fallbacks for a key a tuning file leaves out
     * (and such a key is reported), the real numbers live in mods/base/tuning.json.
     */
    struct Tuning
    {
        struct Player
        {
            float width = 0.6f, height = 1.8f;

            float walkSpeed = 5.5f;       // m/s
            float sprintSpeed = 8.5f;
            float groundAccel = 60.0f;    // m/s^2 towards the wanted velocity
            float groundDecel = 45.0f;    // m/s^2 when there is no input
            float turnAccel = 90.0f;      // m/s^2 when the input points against the current velocity
            float airAccel = 20.0f;
            float airDecel = 3.0f;

            float gravity = 32.0f;            // m/s^2 while rising
            float fallGravityScale = 1.6f;    // multiplier while falling
            float jumpCutGravityScale = 2.6f; // multiplier while rising with jump released (variable jump height)
            float maxFallSpeed = 40.0f;
            float jumpHeight = 1.6f;          // m, apex of a held jump
            float coyoteTime = 0.1f;          // s after leaving a ledge during which a jump still works
            float jumpBuffer = 0.12f;         // s a jump press is remembered before landing

            float stepHeight = 1.05f;         // highest ledge walked up without jumping
            float stepSmoothing = 16.0f;      // how fast the camera and model catch up a step (1/s)

            float mantleHeight = 0.5f;        // ledge assist: a ledge this far above the feet is climbed from the air
            float cornerCorrection = 0.3f;    // m a rising head slides sideways around a block edge

            float landingGrace = 0.15f;       // s after landing the takeoff speed is kept (sprint and roll jumps chain)
            float overspeedDecel = 14.0f;     // m/s^2 that speed above the run speed bleeds off (instead of the full brake)

            float hardLandingSpeed = 19.0f;   // m/s falling: above this, a landing costs hardLandingFrames
            int hardLandingFrames = 16;       // ... unless a dodge pressed just before landing rolls out of it
            float hardLandingMoveSpeed = 0.4f; // fraction of the walk speed kept during it (0 roots)
            float landingRollWindow = 0.35f;  // s a dodge pressed in the air waits for the landing (landing and wake-up rolls)

            float turnSpeed = 900.0f;         // degrees/s the body turns towards the movement

            float health = 100.0f;
            float poise = 10.0f;
            int hitstunFrames = 20;
            int hitstunDodgeFrom = 6;         // a light hit's stagger can be rolled out of from this frame ...
            float heavyHitPoise = 25.0f;      // ... unless the hit did this much poise damage (a heavy stagger)
            float respawnSeconds = 2.0f;

            float modelScale = 0.125f;
            glm::vec3 hand = glm::vec3(0.35f, 1.0f, 0.25f);   // where the weapon is held, local space
        } player;

        struct Stamina
        {
            float max = 100.0f;
            float regen = 35.0f;              // per second
            float regenDelay = 0.6f;          // s after spending before regen starts
            float sprintCost = 18.0f;         // per second
            float minToAct = 1.0f;            // needed to start a dodge or an attack
            float exhaustedRegenScale = 0.6f; // regen multiplier after hitting 0
        } stamina;

        struct Dodge
        {
            int frames = 24;           // whole roll
            int iframeStart = 2;       // invulnerable from this frame ...
            int iframeEnd = 15;        // ... to this one (excluded)
            int recovery = 6;          // last frames: no new action except a buffered one
            float speed = 11.0f;       // m/s at the start
            float endSpeed = 3.0f;     // m/s at the end
            float cost = 22.0f;
            int attackFrom = 14;       // a light press from this frame on becomes the dodge attack
            int jumpFrom = 1;          // a jump from this frame on cancels the roll (and takes off with its speed)
            bool inAir = false;        // allow rolling while airborne
        } dodge;

        struct Camera
        {
            float distance = 5.0f;
            float minDistance = 1.2f;
            float maxDistance = 9.0f;
            float zoomStep = 0.5f;
            float height = 1.55f;          // pivot above the feet
            float shoulder = 0.45f;        // pivot offset to the right
            float sensitivity = 0.15f;     // degrees per pixel
            float minPitch = -70.0f, maxPitch = 55.0f;
            float followSharpness = 14.0f; // 1/s, how fast the pivot follows the player
            float pullInSharpness = 40.0f; // 1/s, towards a wall
            float pullOutSharpness = 5.0f; // 1/s, back out once the wall is gone
            float collisionRadius = 0.25f;
            float fov = 70.0f;
        } camera;

        struct FirstPerson
        {
            float eyeHeight = 1.62f;
            float minPitch = -85.0f, maxPitch = 85.0f;
            float fov = 80.0f;
            glm::vec3 weapon = glm::vec3(0.3f, -0.32f, 0.45f);   // view model offset from the eye: right, up, forward
            float rollDip = 0.7f;          // m the view drops during a dodge roll
            float viewModelScale = 0.3f;   // the weapon is drawn this much smaller and closer: same on screen, rarely in a wall
        } firstPerson;

        struct Feel
        {
            int hitFlashFrames = 6;
            float shakeDecay = 1.8f;       // trauma lost per second
            float shakeOffset = 0.22f;     // m at full trauma
            float shakeAngle = 1.5f;       // degrees at full trauma
            float shakeFrequency = 22.0f;  // Hz
            float hitstopScale = 1.0f;     // multiplies every attack's hitstop
            float knockbackFriction = 16.0f; // m/s^2 that knockback loses on the ground
            float landingShake = 0.0f;     // trauma from landing a big fall
        } feel;

        struct Combat
        {
            float inputBuffer = 0.18f;     // s an attack or dodge press waits for the current action to allow it
            float aimAssistAngle = 60.0f;  // degrees: attacks snap to an enemy within this cone ...
            float aimAssistRange = 4.0f;   // ... and this distance
            float maxAttackPitch = 60.0f;  // degrees an attack may aim up or down (at its target, or with the view)
        } combat;

        struct Terrain
        {
            float gravity = 30.0f;          // m/s^2 for falling pieces
            float crushDamage = 30.0f;      // a falling piece hitting a fighter ...
            float crushPerBlock = 3.0f;     // ... plus this per block of the piece ...
            float crushMax = 90.0f;         // ... up to this
            float crushPoise = 60.0f;
            float crushShake = 0.6f;        // camera trauma when a piece lands near the player
        } terrain;

        std::vector<std::string> abilities = {"base:raise_wall", "base:dig_pit", "base:mine"};   // ability1, 2, 3...

        std::string playerModel = "base:player";
        std::string startWeapon = "base:sword";

        /** Calls f for every number, with its dotted path ("player.walkSpeed"). */
        void forEachNumber(const std::function<void(const std::string& path, float& value)>& f);
        void forEachInt(const std::function<void(const std::string& path, int& value)>& f);

        /** Sets one value by path, for the console. Returns false for an unknown path. */
        bool set(const std::string& path, float value);

        /** Reads a value by path into out, for the console. */
        bool get(const std::string& path, float& out);
    };

    /** Fills tuning from the merged tuning tree; missing keys keep their fallback and are reported as warnings. */
    Tuning readTuning(const Json& tree, const std::string& file, ContentReport& report);
}
