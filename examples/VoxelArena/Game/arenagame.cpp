#include "stdafx.h"

#include "arenagame.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <filesystem>

#include "Input/input.h"

#include "Core/net/client.h"
#include "Core/net/server.h"
#include "Net/enginetransport.h"
#include "Renderer/camera.h"
#include "logger.h"
#include "window.h"

#ifdef _WIN32
    #include <SDL.h>
#else
    #include <SDL2/SDL.h>
#endif

using namespace pg;

namespace arena
{
    namespace
    {
        constexpr const char* DOM = "Arena";
        constexpr float DEG = 3.14159265358979f / 180.0f;
        constexpr int MAX_STEPS_PER_FRAME = 6;
        constexpr size_t CONSOLE_LINES = 11;

        const glm::vec4 WHITE(1.0f);

        glm::mat4 rotY(float degrees) { return glm::rotate(glm::mat4(1.0f), degrees * DEG, glm::vec3(0, 1, 0)); }
        glm::mat4 rotX(float degrees) { return glm::rotate(glm::mat4(1.0f), degrees * DEG, glm::vec3(1, 0, 0)); }

        float lerp(float a, float b, float t) { return a + (b - a) * t; }
        float easeOut(float t) { t = glm::clamp(t, 0.0f, 1.0f); return 1.0f - (1.0f - t) * (1.0f - t); }
        float easeInOut(float t) { t = glm::clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }

        float lerpAngle(float a, float b, float t)
        {
            float d = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
            return a + d * t;
        }

        std::string fmt(float v, int decimals = 1)
        {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(decimals) << v;
            return ss.str();
        }

        std::vector<std::string> split(const std::string& line)
        {
            std::vector<std::string> out;
            std::istringstream ss(line);
            std::string word;

            while (ss >> word)
                out.push_back(word);

            return out;
        }

        /** Where the swing of an attack is at a frame: the weapon (player) or the body lean (enemies). */
        SwingPose poseAt(const AttackDef& a, const SwingPose& rest, float frame)
        {
            SwingPose p;
            const float s = static_cast<float>(a.startup), act = static_cast<float>(a.active), r = static_cast<float>(std::max(1, a.recovery));

            // The pose at a frame lands in yawFrom / pitchFrom / reach
            auto mix = [&](float yaw0, float pitch0, float reach0, float yaw1, float pitch1, float reach1, float t) {
                p.yawFrom = lerp(yaw0, yaw1, t);
                p.pitchFrom = lerp(pitch0, pitch1, t);
                p.reach = lerp(reach0, reach1, t);
            };

            if (frame < s)
            {
                // Wind-up: from rest to the start of the swing, slowing down at the end (a readable pose)
                mix(rest.yawFrom, rest.pitchFrom, 0.0f, a.swing.yawFrom, a.swing.pitchFrom, 0.0f, easeOut(frame / std::max(1.0f, s)));
            }
            else if (frame < s + act)
            {
                mix(a.swing.yawFrom, a.swing.pitchFrom, 0.0f, a.swing.yawTo, a.swing.pitchTo, a.swing.reach, easeOut((frame - s + 1.0f) / act));
            }
            else
            {
                const float t = easeInOut((frame - s - act) / r);
                mix(a.swing.yawTo, a.swing.pitchTo, a.swing.reach, rest.yawFrom, rest.pitchFrom, 0.0f, t);
            }

            return p;
        }

        void pushInstance(std::vector<float>& data, const glm::mat4& view, const glm::mat4& model, const glm::vec4& tint, const glm::vec4& flash)
        {
            const glm::mat4 eye = view * model;
            const float* m = glm::value_ptr(model);
            const float* e = glm::value_ptr(eye);
            data.insert(data.end(), m, m + 16);
            data.insert(data.end(), e, e + 16);
            data.insert(data.end(), {tint.r, tint.g, tint.b, tint.a, flash.r, flash.g, flash.b, flash.a});
        }

        /** Default bindings, used for any action the tuning's "controls" section leaves out. */
        const std::vector<std::pair<std::string, std::vector<std::string>>> DEFAULT_CONTROLS = {
            {"forward", {"W", "Up"}}, {"back", {"S", "Down"}}, {"left", {"A", "Left"}}, {"right", {"D", "Right"}},
            {"jump", {"Space"}}, {"dodge", {"Left Shift"}}, {"sprint", {"Left Shift"}},
            {"light", {"Mouse1", "J"}}, {"heavy", {"Mouse3", "K"}},
            {"freeCursor", {"Left Alt"}}, {"toggleView", {"F", "V"}},
            {"ability1", {"Q"}}, {"ability2", {"E"}}, {"ability3", {"G", "Mouse2"}}, {"ability4", {"R"}},
            {"ability5", {"1"}}, {"ability6", {"2"}}, {"ability7", {"3"}}, {"ability8", {"4"}},
            {"crouch", {"Left Ctrl", "C"}}, {"nextWeapon", {"Tab"}},
        };
    }

    ArenaGame::ArenaGame(MasterRenderer* masterRenderer, Window* window, const ArenaOptions& options)
        : AbstractRenderer(masterRenderer, RenderStage::Render), window(window), options(options)
    {
        input = window ? const_cast<Input*>(window->getInputHandler()) : nullptr;

        screenW = static_cast<float>(window ? window->getWidth() : 1280);
        screenH = static_cast<float>(window ? window->getHeight() : 720);
        aspect = screenH > 0.0f ? screenW / screenH : 16.0f / 9.0f;
    }

    ArenaGame::~ArenaGame() = default;

    // --------------------------------------------------------------------------------------------
    // Setup and content
    // --------------------------------------------------------------------------------------------

    void ArenaGame::init()
    {
        defaultState.depthTestEnabled = true;
        defaultState.depthFunc = DepthFunc::LEqual;

        auto makeMaterial = [this](const std::string& name, int mode) {
            Material m;
            m.shader = masterRenderer->getShader("arena");
            m.nbTextures = 0;
            m.nbAttributes = GpuMesh::INSTANCE_FLOATS;
            m.uniformMap["uMode"] = UniformValue(mode);
            return masterRenderer->registerMaterial(name, m);
        };

        litMaterial = makeMaterial("arena_lit", 0);
        skyMaterial = makeMaterial("arena_sky", 1);
        debugMaterial = makeMaterial("arena_debug", 2);

        // A unit cube, used scaled for the sky box and for the debug boxes
        {
            VoxelGrid cube(1, 1, 1);
            cube.outsideSolid = false;
            cube.set(0, 0, 0, 1);
            BlockTable table(2);
            table[0].visible = false;
            MeshOptions opt;
            opt.aoStrength = 0.0f;

            skyMesh = std::make_shared<GpuMesh>();
            skyMesh->setData(buildMesh(cube, table, opt));
            boxMesh = std::make_shared<GpuMesh>();
            boxMesh->setData(buildMesh(cube, table, opt));
        }

        worldMesh = std::make_shared<GpuMesh>();

        hud.init(ecsRef, masterRenderer, screenW, screenH);

        masterRenderer->getCamera().setAspectRatio(aspect);

        lastReload = std::chrono::steady_clock::now();
        loadAll(true);

        for (const auto& c : options.startupCommands)
            commandQueue.push_back(c);
    }

    void ArenaGame::loadAll(bool first)
    {
        const auto start = std::chrono::steady_clock::now();

        const std::string oldArena = first ? "" : toJsonString(content.arena.shapes) + toJsonString(Json::makeNumber(content.arena.size.x * 10000 + content.arena.size.y * 100 + content.arena.size.z));

        content = loadContent(options.modsDir);
        tuning = readTuning(content.tuning, content.tuningFile, content.report);
        brain.load(content, content.report);
        readControls();

        const std::string newArena = toJsonString(content.arena.shapes) + toJsonString(Json::makeNumber(content.arena.size.x * 10000 + content.arena.size.y * 100 + content.arena.size.z));

        if (first)
        {
            if (options.mode == ArenaOptions::Mode::Client and options.network)
            {
                // The world is the server's: the client builds an empty arena and fills it from the network
                transport = std::make_unique<net::EngineTransport>(options.network);
                client = std::make_unique<net::NetClient>(world, content, tuning, *transport, options.playerName);
            }
            else
            {
                world.start(content, tuning);
                world.setBrain(&brain);

                if (options.mode == ArenaOptions::Mode::Host and options.network)
                {
                    transport = std::make_unique<net::EngineTransport>(options.network);
                    server = std::make_unique<net::NetServer>(world, content, *transport, false);
                }
            }

            camera.distance = tuning.camera.distance;
            camera.yaw = world.player() ? world.player()->yaw : 0.0f;

            // First person starts looking ahead, not down at the feet as the orbit camera does
            if (firstPerson)
                camera.pitch = 5.0f;
        }
        else
        {
            // Only a changed arena is rebuilt: edits made in the arena survive a tuning tweak
            world.setContent(content, tuning, oldArena != newArena);
        }

        // Models: the GPU meshes are kept and refilled, never destroyed (see GpuMesh)
        for (const auto& [id, model] : content.models)
        {
            MeshOptions opt;
            opt.origin = glm::vec3(0.0f);
            opt.aoStrength = 0.45f;
            modelMesh(id)->setData(buildMesh(model->grid, model->table, opt));

            // Body parts, each its own mesh, so limbs can move
            if (model->parts.size() > 1)
                for (const auto& part : model->parts)
                    modelMesh(id + "#" + part.name)->setData(buildMesh(part.grid, model->table, opt));
        }

        // Sounds and effects: synthesised into a cache folder (the engine's audio plays files)
        const auto recipes = readSounds(content.sounds, content.report);
        effects = readEffects(content.effects, recipes, content.report);
        sounds.load(recipes, (std::filesystem::temp_directory_path() / "voxelarena-sounds").string(), content.report);

        meshDirty = true;
        masterRenderer->getCamera().setFovDegrees(fov());

        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

        for (const auto& e : content.report.errors)
            LOG_ERROR(DOM, e.str());
        for (const auto& w : content.report.warnings)
            LOG_WARNING(DOM, w.str());

        print(std::string(first ? "Loaded " : "Reloaded ") + contentSummary() + " in " + fmt(static_cast<float>(ms), 0) + " ms");

        for (size_t i = 0; i < content.report.errors.size() and i < 4; ++i)
            print("\\c{255,120,110,255}error: " + content.report.errors[i].str() + "\\c{236,238,242,255}");
        for (size_t i = 0; i < content.report.warnings.size() and i < 4; ++i)
            print("\\c{240,200,110,255}warning: " + content.report.warnings[i].str() + "\\c{236,238,242,255}");
    }

    std::string ArenaGame::contentSummary() const
    {
        std::string mods;
        for (const auto& m : content.mods)
            mods += (mods.empty() ? "" : ", ") + m.id;

        return std::to_string(content.mods.size()) + " mods (" + mods + "): " + std::to_string(content.weapons.size()) + " weapons, " +
               std::to_string(content.enemies.size()) + " enemies, " + std::to_string(content.blocks.size() - 1) + " blocks, " +
               std::to_string(content.report.errors.size()) + " errors, " + std::to_string(content.report.warnings.size()) + " warnings";
    }

    void ArenaGame::readControls()
    {
        controls.clear();
        const Json* section = content.tuning.find("controls");

        auto parse = [&](const std::string& name) -> Binding {
            Binding b;

            if (name.rfind("Mouse", 0) == 0 and name.size() == 6 and name[5] >= '1' and name[5] <= '5')
            {
                b.mouseButton = name[5] - '0';
                return b;
            }

            const SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());

            if (sc == SDL_SCANCODE_UNKNOWN)
                content.report.warn(content.tuningFile, 0, "controls: unknown key \"" + name + "\" (SDL key names, or Mouse1-Mouse5)");
            else
                b.scancode = sc;

            return b;
        };

        for (const auto& [action, defaults] : DEFAULT_CONTROLS)
        {
            std::vector<std::string> names = defaults;

            if (section)
            {
                if (const Json* v = section->find(action))
                {
                    names.clear();

                    if (v->isString())
                        names.push_back(v->string);
                    else
                        for (const auto& e : v->array)
                            if (e.isString())
                                names.push_back(e.string);
                }
            }

            for (const auto& n : names)
            {
                const Binding b = parse(n);

                if (b.scancode >= 0 or b.mouseButton >= 0)
                    controls[action].push_back(b);
            }
        }
    }

    std::shared_ptr<GpuMesh> ArenaGame::modelMesh(const std::string& modelId)
    {
        auto& mesh = modelMeshes[modelId];

        if (not mesh)
            mesh = std::make_shared<GpuMesh>();

        return mesh;
    }

    // --------------------------------------------------------------------------------------------
    // Events (any thread): they only record, execute() acts
    // --------------------------------------------------------------------------------------------

    void ArenaGame::onEvent(const OnSDLScanCode& event)
    {
        std::lock_guard<std::mutex> lock(inputMutex);

        const bool repeat = keysDown.count(event.key) > 0;
        keysDown.insert(event.key);
        keyPresses.push_back({event.key, repeat, event.mod});

        if (not repeat)
            stepPresses.insert(event.key);
    }

    void ArenaGame::onEvent(const OnSDLScanCodeReleased& event)
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        keysDown.erase(event.key);
    }

    void ArenaGame::onEvent(const OnSDLTextInput& event)
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        typed += event.text;
    }

    void ArenaGame::onEvent(const OnSDLMouseMotion& event)
    {
        std::lock_guard<std::mutex> lock(inputMutex);

        // The motion SDL makes for our own warp, matched by position wherever it lands in the queue
        if (warpPendingX >= 0 and event.x == warpPendingX and event.y == warpPendingY)
        {
            warpPendingX = warpPendingY = -1;
            return;
        }

        // A lock the browser has not granted yet (or WSL before its helper holds the cursor): the cursor is free
        if (not cursorLocked or (window and not window->isCursorCaptured()))
            return;

        lookDx += static_cast<float>(event.xrel);
        lookDy += static_cast<float>(event.yrel);

        // Keep the hidden cursor away from the edges (a warp on every motion would stutter)
        if (window and window->needsCursorRecentering())
        {
            const int w = window->getWidth(), h = window->getHeight();
            const int mx = std::max(80, w / 6), my = std::max(80, h / 6);

            if (event.x < mx or event.x > w - mx or event.y < my or event.y > h - my)
            {
                window->setCursorLocked(true);
                warpPendingX = w / 2;
                warpPendingY = h / 2;
            }
        }
    }

    void ArenaGame::onEvent(const OnMouseClick& event)
    {
        // The engine's MouseClickSystem is the one reader of the press latch (Input::takePress): reading
        // it here too lost every click that system took first. Its click event comes once per press.
        std::lock_guard<std::mutex> lock(inputMutex);

        // The click that grants a pending lock (browser, WSL window getting focus) does not attack either
        if (cursorLocked and (not window or window->isCursorCaptured()))
            mousePresses.insert(static_cast<int>(event.button));
        else if (cursorFree)
            recapture = true;   // a click on a freed window takes the mouse back, it does not attack
    }

    void ArenaGame::onEvent(const OnSDLMouseWheel& event)
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        wheel += static_cast<float>(event.y);
    }

    void ArenaGame::onEvent(const ResizeEvent& event)
    {
        if (event.width <= 0.0f or event.height <= 0.0f)
            return;

        aspect = event.width / event.height;
        masterRenderer->getCamera().setAspectRatio(aspect);
        hud.resize(event.width, event.height);
    }

    // --------------------------------------------------------------------------------------------
    // Input
    // --------------------------------------------------------------------------------------------

    bool ArenaGame::keyIs(int scancode, const std::string& action) const
    {
        auto it = controls.find(action);

        if (it == controls.end())
            return false;

        for (const auto& b : it->second)
            if (b.scancode == scancode)
                return true;

        return false;
    }

    bool ArenaGame::held(const std::string& action) const
    {
        auto it = controls.find(action);

        if (it == controls.end())
            return false;

        for (const auto& b : it->second)
        {
            if (b.scancode >= 0 and keysDown.count(b.scancode))
                return true;

            if (b.mouseButton >= 0 and input and (input->isButtonPressed(static_cast<MouseButton>(b.mouseButton)) or input->isButtonGrabbed(static_cast<MouseButton>(b.mouseButton))))
                return true;
        }

        return false;
    }

    bool ArenaGame::pressed(const std::string& action) const
    {
        auto it = controls.find(action);

        if (it == controls.end())
            return false;

        for (const auto& b : it->second)
        {
            if (b.scancode >= 0 and stepPresses.count(b.scancode))
                return true;

            if (b.mouseButton >= 0 and mousePresses.count(b.mouseButton))
                return true;
        }

        return false;
    }

    void ArenaGame::updateCursorLock()
    {
        if (not window)
            return;

        // The browser let go of the mouse (Esc): free it until a click takes it back
        if (window->takeCursorLockLost())
            cursorFree = true;

        bool altHeld = false;
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            altHeld = held("freeCursor");
        }

        const bool want = not altHeld and not consoleOpen and not cursorFree;

        if (want == cursorLocked)
            return;

        window->setCursorLocked(want);

        std::lock_guard<std::mutex> lock(inputMutex);

        if (want and window->needsCursorRecentering())
        {
            warpPendingX = window->getWidth() / 2;
            warpPendingY = window->getHeight() / 2;
        }

        cursorLocked = want;
        lookDx = lookDy = 0.0f;
    }

    // --------------------------------------------------------------------------------------------
    // Frame
    // --------------------------------------------------------------------------------------------

    void ArenaGame::execute()
    {
        const auto now = std::chrono::steady_clock::now();

        if (not clockStarted)
        {
            lastFrame = now;
            clockStarted = true;
        }

        float frameSeconds = std::chrono::duration<float>(now - lastFrame).count();

        // The ECS runs a few passes per rendered frame, and the renderer shows the list of whichever
        // pass gets to it first after a frame is drawn: sometimes the first pass, sometimes a later one,
        // half a frame apart. Sampled each pass, the shown frames would jump back and forth by half a
        // step (a jitter you see when the camera pans, strafing). So a frame's work is done once, on its
        // first pass, timed by the frame clock: whichever pass is shown, it shows the same moment.
        if (ecsRef->ecsPassPacer.enabled() and ecsRef->ecsPassPacer.scheduleBaseUs() > 0)
        {
            const int64_t frameStartUs = ecsRef->ecsPassPacer.scheduleBaseUs();

            if (frameStartUs == lastFrameStartUs)
                return;

            frameSeconds = lastFrameStartUs > 0 ? static_cast<float>(frameStartUs - lastFrameStartUs) * 1e-6f : 0.0f;
            lastFrameStartUs = frameStartUs;
        }
        else
        {
            // Pacer off (console: pacer off): every pass is a frame, on the wall clock
            lastFrameStartUs = 0;
        }

        lastFrame = now;
        frameSeconds = std::min(frameSeconds, 0.1f);

        if (frameSeconds > 0.0f)
            frameSecondsSmoothed += (frameSeconds - frameSecondsSmoothed) * 0.05f;

        // Debug keys and the console
        std::vector<KeyPress> presses;
        std::string text;
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            presses.swap(keyPresses);
            text.swap(typed);
        }

        for (const auto& p : presses)
        {
            if (p.scancode == SDL_SCANCODE_GRAVE and not p.repeat)
            {
                consoleOpen = not consoleOpen;
                text.clear();
                continue;
            }

            if (consoleOpen)
            {
                consoleKey(p.scancode);
                continue;
            }

            if (p.repeat)
                continue;

            switch (p.scancode)
            {
                case SDL_SCANCODE_F1: showOverlay = not showOverlay; break;
                case SDL_SCANCODE_F2: showBoxes = not showBoxes; break;
                case SDL_SCANCODE_F5: print(runCommand("reload")); break;
                // A lock not held (Esc in a browser also ends Pointer Lock) is let go, not asked again
                case SDL_SCANCODE_ESCAPE: cursorFree = (cursorLocked and window and not window->isCursorCaptured()) or not cursorFree; break;
                default:
                    if (keyIs(p.scancode, "toggleView"))
                    {
                        setFirstPerson(not firstPerson);
                        break;
                    }

                    // The next weapon (the number keys are abilities)
                    if (keyIs(p.scancode, "nextWeapon") and not content.weaponOrder.empty())
                    {
                        const Fighter* me = world.player();
                        const auto at = me ? std::find(content.weaponOrder.begin(), content.weaponOrder.end(), me->weapon) : content.weaponOrder.end();
                        const size_t next = at == content.weaponOrder.end() ? 0 : (static_cast<size_t>(at - content.weaponOrder.begin()) + 1) % content.weaponOrder.size();
                        pendingEquip = static_cast<int>(next) + 1;
                    }
                    break;
            }
        }

        {
            std::lock_guard<std::mutex> lock(inputMutex);

            if (recapture)
            {
                cursorFree = false;
                recapture = false;
            }
        }

        if (consoleOpen)
        {
            for (char c : text)
            {
                if (c != '`' and c >= 32 and c < 127)
                    consoleLine += c;
            }

            // Nothing typed into the console reaches the game
            std::lock_guard<std::mutex> lock(inputMutex);
            stepPresses.clear();
            mousePresses.clear();
        }

        runQueue();
        simulate(frameSeconds);
        updateCamera(frameSeconds);
        stepGaits(frameSeconds);
        stepLooks(frameSeconds);
        particles.step(frameSeconds, world.grid, content.blockTable);
        remeshIfNeeded();
        buildRenderCalls();
        updateCursorLock();

        // ---- HUD ----
        const Fighter* p = world.player();

        if (p)
        {
            // The mana bar dims when not even the cheapest spell is affordable
            float cheapest = 1e9f;
            for (int i = 0; i < InputFrame::ABILITIES; ++i)
                if (const AbilityDef* a = world.ability(i); a and a->manaCost > 0.0f)
                    cheapest = std::min(cheapest, a->manaCost);

            hud.setBars(p->health / std::max(1.0f, p->maxHealth), p->stamina / std::max(1.0f, tuning.stamina.max), p->exhausted,
                        p->mana / std::max(1.0f, tuning.mana.max), p->mana < cheapest and cheapest < 1e8f);

            std::string weapons;
            for (size_t i = 0; i < content.weaponOrder.size(); ++i)
            {
                const auto& id = content.weaponOrder[i];
                const bool current = id == p->weapon;
                weapons += (current ? "\\c{255,214,120,255}" : "\\c{170,174,184,255}") + content.weapons.at(id).name + "   ";
            }
            weapons += "\\c{120,124,134,255}(" + keyName("nextWeapon") + ": next)";
            hud.setWeapon(weapons + "\n" + abilityLine());
            hud.setCrosshair(p->alive());   // both views: the abilities aim with it

            // The hit flash fades in about a third of a second
            hurtFlash = std::max(0.0f, hurtFlash - frameSeconds * 3.0f);
            hud.setHurt(hurtFlash);

            hud.setCenter(not p->alive() ? "You fell" : (paused ? "Paused" : ""));
        }
        else if (client)
        {
            switch (client->state)
            {
                case net::NetClient::State::Connecting: hud.setCenter("Connecting..."); break;
                case net::NetClient::State::Joining: hud.setCenter("Joining..."); break;
                case net::NetClient::State::Refused: hud.setCenter("Refused: " + client->refusal); break;
                case net::NetClient::State::Lost: hud.setCenter("Connection lost"); break;
                case net::NetClient::State::Playing: hud.setCenter(""); break;
            }
        }

        if (content.report.errors.empty())
            hud.setBanner("");
        else
            hud.setBanner(std::to_string(content.report.errors.size()) + " content error(s), first: " + content.report.errors[0].str() + "   (console: errors)");

        // Bars over the enemies, projected by hand: the engine camera matrices belong to the render thread
        {
            std::vector<Hud::EnemyBar> bars;
            const glm::mat4 proj = glm::perspective(fov() * DEG, aspect, 0.1f, 500.0f);
            const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward(), glm::vec3(0, 1, 0));

            for (const auto& f : world.fighters)
            {
                if (f.team != Team::Enemy or not f.alive())
                    continue;

                const auto prev = previous.find(f.id);
                const glm::vec3 pos = prev != previous.end() ? glm::mix(prev->second.position, f.body.position, alpha) : f.body.position;
                const glm::vec4 clip = proj * view * glm::vec4(pos + glm::vec3(0, f.body.height + 0.35f, 0), 1.0f);

                if (clip.w <= 0.1f or glm::length(pos - camera.position) > 22.0f or not f.alive())
                    continue;

                const glm::vec3 ndc = glm::vec3(clip) / clip.w;

                if (std::fabs(ndc.x) > 1.1f or std::fabs(ndc.y) > 1.1f)
                    continue;

                Hud::EnemyBar bar;
                bar.position = glm::vec2((ndc.x * 0.5f + 0.5f) * screenW, (0.5f - ndc.y * 0.5f) * screenH);
                bar.health = f.health / std::max(1.0f, f.maxHealth);
                bar.poise = f.poise / std::max(1.0f, f.maxPoise);
                bar.staggered = f.action == Action::Stagger;
                bars.push_back(bar);
            }

            hud.setEnemyBars(bars);
        }

        // ---- Debug overlay ----
        if (showOverlay and p)
        {
            std::ostringstream o;
            const char* dim = "\\c{150,156,170,255}";
            const char* hi = "\\c{236,238,242,255}";
            const char* warm = "\\c{255,200,110,255}";

            o << hi << "VOXEL ARENA 0.1" << dim << "   fps " << fmt(1.0f / std::max(1e-4f, frameSecondsSmoothed), 0) << "   sim x" << fmt(timeScale, 2) << (paused ? "  PAUSED" : "") << "\n";

            o << hi << "player  " << warm << actionName(p->action) << hi
              << "   hp " << fmt(p->health, 0) << "/" << fmt(p->maxHealth, 0)
              << "   poise " << fmt(p->poise, 0)
              << "   stamina " << fmt(p->stamina, 0) << (p->exhausted ? " (exhausted)" : "")
              << (p->sprinting ? "  sprint" : "") << (world.godMode ? "  GOD" : "") << (world.infiniteStamina ? "  INF STAMINA" : "") << (world.infiniteMana ? "  INF MANA" : "") << "\n";

            const glm::vec3 v = p->body.velocity;
            o << dim << "        pos " << fmt(p->body.position.x) << " " << fmt(p->body.position.y) << " " << fmt(p->body.position.z)
              << "   speed " << fmt(glm::length(glm::vec2(v.x, v.z))) << "   vy " << fmt(v.y)
              << "   ground " << (p->body.onGround ? "yes" : "no") << "   coyote " << fmt(std::max(0.0f, p->coyote), 2)
              << "   jump buffer " << fmt(std::max(0.0f, p->jumpBuffer), 2) << "\n";

            if (const AttackDef* a = world.currentAttack(*p))
            {
                const int frame = std::max(0, p->frame - 1);
                const char* phase = frame < a->startup ? "startup" : (frame < a->startup + a->active ? "ACTIVE" : "recovery");

                o << hi << "        " << a->name << "  frame " << frame + 1 << "/" << a->total() << "  " << warm << phase << hi
                  << dim << "   [" << a->startup << " | " << a->active << " | " << a->recovery << "]"
                  << "   chain " << a->chainFrom << "   dodge " << (a->dodgeCancelFrom < 0 ? std::string("-") : std::to_string(a->dodgeCancelFrom)) << "\n        ";

                // Frame timeline: startup (s), active (A), recovery (r); the current frame lit
                for (int i = 0; i < a->total(); ++i)
                {
                    const char c = i < a->startup ? 's' : (i < a->startup + a->active ? 'A' : 'r');
                    o << (i == frame ? "\\c{255,255,255,255}" : (i < a->startup ? "\\c{120,150,220,255}" : (i < a->startup + a->active ? "\\c{240,90,80,255}" : "\\c{110,120,130,255}"))) << c;
                }
                o << "\n";
            }
            else if (p->action == Action::Dodge)
            {
                const int frame = std::max(0, p->frame - 1);
                o << hi << "        dodge  frame " << frame + 1 << "/" << tuning.dodge.frames << (p->invulnerable(tuning) ? "  " + std::string(warm) + "INVULNERABLE" : "") << "\n";
            }
            else
            {
                o << "\n";
            }

            int shown = 0;
            for (const auto& f : world.fighters)
            {
                if (f.team != Team::Enemy or shown >= 6)
                    continue;

                ++shown;
                o << dim << "#" << f.id << " " << f.type << "  " << hi << actionName(f.action) << dim;

                if (const AttackDef* a = world.currentAttack(f))
                {
                    const int frame = std::max(0, f.frame - 1);
                    o << " " << a->name << " " << frame + 1 << "/" << a->total() << (frame < a->startup ? " windup" : (frame < a->startup + a->active ? " ACTIVE" : " recovery"));
                }
                else if (f.action == Action::Stagger)
                {
                    o << " " << f.frame << "/" << f.actionLength;
                }

                o << "   hp " << fmt(f.health, 0) << "  poise " << fmt(f.poise, 0) << "   ai " << f.aiMove;

                if (not f.aiLastError.empty())
                    o << "  \\c{255,120,110,255}" << f.aiLastError.substr(0, 90);

                o << "\n";
            }

            scriptStats.timer += frameSeconds;

            if (scriptStats.timer >= 1.0f)
            {
                scriptStats.line = std::to_string(brain.stats.calls - scriptStats.calls) + " think() calls/s, " + fmt(static_cast<float>((brain.stats.seconds - scriptStats.seconds) * 1000.0), 2) + " ms/s";
                scriptStats.calls = brain.stats.calls;
                scriptStats.seconds = brain.stats.seconds;
                scriptStats.timer = 0.0f;
            }

            o << dim << "scripts  " << scriptStats.line << "\n";
            o << dim << "terrain  " << world.terrain.damagedCount() << " damaged, " << world.terrain.temporaryCount() << " temporary, " << world.terrain.falling.size() << " falling\n";
            o << dim << "content  " << content.mods.size() << " mods, " << content.report.errors.size() << " errors, " << content.report.warnings.size() << " warnings"
              << "   kills " << enemyKills << "   falls " << playerDeaths << "\n";
            o << dim << "F1 overlay  F2 hitboxes  F5 reload  ` console  " << keyName("nextWeapon") << " weapon  Esc mouse  wheel zoom";

            hud.setOverlay(o.str(), true);
        }
        else
        {
            hud.setOverlay("", false);
        }

        std::vector<std::string> lines(consoleLog.begin(), consoleLog.end());
        hud.setConsole(consoleOpen, lines, consoleLine);
    }

    void ArenaGame::simulate(float frameSeconds)
    {
        if (not paused)
            accumulator += static_cast<double>(frameSeconds) * timeScale;

        int steps = 0;

        while ((not paused and accumulator >= STEP_SECONDS) or stepRequests > 0)
        {
            if (steps >= MAX_STEPS_PER_FRAME)
            {
                // Too far behind (a hitch): drop the rest rather than spiral
                accumulator = 0.0;
                break;
            }

            InputFrame in;
            {
                std::lock_guard<std::mutex> lock(inputMutex);

                if (not consoleOpen)
                {
                    glm::vec2 move(0.0f);
                    move.y += held("forward") ? 1.0f : 0.0f;
                    move.y -= held("back") ? 1.0f : 0.0f;
                    move.x += held("right") ? 1.0f : 0.0f;
                    move.x -= held("left") ? 1.0f : 0.0f;

                    in.move = glm::length(move) > 1.0f ? glm::normalize(move) : move;
                    in.jumpHeld = held("jump");
                    in.sprintHeld = held("sprint");
                    in.jumpPressed = pressed("jump");
                    in.dodgePressed = pressed("dodge");
                    in.lightPressed = pressed("light");
                    in.heavyPressed = pressed("heavy");
                    in.crouchHeld = held("crouch");

                    for (int i = 0; i < InputFrame::ABILITIES; ++i)
                    {
                        const std::string action = "ability" + std::to_string(i + 1);
                        in.abilityPressed[i] = pressed(action);
                        in.abilityHeld[i] = held(action);
                    }
                }

                // A press is handed to one step only
                stepPresses.clear();
                mousePresses.clear();
            }

            auto injected = [&](const std::string& a) { return injectedHolds.count(a) and injectedHolds[a] > 0; };
            in.move.y += injected("forward") ? 1.0f : 0.0f;
            in.move.y -= injected("back") ? 1.0f : 0.0f;
            in.move.x += injected("right") ? 1.0f : 0.0f;
            in.move.x -= injected("left") ? 1.0f : 0.0f;
            in.move = glm::length(in.move) > 1.0f ? glm::normalize(in.move) : in.move;
            in.jumpHeld = in.jumpHeld or injected("jump");
            in.sprintHeld = in.sprintHeld or injected("sprint");
            in.jumpPressed = in.jumpPressed or injectedPresses.count("jump");
            in.dodgePressed = in.dodgePressed or injectedPresses.count("dodge");
            in.lightPressed = in.lightPressed or injectedPresses.count("light");
            in.heavyPressed = in.heavyPressed or injectedPresses.count("heavy");
            in.crouchHeld = in.crouchHeld or injected("crouch");

            for (int i = 0; i < InputFrame::ABILITIES; ++i)
            {
                const std::string action = "ability" + std::to_string(i + 1);
                in.abilityPressed[i] = in.abilityPressed[i] or injectedPresses.count(action);
                in.abilityHeld[i] = in.abilityHeld[i] or injected(action);
            }

            injectedPresses.clear();

            for (auto& [a, n] : injectedHolds)
                n = std::max(0, n - 1);

            if (waitSteps > 0)
                --waitSteps;

            in.equip = pendingEquip;
            pendingEquip = 0;

            in.cameraYaw = camera.yaw;
            in.cameraPitch = camera.pitch;
            in.faceCamera = firstPerson;

            // The abilities aim where the camera looks (the crosshair)
            in.aimOrigin = camera.position;
            in.aimDir = camera.forward();

            previous.clear();
            for (const auto& f : world.fighters)
                previous[f.id] = Pose{f.body.position, f.yaw, f.stepOffset};

            const float vyBefore = world.player() ? world.player()->body.velocity.y : 0.0f;
            const bool groundBefore = world.player() ? world.player()->body.onGround : true;

            // Who steps the world: the server (host), the prediction (client), or nobody else (single)
            if (server)
            {
                server->poll();
                server->step(&in);

                for (const auto& line : server->log)
                    print(line);
                server->log.clear();
            }
            else if (client)
            {
                client->poll(netNowMs());
                client->step(in, netNowMs());
            }
            else
            {
                world.step(in);
            }

            simTime += STEP_SECONDS;

            // A big fall shakes the camera when it lands
            if (const Fighter* p = world.player(); p and p->body.onGround and not groundBefore and vyBefore < -12.0f)
                camera.addTrauma(tuning.feel.landingShake * std::min(1.0f, -vyBefore / 30.0f));

            feedback();

            if (stepRequests > 0)
                --stepRequests;
            else
                accumulator -= STEP_SECONDS;

            ++steps;
        }

        alpha = paused ? 1.0f : static_cast<float>(glm::clamp(accumulator / STEP_SECONDS, 0.0, 1.0));

        // A client draws the others where the snapshots put them (already smooth): no step-to-step blend for them
        if (client)
        {
            client->poll(netNowMs());
            client->updateDisplay(netNowMs());

            for (const auto& f : world.fighters)
                if (f.id != client->playerId)
                    previous[f.id] = Pose{f.body.position, f.yaw, f.stepOffset};

            for (const auto& e : client->takeEvents())
            {
                playEvent(e);

                // Hits on the local player shake its camera (the world's hit list is the server's)
                if (e.fighter == client->playerId and (e.kind == "hit" or e.kind == "stagger" or e.kind == "kill"))
                {
                    camera.addTrauma(e.kind == "hit" ? 0.2f : 0.45f);
                    hurtFlash = std::max(hurtFlash, e.kind == "hit" ? 0.6f : 1.0f);
                }
            }
        }

        // Script tables of enemies that are gone
        if (world.steps % 120 == 0)
        {
            std::vector<int> ids;
            for (const auto& f : world.fighters)
                ids.push_back(f.id);
            brain.forget(ids);
        }
    }

    void ArenaGame::playEvent(const WorldEvent& e)
    {
        // A block set alight burns for `strength` seconds: flames until then (or until it is gone)
        if (e.kind == "ignite")
            burningLooks.push_back({glm::ivec3(glm::floor(e.position)), e.strength, 0.0f});

        auto it = effects.find(e.kind);

        // Along a line (a volt arc): the particles spread from one end to the other
        if (it != effects.end() and it->second.along and e.line)
        {
            const EffectDef& look = it->second;
            const Fighter* p = world.player();
            const glm::vec3 middle = (e.position + e.to) * 0.5f;
            sounds.play(ecsRef, look.sound, p ? glm::length(middle - p->body.position) : 0.0f);

            ParticlePreset one = look.particles;
            one.count = 1;
            std::uniform_real_distribution<float> jitter(-0.12f, 0.12f);

            for (int k = 0; k < look.particles.count; ++k)
            {
                const float t = (static_cast<float>(k) + 0.5f) / static_cast<float>(look.particles.count);
                particles.burst(one, glm::mix(e.position, e.to, t) + glm::vec3(jitter(lookRng), jitter(lookRng), jitter(lookRng)), e.color);
            }

            return;
        }

        playEffect(e.kind, e.position, e.color);
    }

    const EffectDef* ArenaGame::statusLook(const Fighter& f) const
    {
        // The newest shows (frozen over the burn it came with)
        for (auto s = f.statuses.rbegin(); s != f.statuses.rend(); ++s)
            if (auto it = effects.find(s->id); it != effects.end() and it->second.hasTint)
                return &it->second;

        return nullptr;
    }

    bool ArenaGame::heldStill(const Fighter& f) const
    {
        if (f.action != Action::Stagger or not f.heavyStagger)
            return false;

        for (const auto& s : f.statuses)
            if (auto it = content.statuses.find(s.id); it != content.statuses.end() and it->second.heavy and it->second.stagger > 0)
                return true;

        return false;
    }

    void ArenaGame::stepLooks(float frameSeconds)
    {
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);

        // Statuses: particles every so often, from the body
        for (const auto& f : world.fighters)
        {
            for (const auto& s : f.statuses)
            {
                auto it = effects.find(s.id);

                if (it == effects.end() or it->second.every <= 0.0f or it->second.particles.count <= 0)
                    continue;

                float& timer = statusTimers[{f.id, s.id}];
                timer += frameSeconds;

                while (timer >= it->second.every)
                {
                    timer -= it->second.every;
                    const glm::vec3 at = f.body.position + glm::vec3((unit(lookRng) - 0.5f) * f.body.width, unit(lookRng) * f.body.height, (unit(lookRng) - 0.5f) * f.body.width);
                    particles.burst(it->second.particles, at, it->second.particles.color);
                }
            }
        }

        // Timers of statuses gone
        for (auto it = statusTimers.begin(); it != statusTimers.end();)
        {
            const Fighter* f = world.find(it->first.first);
            const bool still = f and std::any_of(f->statuses.begin(), f->statuses.end(), [&](const StatusState& s) { return s.id == it->first.second; });
            it = still ? std::next(it) : statusTimers.erase(it);
        }

        // Blocks on fire: flames over them
        auto flames = effects.find("burning");

        for (auto it = burningLooks.begin(); it != burningLooks.end();)
        {
            it->left -= frameSeconds;
            it->timer += frameSeconds;

            if (it->left <= 0.0f or world.grid.get(it->cell.x, it->cell.y, it->cell.z) == AIR)
            {
                it = burningLooks.erase(it);
                continue;
            }

            while (flames != effects.end() and it->timer >= 0.09f)
            {
                it->timer -= 0.09f;
                const glm::vec3 at = glm::vec3(it->cell) + glm::vec3(unit(lookRng), 0.6f + unit(lookRng) * 0.5f, unit(lookRng));
                particles.burst(flames->second.particles, at, flames->second.particles.color);
            }

            ++it;
        }
    }

    void ArenaGame::playEffect(const std::string& kind, const glm::vec3& at, const glm::vec3& color)
    {
        auto it = effects.find(kind);

        if (it == effects.end())
            return;

        const EffectDef& e = it->second;
        const Fighter* p = world.player();
        const float distance = p ? glm::length(at - p->body.position) : 0.0f;

        sounds.play(ecsRef, e.sound, distance);

        if (e.particles.count > 0)
            particles.burst(e.particles, at, color);

        if (e.shake > 0.0f and distance < 12.0f)
            camera.addTrauma(e.shake * (1.0f - distance / 12.0f));
    }

    void ArenaGame::feedback()
    {
        const Fighter* p = world.player();

        for (const auto& e : world.events)
            playEvent(e);

        world.events.clear();

        for (const auto& h : world.hits)
        {
            const bool involvesPlayer = p and (h.attacker == p->id or h.target == p->id);

            if (involvesPlayer)
                camera.addTrauma(h.shake);

            // Hit: the screen flashes red, more for a stagger or worse
            if (p and h.target == p->id)
                hurtFlash = std::max(hurtFlash, h.killed or h.staggered ? 1.0f : 0.6f);

            if (h.killed)
            {
                if (p and h.target == p->id)
                    ++playerDeaths;
                else
                    ++enemyKills;
            }
        }

        world.hits.clear();

        camera.addTrauma(world.terrainShake);
        world.terrainShake = 0.0f;
        world.terrainEvents = TerrainEvents{};
    }

    void ArenaGame::updateCamera(float frameSeconds)
    {
        float dx = 0.0f, dy = 0.0f, zoom = 0.0f;
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            dx = lookDx;
            dy = lookDy;
            zoom = wheel;
            lookDx = lookDy = wheel = 0.0f;
        }

        if (cursorLocked and not consoleOpen and mouseLook)
        {
            if (firstPerson)
                camera.look(dx, dy, tuning.camera.sensitivity, tuning.firstPerson.minPitch, tuning.firstPerson.maxPitch);
            else
                camera.look(dx, dy, tuning.camera);
        }

        if (not consoleOpen and not firstPerson)
            camera.zoom(zoom, tuning.camera);

        const Fighter* p = world.player();

        if (not p)
            return;

        glm::vec3 feet = p->body.position;
        float stepOffset = p->stepOffset;

        if (auto it = previous.find(p->id); it != previous.end())
        {
            feet = glm::mix(it->second.position, p->body.position, alpha);
            stepOffset = lerp(it->second.stepOffset, p->stepOffset, alpha);
        }

        // A client's own player is drawn where it was predicted, easing into the server's correction
        if (client)
            feet += client->correction;

        if (firstPerson)
        {
            // The view dips with a roll, so a dodge is felt without seeing the body
            float dip = 0.0f;

            if (p->action == Action::Dodge)
            {
                const int rollFrames = std::max(1, tuning.dodge.frames - tuning.dodge.recovery);
                const float t = glm::clamp((static_cast<float>(std::max(0, p->frame - 1)) + alpha) / static_cast<float>(rollFrames), 0.0f, 1.0f);
                dip = tuning.firstPerson.rollDip * std::sin(t * 3.14159f);
            }
            else if (p->action == Action::Land)
            {
                const float t = (static_cast<float>(std::max(0, p->frame - 1)) + alpha) / static_cast<float>(std::max(1, p->actionLength));
                dip = tuning.firstPerson.rollDip * 0.6f * (1.0f - easeOut(t));
            }

            const glm::vec3 eye = feet + glm::vec3(0, tuning.firstPerson.eyeHeight + stepOffset - dip, 0);
            camera.updateFirstPerson(frameSeconds, eye, tuning, simTime);
        }
        else
        {
            const glm::vec3 head = feet + glm::vec3(0, tuning.camera.height + stepOffset, 0);
            // The middle of the body: always free space, the start of the check for a roof over the head
            const glm::vec3 inside = feet + glm::vec3(0, p->body.height * 0.5f, 0);
            camera.update(frameSeconds, head, inside, world.grid, content.blockTable, tuning, simTime);
        }

        auto& cam = masterRenderer->getCamera();
        cam.position = camera.position;
        // Engine camera: front = (cos yaw cos pitch, sin pitch, sin yaw cos pitch)
        cam.yaw = 90.0f - camera.viewYaw;
        cam.pitch = -camera.viewPitch;
        cam.worldUp = glm::vec3(0, 1, 0);
        cam.updateCameraVectors();
    }

    void ArenaGame::setFirstPerson(bool on)
    {
        firstPerson = on;
        masterRenderer->getCamera().setFovDegrees(fov());
        print(on ? "first person" : "third person");
    }

    void ArenaGame::stepGaits(float frameSeconds)
    {
        for (const auto& f : world.fighters)
        {
            Gait& g = gaits[f.id];

            float walkSpeed = tuning.player.walkSpeed;
            if (f.team == Team::Enemy)
                if (auto it = content.enemies.find(f.type); it != content.enemies.end())
                    walkSpeed = std::max(1.0f, it->second.walkSpeed);

            const float speed = glm::length(glm::vec2(f.body.velocity.x, f.body.velocity.z));
            const float target = f.body.onGround ? glm::clamp(speed / walkSpeed, 0.0f, 1.3f) : 0.0f;
            g.amount += (target - g.amount) * (1.0f - std::exp(-10.0f * frameSeconds));

            // A stride about as long as the body is tall
            const float before = std::sin(g.phase);
            g.phase += speed * frameSeconds * 6.2831853f / std::max(0.5f, f.body.height * 0.85f);

            // Footsteps where a foot lands (the swing crosses zero): a sound for the player, dust when running
            if (f.body.onGround and g.amount > 0.35f and (before < 0.0f) != (std::sin(g.phase) < 0.0f))
            {
                const glm::ivec3 below(glm::floor(f.body.position - glm::vec3(0, 0.5f, 0)));
                const BlockId ground = world.grid.get(below.x, below.y, below.z);
                const glm::vec3 dust = (ground < content.blockTable.size() ? content.blockTable[ground].color : glm::vec3(0.7f)) * 0.85f + glm::vec3(0.12f);

                // Sprinting, told by the speed: the same for every player, whoever's machine runs it
                const bool sprinting = speed > (tuning.player.walkSpeed + tuning.player.sprintSpeed) * 0.5f;

                if (f.team == Team::Player)
                    playEffect(sprinting ? "sprint_step" : "step", f.body.position + glm::vec3(0, 0.05f, 0), dust);
                else if (speed > 5.5f)
                    playEffect("run_dust", f.body.position + glm::vec3(0, 0.05f, 0), dust);
            }
        }

        // Forget the fighters that left
        for (auto it = gaits.begin(); it != gaits.end();)
            it = world.find(it->first) ? std::next(it) : gaits.erase(it);
    }

    glm::mat4 ArenaGame::partPose(const std::string& part, const Fighter& f, const Gait& g, float frame) const
    {
        // A limb hangs down: turning it by rotX(-a) swings it forward by a degrees
        auto forward = [](float degrees) { return rotX(-degrees); };

        const float swing = std::sin(g.phase) * 32.0f * g.amount;
        const bool leg = part.rfind("leg", 0) == 0;
        const bool arm = part.rfind("arm", 0) == 0;
        const bool left = part == "leg_l" or part == "arm_l";

        if (part == "head")
        {
            if (f.action == Action::Stagger)
                return rotX(-15.0f);
            return rotX(std::sin(g.phase * 2.0f) * 3.0f * g.amount);
        }

        if (not leg and not arm)
            return glm::mat4(1.0f);

        // Rolling: tucked
        if (f.action == Action::Dodge)
            return forward(leg ? 75.0f : 60.0f);

        // Air dash: legs trailing, arms back
        if (f.action == Action::AirDash)
            return forward(leg ? (left ? -25.0f : -10.0f) : -40.0f);

        if (f.action == Action::Stagger)
            return forward(arm ? -35.0f : 0.0f);

        // Four legs: a trot, diagonal pairs together
        if (part == "leg_fl" or part == "leg_br")
            return forward(swing * 1.2f);
        if (part == "leg_fr" or part == "leg_bl")
            return forward(-swing * 1.2f);

        // In the air: a stride
        if (not f.body.onGround and leg)
            return forward(left ? -20.0f : 25.0f);

        const AttackDef* attack = world.currentAttack(f);

        if (attack and arm)
        {
            const float s = static_cast<float>(attack->startup), act = static_cast<float>(attack->active);

            if (f.team == Team::Player)
            {
                // The weapon arm follows the swing; the other one balances
                if (part == "arm_r")
                {
                    auto w = content.weapons.find(f.weapon);
                    const SwingPose pose = poseAt(*attack, w != content.weapons.end() ? w->second.rest : SwingPose{}, frame);
                    return rotY(-pose.yawFrom * 0.6f) * forward(60.0f + 0.4f * pose.pitchFrom);
                }

                return forward(25.0f);
            }

            if (attack->projectile.enabled)
                return forward(frame < s + act ? 88.0f : 88.0f * (1.0f - (frame - s - act) / std::max(1.0f, static_cast<float>(attack->recovery))));

            // Both arms up through the wind-up, down with the blow, back to rest in the recovery
            if (frame < s)
                return forward(150.0f * easeOut(frame / std::max(1.0f, s)));
            if (frame < s + act + 2.0f)
                return forward(150.0f - 130.0f * easeOut((frame - s) / (act + 2.0f)));
            return forward(20.0f * (1.0f - easeInOut((frame - s - act - 2.0f) / std::max(1.0f, static_cast<float>(attack->recovery)))));
        }

        // Walking: legs in turn, arms against them
        if (leg)
            return forward(left ? swing : -swing);

        const float breathe = std::sin(simTime * 2.0f) * 2.0f;
        return forward((left ? -swing : swing) * 0.8f + breathe);
    }

    std::string ArenaGame::netStatus() const
    {
        if (server)
        {
            std::string out = "hosting, " + std::to_string(server->clientCount()) + " joined";

            for (const auto& f : world.fighters)
                if (f.team == Team::Player)
                    out += "\n  player #" + std::to_string(f.id) + (f.owner == 0 ? " (you)" : " (peer " + std::to_string(f.owner) + ")");

            return out;
        }

        if (client)
            return "joined as #" + std::to_string(client->playerId) + ", round trip " + fmt(client->rttMs, 0) + " ms, " + std::to_string(client->pendingInputs()) + " inputs in flight";

        return "single player";
    }

    void ArenaGame::updatePieceMeshes()
    {
        // Pieces that landed give their mesh back
        for (auto it = pieceMeshes.begin(); it != pieceMeshes.end();)
        {
            const bool alive = std::any_of(world.terrain.falling.begin(), world.terrain.falling.end(), [&](const FallingPiece& p) { return p.id == it->first; });

            if (alive)
            {
                ++it;
                continue;
            }

            freePieceMeshes.push_back(it->second);
            it = pieceMeshes.erase(it);
        }

        for (const auto& piece : world.terrain.falling)
        {
            if (pieceMeshes.count(piece.id))
                continue;

            std::shared_ptr<GpuMesh> mesh;

            if (freePieceMeshes.empty())
            {
                mesh = std::make_shared<GpuMesh>();
            }
            else
            {
                mesh = freePieceMeshes.back();
                freePieceMeshes.pop_back();
            }

            mesh->setData(buildMesh(piece.cells, content.blockTable));
            pieceMeshes[piece.id] = mesh;
        }
    }

    bool ArenaGame::aimed(RayHit& hit) const
    {
        const Fighter* p = world.player();

        if (not p or not p->alive())
            return false;

        float range = 0.0f;
        for (int i = 0; i < InputFrame::ABILITIES; ++i)
            if (const AbilityDef* a = world.ability(i); a and (a->kind == "mine" or a->kind == "pit"))
                range = std::max(range, a->range);

        return range > 0.0f and world.aimedCell(*p, camera.position, camera.forward(), range, hit);
    }

    std::string ArenaGame::keyName(const std::string& action) const
    {
        auto it = controls.find(action);

        if (it == controls.end() or it->second.empty())
            return "?";

        const Binding& b = it->second.front();
        return b.scancode >= 0 ? SDL_GetScancodeName(static_cast<SDL_Scancode>(b.scancode)) : "Mouse" + std::to_string(b.mouseButton);
    }

    std::string ArenaGame::abilityLine() const
    {
        const Fighter* p = world.player();
        std::string out;

        for (int i = 0; i < InputFrame::ABILITIES; ++i)
        {
            const AbilityDef* a = world.ability(i);

            if (not a)
                continue;

            const std::string key = keyName("ability" + std::to_string(i + 1));

            const bool ready = p and p->abilityCooldown[i] <= 0.0f and (a->staminaCost <= 0.0f or p->stamina >= tuning.stamina.minToAct) and
                               (a->manaCost <= 0.0f or p->mana >= a->manaCost);

            // Spells in their element's colour
            std::string colour = "\\c{220,226,236,255}";
            if (a->element == Element::Fire) colour = "\\c{255,170,100,255}";
            if (a->element == Element::Ice) colour = "\\c{170,220,255,255}";
            if (a->element == Element::Volt) colour = "\\c{255,240,140,255}";

            out += (ready ? colour : "\\c{120,124,134,255}") + std::string("[") + key + "] " + a->name + "   ";
        }

        return out;
    }

    void ArenaGame::remeshIfNeeded()
    {
        if (not meshDirty and world.grid.revision() == meshedRevision and world.terrain.revision() == meshedTerrainRevision)
            return;

        // Damaged blocks darken as they get closer to breaking
        MeshOptions opt;
        opt.brightness = [this](int x, int y, int z) { return 1.0f - 0.45f * world.terrain.damageRatio(x, y, z); };

        worldMesh->setData(buildMesh(world.grid, content.blockTable, opt));
        meshedRevision = world.grid.revision();
        meshedTerrainRevision = world.terrain.revision();
        meshDirty = false;
    }

    void ArenaGame::buildRenderCalls()
    {
        renderCallList.clear();

        // The camera this list is drawn with (see GpuMesh: baked into every instance)
        const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward(), glm::vec3(0, 1, 0));

        auto add = [this](const std::shared_ptr<GpuMesh>& mesh, size_t material, OpacityType opacity, std::vector<float> data) {
            if (not mesh or data.empty())
                return;

            RenderCall call(mesh);
            call.batchable = false;
            call.state = defaultState;
            call.setVisibility(true);
            call.setRenderStage(renderStage);
            call.setViewport(0);
            call.setOpacity(opacity);
            call.setDepth(0);
            call.setMaterial(material);
            call.data = std::move(data);

            renderCallList.push_back(std::move(call));
        };

        // Sky box around the camera, inside the far plane
        {
            std::vector<float> d;
            pushInstance(d, view, glm::translate(glm::mat4(1.0f), camera.position - glm::vec3(200.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(400.0f)), WHITE, glm::vec4(0.0f));
            add(skyMesh, skyMaterial, OpacityType::Opaque, std::move(d));
        }

        // The arena
        if (worldMesh->faces() > 0)
        {
            std::vector<float> d;
            pushInstance(d, view, glm::mat4(1.0f), WHITE, glm::vec4(0.0f));
            add(worldMesh, litMaterial, OpacityType::Opaque, std::move(d));
        }

        // Fighters, grouped by model: one draw per model with an instance per fighter
        std::map<std::string, std::vector<float>> instances;
        std::vector<float> trail, danger;   // see-through: slash trails, heavy attack tells

        for (const auto& f : world.fighters)
        {
            std::string modelId;
            float scale = 0.125f;

            if (f.team == Team::Player)
            {
                modelId = tuning.playerModel;
                scale = tuning.player.modelScale;
            }
            else if (auto it = content.enemies.find(f.type); it != content.enemies.end())
            {
                modelId = it->second.model;
                scale = it->second.modelScale;
            }

            auto modelIt = content.models.find(modelId);

            if (modelIt == content.models.end())
                continue;

            const VoxelModel& model = *modelIt->second;

            glm::vec3 pos = f.body.position;
            float yaw = f.yaw;
            float stepOffset = f.stepOffset;

            if (auto it = previous.find(f.id); it != previous.end())
            {
                pos = glm::mix(it->second.position, f.body.position, alpha);
                yaw = lerpAngle(it->second.yaw, f.yaw, alpha);
                stepOffset = lerp(it->second.stepOffset, f.stepOffset, alpha);
            }

            if (client and f.id == client->playerId)
                pos += client->correction;

            float frame = static_cast<float>(std::max(0, f.frame - 1)) + (f.hitstop > 0 ? 0.0f : alpha);

            // On a client, the others are as of the render tick: their frame comes with them
            if (client and f.id != client->playerId)
                if (auto it = client->displayFrame.find(f.id); it != client->displayFrame.end())
                    frame = it->second;

            // Frozen solid: the pose stops where it was
            if (heldStill(f))
                frame = 0.0f;

            const float h = f.body.height;

            const AttackDef* attack = world.currentAttack(f);

            // The body frame at a frame of the action: feet, facing, then the action's pose (a function
            // of the frame, so the slash trail can ask where the body was a few frames ago)
            auto bodyAt = [&](float fr) {
                glm::mat4 body = glm::translate(glm::mat4(1.0f), pos + glm::vec3(0, stepOffset, 0)) * rotY(yaw);

                if (f.action == Action::Dodge)
                {
                    // A forward roll around the body centre
                    const int rollFrames = std::max(1, tuning.dodge.frames - tuning.dodge.recovery);
                    const float t = glm::clamp(fr / static_cast<float>(rollFrames), 0.0f, 1.0f);
                    body = body * glm::translate(glm::mat4(1.0f), glm::vec3(0, h * 0.4f, 0)) * rotX(360.0f * easeInOut(t)) * glm::translate(glm::mat4(1.0f), glm::vec3(0, -h * 0.4f, 0));
                }
                else if (f.action == Action::Stagger)
                {
                    // Rocked back, then recovering
                    const float t = fr / std::max(1.0f, static_cast<float>(f.actionLength));
                    body = body * rotX(-14.0f * std::sin(glm::clamp(t, 0.0f, 1.0f) * 3.14159f) - 4.0f * std::sin(fr * 0.9f) * (1.0f - t));
                }
                else if (f.action == Action::Dead)
                {
                    body = body * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.15f, 0)) * rotX(-90.0f);
                }
                else if (f.action == Action::AirDash)
                {
                    // Leaning into the burst
                    body = body * glm::translate(glm::mat4(1.0f), glm::vec3(0, h * 0.5f, 0)) * rotX(22.0f) * glm::translate(glm::mat4(1.0f), glm::vec3(0, -h * 0.5f, 0));
                }
                else if (f.action == Action::Land)
                {
                    // Squashed by the impact, standing back up
                    const float t = easeOut(fr / std::max(1.0f, static_cast<float>(f.actionLength)));
                    body = body * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f + 0.1f * (1.0f - t), 0.75f + 0.25f * t, 1.0f + 0.1f * (1.0f - t)));
                }
                else if (attack and f.team == Team::Enemy)
                {
                    body = body * rotX(poseAt(*attack, SwingPose{}, fr).pitchFrom);
                }

                // The attack turns the body (a backflip, a spin), about its middle; the weapon follows
                if (attack and attack->turn.to > attack->turn.from)
                {
                    const float t = easeInOut(glm::clamp((fr - static_cast<float>(attack->turn.from)) / static_cast<float>(attack->turn.to - attack->turn.from), 0.0f, 1.0f));
                    body = body * glm::translate(glm::mat4(1.0f), glm::vec3(0, h * 0.5f, 0)) * rotY(attack->turn.yaw * t) * rotX(attack->turn.pitch * t) *
                           glm::translate(glm::mat4(1.0f), glm::vec3(0, -h * 0.5f, 0));
                }

                return body;
            };

            const glm::mat4 body = bodyAt(frame);

            const glm::mat4 modelMatrix = body * glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -model.pivot);

            // Hit flash (white), wind-up glow (orange, growing), stagger (yellow pulse)
            glm::vec4 flash(0.0f);

            const EffectDef* look = statusLook(f);

            // A weapon skill being charged: a glow that builds (full: it pulses)
            const bool charging = attack and attack->charge.at >= 0 and f.chargeFrames > 0 and frame <= static_cast<float>(attack->charge.at + 1);
            const float charge = f.chargeRatio(attack);

            if (charging)
                flash = glm::vec4(0.65f, 0.85f, 1.0f, charge >= 1.0f ? 0.55f + 0.2f * std::sin(simTime * 20.0f) : 0.15f + 0.4f * charge);
            else if (f.flash > 0)
                flash = glm::vec4(1.0f, 1.0f, 1.0f, 0.85f * static_cast<float>(f.flash) / static_cast<float>(std::max(1, tuning.feel.hitFlashFrames)));
            else if (look)
                flash = glm::vec4(look->tint, heldStill(f) ? 0.6f : 0.32f + 0.12f * std::sin(simTime * 9.0f + static_cast<float>(f.id)));
            else if (attack and f.team == Team::Enemy and frame < attack->startup)
                flash = attack->tellHeavy ? glm::vec4(1.0f, 0.12f, 0.08f, 0.65f * easeInOut(frame / std::max(1.0f, static_cast<float>(attack->startup))))
                                          : glm::vec4(1.0f, 0.45f, 0.1f, 0.55f * easeInOut(frame / std::max(1.0f, static_cast<float>(attack->startup))));
            else if (f.action == Action::Stagger)
                flash = glm::vec4(1.0f, 0.9f, 0.3f, 0.25f + 0.15f * std::sin(frame * 0.6f));

            const glm::vec4 tint = f.action == Action::Dead ? glm::vec4(0.6f, 0.6f, 0.6f, 1.0f) : WHITE;

            // In first person the player is the camera: no body, the weapon is drawn as a view model
            const bool viewModel = firstPerson and f.team == Team::Player and f.id == world.localPlayerId;

            if (not viewModel)
            {
                if (model.parts.size() > 1 and f.alive())
                {
                    // Each body part turned about its joint by the gait and the action
                    const Gait& gait = gaits[f.id];

                    for (const auto& part : model.parts)
                    {
                        const glm::mat4 local = glm::translate(glm::mat4(1.0f), part.joint) * partPose(part.name, f, gait, frame) * glm::translate(glm::mat4(1.0f), -part.joint);
                        pushInstance(instances[modelId + "#" + part.name], view, modelMatrix * local, tint, flash);
                    }
                }
                else
                {
                    pushInstance(instances[modelId], view, modelMatrix, tint, flash);
                }
            }

            // The weapon in the hand
            if (f.team == Team::Player)
            {
                auto w = content.weapons.find(f.weapon);
                auto weaponModel = w != content.weapons.end() ? content.models.find(w->second.model) : content.models.end();

                if (w != content.weapons.end() and weaponModel != content.models.end() and f.alive())
                {
                    const WeaponDef& weapon = w->second;

                    // Where the weapon is at a frame of the action, in the hand of the body as it was then
                    auto weaponAt = [&](float fr, const glm::mat4& bodyThen) {
                        SwingPose pose;
                        pose.yawFrom = weapon.rest.yawFrom;
                        pose.pitchFrom = weapon.rest.pitchFrom;

                        if (attack)
                            pose = poseAt(*attack, weapon.rest, fr);

                        // Hand in the body frame; x is the fighter's right, which is the model's -X
                        glm::mat4 handFrame = bodyThen * glm::translate(glm::mat4(1.0f), glm::vec3(-tuning.player.hand.x, tuning.player.hand.y, tuning.player.hand.z)) *
                                              rotX(attack ? f.attackPitch : 0.0f);   // aimed down at a target below, up at one above

                        if (viewModel)
                        {
                            // Held in front of the eye, turning with the view. Scaled down around the eye:
                            // the same picture on screen, but close enough not to poke into walls
                            // A spin (whirlwind, air flurry) turns the body, not the view: the weapon goes round the eye instead
                            float around = 0.0f;
                            if (attack and std::fabs(attack->turn.yaw) > 0.01f and attack->turn.to > attack->turn.from)
                                around = attack->turn.yaw * easeInOut(glm::clamp((fr - static_cast<float>(attack->turn.from)) / static_cast<float>(attack->turn.to - attack->turn.from), 0.0f, 1.0f));

                            const auto& fp = tuning.firstPerson;
                            handFrame = glm::translate(glm::mat4(1.0f), camera.position) * rotY(camera.viewYaw + around) * rotX(camera.viewPitch) *
                                        glm::scale(glm::mat4(1.0f), glm::vec3(fp.viewModelScale)) *
                                        glm::translate(glm::mat4(1.0f), glm::vec3(-fp.weapon.x, fp.weapon.y, fp.weapon.z));
                        }

                        return handFrame *
                            rotY(-pose.yawFrom) * rotX(-pose.pitchFrom) *
                            glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, pose.reach)) *
                            glm::scale(glm::mat4(1.0f), glm::vec3(weapon.modelScale)) *
                            glm::translate(glm::mat4(1.0f), -weapon.grip);
                    };

                    pushInstance(instances[weapon.model], view, weaponAt(frame, body), WHITE, f.flash > 0 or charging ? flash * 0.5f : glm::vec4(0.0f));

                    // The slash: ghosts of the blade along the last frames of the swing, fading, while it
                    // can hit (and a moment after), so the reach and the arc of the attack read at a glance
                    constexpr float TRAIL = 5.0f;

                    if (attack and frame >= static_cast<float>(attack->startup) and frame < static_cast<float>(attack->startup + attack->active + 2))
                    {
                        const VoxelModel& wm = *weaponModel->second;
                        const float bladeFrom = weapon.grip.z + 1.0f;
                        const float bladeLength = std::max(1.0f, static_cast<float>(wm.grid.depth()) - bladeFrom);
                        const float oldest = std::max(static_cast<float>(attack->startup) - 1.0f, frame - TRAIL);

                        for (float fr = frame; fr >= oldest; fr -= 0.34f)
                        {
                            const float age = (frame - fr) / TRAIL;
                            const glm::mat4 ghost = weaponAt(fr, bodyAt(fr)) * glm::translate(glm::mat4(1.0f), glm::vec3(weapon.grip.x - 0.6f, weapon.grip.y - 0.6f, bladeFrom)) *
                                                    glm::scale(glm::mat4(1.0f), glm::vec3(1.2f, 1.2f, bladeLength));
                            // Softer in first person: right in front of the eye
                            pushInstance(trail, view, ghost, glm::vec4(0.9f, 0.95f, 1.0f, (viewModel ? 0.28f : 0.5f) * (1.0f - age)), glm::vec4(0.0f));
                        }
                    }
                }
            }

            // An enemy's heavy attack, winding up: where it will land, filling in as it comes, and a
            // marker over the head
            if (f.team == Team::Enemy and attack and attack->tellHeavy and frame < static_cast<float>(attack->startup + attack->active))
            {
                const float progress = glm::clamp(frame / std::max(1.0f, static_cast<float>(attack->startup)), 0.0f, 1.0f);

                // The lunge still to come moves the strike forward
                const float lungeFrom = std::max(frame, static_cast<float>(attack->lungeFrom));
                const float lungeTo = std::min(static_cast<float>(attack->lungeTo), static_cast<float>(attack->startup + attack->active));
                const float ahead = attack->lunge * std::max(0.0f, lungeTo - lungeFrom) / 60.0f;

                for (const auto& box : attack->hitboxes)
                {
                    const glm::vec3 centre = pos + localToWorld(yaw, box.offset + glm::vec3(0, 0, ahead));
                    const glm::mat4 ground = glm::translate(glm::mat4(1.0f), glm::vec3(centre.x, pos.y + 0.03f, centre.z)) * rotY(yaw);

                    pushInstance(danger, view, ground * glm::scale(glm::mat4(1.0f), glm::vec3(box.size.x, 0.04f, box.size.z)) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0, -0.5f)),
                                 glm::vec4(1.0f, 0.15f, 0.1f, 0.16f + 0.1f * progress), glm::vec4(0.0f));
                    pushInstance(danger, view, ground * glm::scale(glm::mat4(1.0f), glm::vec3(box.size.x * progress, 0.06f, box.size.z * progress)) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0, -0.5f)),
                                 glm::vec4(1.0f, 0.2f, 0.1f, 0.32f), glm::vec4(0.0f));
                }

                const float bob = 0.06f * std::sin(simTime * 10.0f);
                const glm::mat4 marker = glm::translate(glm::mat4(1.0f), pos + glm::vec3(0, h + 0.6f + bob, 0)) * rotY(simTime * 200.0f) * rotX(45.0f) *
                                         glm::rotate(glm::mat4(1.0f), 45.0f * DEG, glm::vec3(0, 0, 1)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.24f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f));
                pushInstance(danger, view, marker, glm::vec4(1.0f, 0.2f, 0.12f, 0.6f + 0.35f * progress), glm::vec4(0.0f));
            }
        }

        add(boxMesh, debugMaterial, OpacityType::Normal, std::move(trail));
        add(boxMesh, debugMaterial, OpacityType::Normal, std::move(danger));

        // Projectiles, along their flight
        for (const auto& pr : world.projectiles)
        {
            auto modelIt = content.models.find(pr.def.model);

            if (modelIt == content.models.end())
                continue;

            const VoxelModel& m = *modelIt->second;
            const glm::vec3 dir = glm::length(pr.velocity) > 1e-4f ? glm::normalize(pr.velocity) : glm::vec3(0, 0, 1);
            const float yaw = std::atan2(dir.x, dir.z) / DEG;
            const float pitch = -std::asin(glm::clamp(dir.y, -1.0f, 1.0f)) / DEG;
            const glm::vec3 centre(m.grid.width() * 0.5f, m.grid.height() * 0.5f, m.grid.depth() * 0.5f);
            const glm::vec3 at = pr.position + pr.velocity * (alpha * STEP_SECONDS);

            const glm::mat4 mm = glm::translate(glm::mat4(1.0f), at) * rotY(yaw) * rotX(pitch) * glm::scale(glm::mat4(1.0f), glm::vec3(pr.def.modelScale)) * glm::translate(glm::mat4(1.0f), -centre);
            pushInstance(instances[pr.def.model], view, mm, WHITE, glm::vec4(0.0f));
        }

        // Particles: little lit cubes, shrinking away
        if (not particles.all().empty())
        {
            std::vector<float> d;
            d.reserve(particles.all().size() * GpuMesh::INSTANCE_FLOATS);

            for (const auto& q : particles.all())
            {
                const float s = q.size * std::sqrt(std::max(0.0f, q.life / q.maxLife));
                const glm::mat4 m = glm::translate(glm::mat4(1.0f), q.position - glm::vec3(s * 0.5f)) * glm::scale(glm::mat4(1.0f), glm::vec3(s));
                pushInstance(d, view, m, glm::vec4(q.color, 1.0f), glm::vec4(0.0f));
            }

            add(boxMesh, litMaterial, OpacityType::Opaque, std::move(d));
        }

        // Falling pieces of the arena
        updatePieceMeshes();

        for (const auto& piece : world.terrain.falling)
        {
            auto it = pieceMeshes.find(piece.id);

            if (it == pieceMeshes.end() or it->second->faces() == 0)
                continue;

            std::vector<float> d;
            pushInstance(d, view, glm::translate(glm::mat4(1.0f), glm::vec3(piece.origin) - glm::vec3(0, piece.drop, 0)), WHITE, glm::vec4(0.0f));
            add(it->second, litMaterial, OpacityType::Opaque, std::move(d));
        }

        // The block the crosshair is on, when an ability can reach it
        if (RayHit hit; not consoleOpen and aimed(hit))
        {
            std::vector<float> d;
            const glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(hit.cell) - glm::vec3(0.02f)) * glm::scale(glm::mat4(1.0f), glm::vec3(1.04f));
            pushInstance(d, view, m, glm::vec4(1.0f, 1.0f, 1.0f, 0.16f), glm::vec4(0.0f));
            add(boxMesh, debugMaterial, OpacityType::Normal, std::move(d));
        }

        for (auto& [id, data] : instances)
        {
            auto mesh = modelMesh(id);

            if (mesh->faces() > 0)
                add(mesh, litMaterial, OpacityType::Opaque, std::move(data));
        }

        // Hitboxes and hurtboxes
        if (showBoxes)
        {
            std::vector<float> data;

            for (const auto& b : world.debugBoxes())
            {
                // The player's own hurtbox would wrap the first person camera
                if (firstPerson and b.hurt and world.player() and b.owner == world.player()->id)
                    continue;

                glm::vec4 color;

                if (b.hurt)
                    color = b.safe ? glm::vec4(0.3f, 0.6f, 1.0f, 0.22f) : glm::vec4(0.3f, 1.0f, 0.4f, 0.16f);
                else
                    color = b.active ? glm::vec4(1.0f, 0.2f, 0.15f, 0.42f) : glm::vec4(1.0f, 0.7f, 0.2f, 0.14f);

                const glm::mat4 m = glm::translate(glm::mat4(1.0f), b.centre) * rotY(b.yaw) * glm::scale(glm::mat4(1.0f), b.size) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f));
                pushInstance(data, view, m, glm::vec4(color.r, color.g, color.b, color.a), glm::vec4(0.0f));
            }

            add(boxMesh, debugMaterial, OpacityType::Normal, std::move(data));
        }

        finishChanges();
    }

    // --------------------------------------------------------------------------------------------
    // Console
    // --------------------------------------------------------------------------------------------

    void ArenaGame::print(const std::string& text)
    {
        if (text.empty())
            return;

        std::istringstream ss(text);
        std::string line;

        while (std::getline(ss, line))
        {
            std::string plain;

            for (size_t i = 0; i < line.size(); ++i)
            {
                if (line[i] == '\\' and i + 1 < line.size() and line[i + 1] == 'c' and line.find('}', i) != std::string::npos)
                    i = line.find('}', i);
                else
                    plain += line[i];
            }

            LOG_INFO(DOM, plain);
            consoleLog.push_back(line);

            while (consoleLog.size() > CONSOLE_LINES)
                consoleLog.pop_front();
        }
    }

    void ArenaGame::consoleKey(int scancode)
    {
        switch (scancode)
        {
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER:
            {
                const std::string line = consoleLine;
                consoleLine.clear();
                historyIndex = -1;

                if (not line.empty())
                {
                    history.push_back(line);
                    print("> " + line);
                    print(runCommand(line));
                }
                break;
            }

            case SDL_SCANCODE_BACKSPACE:
                if (not consoleLine.empty())
                    consoleLine.pop_back();
                break;

            case SDL_SCANCODE_ESCAPE:
                consoleOpen = false;
                break;

            case SDL_SCANCODE_UP:
                if (not history.empty())
                {
                    historyIndex = historyIndex < 0 ? static_cast<int>(history.size()) - 1 : std::max(0, historyIndex - 1);
                    consoleLine = history[historyIndex];
                }
                break;

            case SDL_SCANCODE_DOWN:
                if (historyIndex >= 0)
                {
                    ++historyIndex;

                    if (historyIndex >= static_cast<int>(history.size()))
                    {
                        historyIndex = -1;
                        consoleLine.clear();
                    }
                    else
                    {
                        consoleLine = history[historyIndex];
                    }
                }
                break;

            default:
                break;
        }
    }

    void ArenaGame::runQueue()
    {
        while (waitSteps <= 0 and std::chrono::steady_clock::now() >= sleepUntil and not commandQueue.empty())
        {
            std::string line = commandQueue.front();
            commandQueue.pop_front();

            // "a; b; c" runs one after the other, waits included
            const auto semi = line.find(';');

            if (semi != std::string::npos)
            {
                commandQueue.push_front(line.substr(semi + 1));
                line = line.substr(0, semi);
            }

            print(runCommand(line));
        }
    }

    std::string ArenaGame::runCommand(const std::string& line)
    {
        const auto args = split(line);

        if (args.empty())
            return "";

        const std::string& cmd = args[0];
        Fighter* p = world.player();

        if (cmd == "help")
        {
            return "reload | tune <key> [value] | tunes [prefix] | spawn <enemy> [count] | clear | reset\n"
                   "place x y z <block|air> | fill x0 y0 z0 x1 y1 z1 <block|air>\n"
                   "view [first|third] | weapon <id> | weapons | god | stamina | mana | ai | hitboxes | overlay | pause | step [n] | slowmo <x> | pacer [n|off [fps]] | players | mods | errors | warnings\n"
                   "scripted input: mouse [on|off] | press <action> | hold <action> <steps> | wait <steps> | sleep <seconds> | look <yaw> <pitch> | teleport x y z  (join with ;)";
        }

        if (cmd == "where" and p)
            return "player " + fmt(p->body.position.x, 2) + " " + fmt(p->body.position.y, 2) + " " + fmt(p->body.position.z, 2) + " " + actionName(p->action) + "  yaw " + fmt(p->yaw, 1) + "  stamina " + fmt(p->stamina, 1) +
                   "  cam " + fmt(glm::distance(camera.position, camera.pivot), 2) + " m drop " + fmt(camera.pitchDrop, 1) +
                   "  steps " + std::to_string(world.steps) + "  sim " + fmt(simTime, 2) + " s  real " + fmt(std::chrono::duration<float>(std::chrono::steady_clock::now() - lastReload).count(), 2) + " s";

        if (cmd == "enemies")
        {
            std::string out;
            for (const auto& f : world.fighters)
                if (f.team == Team::Enemy)
                    out += "#" + std::to_string(f.id) + " " + f.type + " hp " + fmt(f.health, 0) + "/" + fmt(f.maxHealth, 0) + " " + actionName(f.action) + "\n";
            return out.empty() ? "no enemies" : out;
        }

        if (cmd == "echo")
            return line.size() > 5 ? line.substr(5) : "";

        if (cmd == "wait")
        {
            waitSteps = args.size() > 1 ? std::max(0, std::atoi(args[1].c_str())) : 60;
            return "";
        }

        if (cmd == "sleep")
        {
            // Real time, unlike wait: lets a script look at a paused frame
            const float seconds = args.size() > 1 ? static_cast<float>(std::atof(args[1].c_str())) : 1.0f;
            sleepUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(seconds * 1000.0f));
            return "";
        }

        if (cmd == "press" and args.size() > 1)
        {
            injectedPresses.insert(args[1]);
            return "";
        }

        if (cmd == "hold" and args.size() > 1)
        {
            injectedHolds[args[1]] = args.size() > 2 ? std::max(0, std::atoi(args[2].c_str())) : 60;
            return "";
        }

        if (cmd == "look" and args.size() > 2)
        {
            camera.yaw += static_cast<float>(std::atof(args[1].c_str()));
            camera.pitch = glm::clamp(camera.pitch + static_cast<float>(std::atof(args[2].c_str())), tuning.camera.minPitch, tuning.camera.maxPitch);
            return "";
        }

        if (cmd == "teleport" and args.size() > 3 and p)
        {
            p->body.position = glm::vec3(std::atof(args[1].c_str()), std::atof(args[2].c_str()), std::atof(args[3].c_str()));
            p->body.velocity = glm::vec3(0.0f);
            previous.erase(p->id);
            return "";
        }

        if ((cmd == "place" and args.size() > 4) or (cmd == "fill" and args.size() > 7))
        {
            const std::string ref = cmd == "place" ? args[4] : args[7];
            const BlockId id = ref == "air" ? AIR : content.block(resolveId(ref, "base"));

            if (ref != "air" and id == AIR)
                return "no block " + ref;

            auto at = [&](size_t i) { return static_cast<int>(std::floor(std::atof(args[i].c_str()))); };

            if (cmd == "place")
                world.grid.set(at(1), at(2), at(3), id);
            else
                world.grid.fill({at(1), at(2), at(3)}, {at(4), at(5), at(6)}, id);

            return "";   // the grid's revision changed: the next frame re-meshes it
        }

        if (cmd == "mouse")
        {
            mouseLook = args.size() > 1 ? args[1] != "off" : not mouseLook;
            return std::string("mouse look ") + (mouseLook ? "on" : "off");
        }

        if (cmd == "view")
        {
            setFirstPerson(args.size() > 1 ? args[1] == "first" : not firstPerson);
            return "";
        }

        if (cmd == "overlay")
        {
            showOverlay = not showOverlay;
            return "";
        }

        if (cmd == "players")
            return netStatus();

        // The ECS pass pacer (see execute): N passes per rendered frame, or off (free running, or capped)
        if (cmd == "pacer")
        {
            if (args.size() > 1 and args[1] == "off")
            {
                const int cap = args.size() > 2 ? std::max(0, std::atoi(args[2].c_str())) : 0;
                ecsRef->setEcsTargetFPS(cap);
                ecsRef->setEcsPassesPerFrame(0);
            }
            else if (args.size() > 1)
            {
                ecsRef->setEcsPassesPerFrame(std::max(1, std::atoi(args[1].c_str())));
            }

            const int n = ecsRef->getEcsPassesPerFrame();

            if (n > 0)
                return "pacer: " + std::to_string(n) + " passes per frame, the game's work on the first";

            const int cap = ecsRef->getEcsTargetFPS();
            return std::string("pacer off: the ECS runs ") + (cap > 0 ? "at most " + std::to_string(cap) + " passes a second" : "as fast as it can") + ", each pass a frame";
        }

        // The world belongs to the server: a client asking to change it would only change its own view
        if (client and (cmd == "reload" or cmd == "spawn" or cmd == "clear" or cmd == "reset" or cmd == "place" or cmd == "fill" or
                        cmd == "god" or cmd == "stamina" or cmd == "mana" or cmd == "ai" or cmd == "tune" or cmd == "teleport" or cmd == "pause" or
                        cmd == "step" or cmd == "slowmo"))
            return cmd + ": only the host can do that";

        // Every machine must run the same content (the fingerprint): no reload once others have joined
        if (server and cmd == "reload" and server->clientCount() > 0)
            return "reload: not while players are connected (their content would no longer match)";

        if (cmd == "reload")
        {
            loadAll(false);
            return "";
        }

        if (cmd == "tune")
        {
            if (args.size() < 2)
                return "tune <key> [value], e.g. tune player.walkSpeed 7 (until the next reload)";

            float value = 0.0f;

            if (args.size() == 2)
                return tuning.get(args[1], value) ? args[1] + " = " + fmt(value, 3) : "no tuning value " + args[1];

            char* end = nullptr;
            value = std::strtof(args[2].c_str(), &end);

            if (end == args[2].c_str())
                return "not a number: " + args[2];

            if (not tuning.set(args[1], value))
                return "no tuning value " + args[1];

            world.tuning = tuning;
            return args[1] + " = " + fmt(value, 3) + " (until the next reload)";
        }

        if (cmd == "tunes")
        {
            const std::string prefix = args.size() > 1 ? args[1] : "";
            std::string out;
            int n = 0;

            auto list = [&](const std::string& path, float v) {
                if (path.rfind(prefix, 0) == 0 and n < 40)
                {
                    out += path + "=" + fmt(v, 2) + "  ";
                    if (++n % 5 == 0)
                        out += "\n";
                }
            };

            tuning.forEachNumber([&](const std::string& path, float& v) { list(path, v); });
            tuning.forEachInt([&](const std::string& path, int& v) { list(path, static_cast<float>(v)); });
            return out.empty() ? "nothing starts with " + prefix : out;
        }

        if (cmd == "spawn")
        {
            if (args.size() < 2)
                return "spawn <enemy> [count]; enemies: " + [&] { std::string s; for (const auto& [id, e] : content.enemies) s += id + " "; return s; }();

            const std::string id = resolveId(args[1], "base");

            if (not content.enemies.count(id))
                return "no enemy " + id;

            const int count = args.size() > 2 ? std::max(1, std::atoi(args[2].c_str())) : 1;
            const glm::vec3 ahead = p ? p->body.position + yawForward(camera.yaw) * 5.0f + glm::vec3(0, 1.0f, 0) : content.arena.playerSpawn;

            for (int i = 0; i < count; ++i)
            {
                const float angle = static_cast<float>(i) * 2.399f;
                const glm::vec3 at = ahead + glm::vec3(std::cos(angle), 0, std::sin(angle)) * (count > 1 ? 1.5f + 0.4f * static_cast<float>(i) : 0.0f);
                world.spawnEnemy(id, glm::clamp(at, glm::vec3(1.5f), glm::vec3(world.grid.width() - 1.5f, world.grid.height() - 2.0f, world.grid.depth() - 1.5f)), camera.yaw + 180.0f);
            }

            return "spawned " + std::to_string(count) + " " + id;
        }

        if (cmd == "clear")
        {
            world.removeEnemies();
            return "enemies removed";
        }

        if (cmd == "reset")
        {
            world.start(content, tuning);
            world.setBrain(&brain);
            meshDirty = true;
            return "arena reset";
        }

        if (cmd == "weapon")
        {
            if (args.size() < 2)
                return "weapon <id>";

            const std::string id = resolveId(args[1], "base");

            // Through the input, like the number keys: a client's choice reaches the server that way
            const auto it = std::find(content.weaponOrder.begin(), content.weaponOrder.end(), id);

            if (it == content.weaponOrder.end())
                return "no weapon " + id;

            pendingEquip = static_cast<int>(it - content.weaponOrder.begin()) + 1;
            return "wielding " + id;
        }

        if (cmd == "weapons")
        {
            std::string out;
            for (size_t i = 0; i < content.weaponOrder.size(); ++i)
                out += "[" + std::to_string(i + 1) + "] " + content.weaponOrder[i] + "  ";
            return out;
        }

        if (cmd == "god")
        {
            world.godMode = not world.godMode;
            return std::string("god mode ") + (world.godMode ? "on" : "off");
        }

        // For testing moves and spells: players spend nothing (on, off, or a toggle)
        if (cmd == "stamina" or cmd == "mana")
        {
            bool& flag = cmd == "stamina" ? world.infiniteStamina : world.infiniteMana;
            flag = args.size() > 1 ? (args[1] == "on" or args[1] == "infinite") : not flag;

            if (flag)
                for (auto& f : world.fighters)
                    if (f.team == Team::Player)
                        (cmd == "stamina" ? f.stamina : f.mana) = cmd == "stamina" ? tuning.stamina.max : tuning.mana.max;

            return "infinite " + cmd + (flag ? " on" : " off");
        }

        if (cmd == "ai")
        {
            world.aiEnabled = not world.aiEnabled;
            return std::string("AI ") + (world.aiEnabled ? "on" : "off");
        }

        if (cmd == "hitboxes")
        {
            showBoxes = not showBoxes;
            return std::string("hitboxes ") + (showBoxes ? "shown" : "hidden");
        }

        if (cmd == "pause")
        {
            paused = not paused;
            return paused ? "paused (step [n] advances frames)" : "running";
        }

        if (cmd == "step")
        {
            paused = true;
            stepRequests += args.size() > 1 ? std::max(1, std::atoi(args[1].c_str())) : 1;
            return "";
        }

        if (cmd == "slowmo")
        {
            timeScale = args.size() > 1 ? glm::clamp(static_cast<float>(std::atof(args[1].c_str())), 0.05f, 4.0f) : (timeScale < 1.0f ? 1.0f : 0.25f);
            return "time scale " + fmt(timeScale, 2);
        }

        if (cmd == "mods")
        {
            std::string out;
            for (const auto& m : content.mods)
                out += m.id + " " + m.version + " (" + m.name + ")  " + m.dir + "\n";
            for (const auto& i : content.report.info)
                if (i.find("replaces") != std::string::npos or i.find("patches") != std::string::npos)
                    out += "  " + i + "\n";
            return out;
        }

        if (cmd == "errors" or cmd == "warnings")
        {
            const auto& list = cmd == "errors" ? content.report.errors : content.report.warnings;

            if (list.empty())
                return "no " + cmd;

            std::string out;
            for (size_t i = 0; i < list.size() and i < 9; ++i)
                out += list[i].str() + "\n";
            if (list.size() > 9)
                out += "... " + std::to_string(list.size() - 9) + " more in the log";
            return out;
        }

        return "unknown command " + cmd + " (help)";
    }
}
