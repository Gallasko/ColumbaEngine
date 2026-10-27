#include "stdafx.h"

#include "arenagame.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "Input/input.h"
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

        void pushInstance(std::vector<float>& data, const glm::mat4& model, const glm::vec4& tint, const glm::vec4& flash)
        {
            const float* m = glm::value_ptr(model);
            data.insert(data.end(), m, m + 16);
            data.insert(data.end(), {tint.r, tint.g, tint.b, tint.a, flash.r, flash.g, flash.b, flash.a});
        }

        /** Default bindings, used for any action the tuning's "controls" section leaves out. */
        const std::vector<std::pair<std::string, std::vector<std::string>>> DEFAULT_CONTROLS = {
            {"forward", {"W", "Up"}}, {"back", {"S", "Down"}}, {"left", {"A", "Left"}}, {"right", {"D", "Right"}},
            {"jump", {"Space"}}, {"dodge", {"Left Shift"}}, {"sprint", {"Left Shift"}},
            {"light", {"Mouse1", "J"}}, {"heavy", {"Mouse3", "K"}},
            {"freeCursor", {"Left Alt"}},
        };
    }

    ArenaGame::ArenaGame(MasterRenderer* masterRenderer, Window* window, const ArenaOptions& options)
        : AbstractRenderer(masterRenderer, RenderStage::Render), window(window), options(options)
    {
        input = window ? const_cast<Input*>(window->getInputHandler()) : nullptr;

        // Under WSLg the cursor warp is a no-op and sends no motion event of its own (see VoxelTest's camera)
        onWSL = std::getenv("WSL_INTEROP") != nullptr or std::getenv("WSL_DISTRO_NAME") != nullptr;

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
            world.start(content, tuning);
            world.setBrain(&brain);
            camera.distance = tuning.camera.distance;
            camera.yaw = world.player() ? world.player()->yaw : 0.0f;
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
        }

        meshDirty = true;
        masterRenderer->getCamera().setFovDegrees(tuning.camera.fov);

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

        if (not cursorLocked)
            return;

        lookDx += static_cast<float>(event.xrel);
        lookDy += static_cast<float>(event.yrel);

        // Keep the hidden cursor away from the edges (a warp on every motion would stutter)
        if (window and not onWSL)
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

        if (want and not onWSL)
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
        lastFrame = now;
        frameSeconds = std::min(frameSeconds, 0.1f);

        if (frameSeconds > 0.0f)
            frameSecondsSmoothed += (frameSeconds - frameSecondsSmoothed) * 0.05f;

        // Mouse presses are latched by the engine; collect them for the next step
        if (input)
        {
            for (int b = 1; b <= 5; ++b)
            {
                if (input->takePress(static_cast<MouseButton>(b)) and not consoleOpen and cursorLocked)
                    mousePresses.insert(b);
            }
        }

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
                case SDL_SCANCODE_ESCAPE: cursorFree = not cursorFree; break;
                default:
                    if (p.scancode >= SDL_SCANCODE_1 and p.scancode <= SDL_SCANCODE_9)
                    {
                        const size_t index = static_cast<size_t>(p.scancode - SDL_SCANCODE_1);

                        if (index < content.weaponOrder.size())
                            world.equip(content.weaponOrder[index]);
                    }
                    break;
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
        remeshIfNeeded();
        buildRenderCalls();
        updateCursorLock();

        // ---- HUD ----
        const Fighter* p = world.player();

        if (p)
        {
            hud.setBars(p->health / std::max(1.0f, p->maxHealth), p->stamina / std::max(1.0f, tuning.stamina.max), p->exhausted);

            std::string weapons;
            for (size_t i = 0; i < content.weaponOrder.size(); ++i)
            {
                const auto& id = content.weaponOrder[i];
                const bool current = id == p->weapon;
                weapons += (current ? "\\c{255,214,120,255}" : "\\c{170,174,184,255}") + std::string("[") + std::to_string(i + 1) + "] " + content.weapons.at(id).name + "   ";
            }
            hud.setWeapon(weapons);

            hud.setCenter(not p->alive() ? "You fell" : (paused ? "Paused" : ""));
        }

        if (content.report.errors.empty())
            hud.setBanner("");
        else
            hud.setBanner(std::to_string(content.report.errors.size()) + " content error(s), first: " + content.report.errors[0].str() + "   (console: errors)");

        // Bars over the enemies, projected by hand: the engine camera matrices belong to the render thread
        {
            std::vector<Hud::EnemyBar> bars;
            const glm::mat4 proj = glm::perspective(tuning.camera.fov * DEG, aspect, 0.1f, 500.0f);
            const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward(), glm::vec3(0, 1, 0));

            for (const auto& f : world.fighters)
            {
                if (f.team != Team::Enemy or not f.alive())
                    continue;

                const auto prev = previous.find(f.id);
                const glm::vec3 pos = prev != previous.end() ? glm::mix(prev->second.position, f.body.position, alpha) : f.body.position;
                const glm::vec4 clip = proj * view * glm::vec4(pos + glm::vec3(0, f.body.height + 0.35f, 0), 1.0f);

                if (clip.w <= 0.1f or glm::length(pos - camera.position) > 22.0f)
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
              << (p->sprinting ? "  sprint" : "") << (world.godMode ? "  GOD" : "") << "\n";

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
            o << dim << "content  " << content.mods.size() << " mods, " << content.report.errors.size() << " errors, " << content.report.warnings.size() << " warnings"
              << "   kills " << enemyKills << "   falls " << playerDeaths << "\n";
            o << dim << "F1 overlay  F2 hitboxes  F5 reload  ` console  1-" << std::max<size_t>(1, content.weaponOrder.size()) << " weapon  Esc mouse  wheel zoom";

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
            injectedPresses.clear();

            for (auto& [a, n] : injectedHolds)
                n = std::max(0, n - 1);

            if (waitSteps > 0)
                --waitSteps;

            in.cameraYaw = camera.yaw;

            previous.clear();
            for (const auto& f : world.fighters)
                previous[f.id] = Pose{f.body.position, f.yaw, f.stepOffset};

            const float vyBefore = world.player() ? world.player()->body.velocity.y : 0.0f;
            const bool groundBefore = world.player() ? world.player()->body.onGround : true;

            world.step(in);
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

        // Script tables of enemies that are gone
        if (world.steps % 120 == 0)
        {
            std::vector<int> ids;
            for (const auto& f : world.fighters)
                ids.push_back(f.id);
            brain.forget(ids);
        }
    }

    void ArenaGame::feedback()
    {
        const Fighter* p = world.player();

        for (const auto& h : world.hits)
        {
            const bool involvesPlayer = p and (h.attacker == p->id or h.target == p->id);

            if (involvesPlayer)
                camera.addTrauma(h.shake);

            if (h.killed)
            {
                if (p and h.target == p->id)
                    ++playerDeaths;
                else
                    ++enemyKills;
            }
        }

        world.hits.clear();
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

        if (cursorLocked and not consoleOpen)
            camera.look(dx, dy, tuning.camera);

        if (not consoleOpen)
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

        const glm::vec3 head = feet + glm::vec3(0, tuning.camera.height + stepOffset, 0);
        camera.update(frameSeconds, head, world.grid, content.blockTable, tuning, simTime);

        auto& cam = masterRenderer->getCamera();
        cam.position = camera.position;
        // Engine camera: front = (cos yaw cos pitch, sin pitch, sin yaw cos pitch)
        cam.yaw = 90.0f - camera.viewYaw;
        cam.pitch = -camera.viewPitch;
        cam.worldUp = glm::vec3(0, 1, 0);
        cam.updateCameraVectors();
    }

    void ArenaGame::remeshIfNeeded()
    {
        if (not meshDirty and world.grid.revision() == meshedRevision)
            return;

        worldMesh->setData(buildMesh(world.grid, content.blockTable));
        meshedRevision = world.grid.revision();
        meshDirty = false;
    }

    void ArenaGame::buildRenderCalls()
    {
        renderCallList.clear();

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
            pushInstance(d, glm::translate(glm::mat4(1.0f), camera.position - glm::vec3(200.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(400.0f)), WHITE, glm::vec4(0.0f));
            add(skyMesh, skyMaterial, OpacityType::Opaque, std::move(d));
        }

        // The arena
        if (worldMesh->faces() > 0)
        {
            std::vector<float> d;
            pushInstance(d, glm::mat4(1.0f), WHITE, glm::vec4(0.0f));
            add(worldMesh, litMaterial, OpacityType::Opaque, std::move(d));
        }

        // Fighters, grouped by model: one draw per model with an instance per fighter
        std::map<std::string, std::vector<float>> instances;

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

            const float frame = static_cast<float>(std::max(0, f.frame - 1)) + (f.hitstop > 0 ? 0.0f : alpha);
            const float h = f.body.height;

            // The body frame: feet, facing, then the action's pose
            glm::mat4 body = glm::translate(glm::mat4(1.0f), pos + glm::vec3(0, stepOffset, 0)) * rotY(yaw);

            const AttackDef* attack = world.currentAttack(f);

            if (f.action == Action::Dodge)
            {
                // A forward roll around the body centre
                const int rollFrames = std::max(1, tuning.dodge.frames - tuning.dodge.recovery);
                const float t = glm::clamp(frame / static_cast<float>(rollFrames), 0.0f, 1.0f);
                body = body * glm::translate(glm::mat4(1.0f), glm::vec3(0, h * 0.4f, 0)) * rotX(360.0f * easeInOut(t)) * glm::translate(glm::mat4(1.0f), glm::vec3(0, -h * 0.4f, 0));
            }
            else if (f.action == Action::Stagger)
            {
                // Rocked back, then recovering
                const float t = frame / std::max(1.0f, static_cast<float>(f.actionLength));
                body = body * rotX(-14.0f * std::sin(glm::clamp(t, 0.0f, 1.0f) * 3.14159f) - 4.0f * std::sin(frame * 0.9f) * (1.0f - t));
            }
            else if (f.action == Action::Dead)
            {
                body = body * glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.15f, 0)) * rotX(-90.0f);
            }
            else if (attack and f.team == Team::Enemy)
            {
                body = body * rotX(poseAt(*attack, SwingPose{}, frame).pitchFrom);
            }

            const glm::mat4 modelMatrix = body * glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -model.pivot);

            // Hit flash (white), wind-up glow (orange, growing), stagger (yellow pulse)
            glm::vec4 flash(0.0f);

            if (f.flash > 0)
                flash = glm::vec4(1.0f, 1.0f, 1.0f, 0.85f * static_cast<float>(f.flash) / static_cast<float>(std::max(1, tuning.feel.hitFlashFrames)));
            else if (attack and f.team == Team::Enemy and frame < attack->startup)
                flash = glm::vec4(1.0f, 0.45f, 0.1f, 0.55f * easeInOut(frame / std::max(1.0f, static_cast<float>(attack->startup))));
            else if (f.action == Action::Stagger)
                flash = glm::vec4(1.0f, 0.9f, 0.3f, 0.25f + 0.15f * std::sin(frame * 0.6f));

            const glm::vec4 tint = f.action == Action::Dead ? glm::vec4(0.6f, 0.6f, 0.6f, 1.0f) : WHITE;

            pushInstance(instances[modelId], modelMatrix, tint, flash);

            // The weapon in the hand
            if (f.team == Team::Player)
            {
                auto w = content.weapons.find(f.weapon);

                if (w != content.weapons.end() and content.models.count(w->second.model) and f.alive())
                {
                    const WeaponDef& weapon = w->second;

                    SwingPose pose;
                    pose.yawFrom = weapon.rest.yawFrom;
                    pose.pitchFrom = weapon.rest.pitchFrom;

                    if (attack)
                        pose = poseAt(*attack, weapon.rest, frame);

                    // Hand in the body frame; x is the fighter's right, which is the model's -X
                    const glm::vec3 hand(-tuning.player.hand.x, tuning.player.hand.y, tuning.player.hand.z);

                    const glm::mat4 weaponMatrix = body * glm::translate(glm::mat4(1.0f), hand) *
                        rotY(-pose.yawFrom) * rotX(-pose.pitchFrom) *
                        glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, pose.reach)) *
                        glm::scale(glm::mat4(1.0f), glm::vec3(weapon.modelScale)) *
                        glm::translate(glm::mat4(1.0f), -weapon.grip);

                    pushInstance(instances[weapon.model], weaponMatrix, WHITE, f.flash > 0 ? flash * 0.5f : glm::vec4(0.0f));
                }
            }
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
                glm::vec4 color;

                if (b.hurt)
                    color = b.safe ? glm::vec4(0.3f, 0.6f, 1.0f, 0.22f) : glm::vec4(0.3f, 1.0f, 0.4f, 0.16f);
                else
                    color = b.active ? glm::vec4(1.0f, 0.2f, 0.15f, 0.42f) : glm::vec4(1.0f, 0.7f, 0.2f, 0.14f);

                const glm::mat4 m = glm::translate(glm::mat4(1.0f), b.centre) * rotY(b.yaw) * glm::scale(glm::mat4(1.0f), b.size) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f));
                pushInstance(data, m, glm::vec4(color.r, color.g, color.b, color.a), glm::vec4(0.0f));
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
                   "weapon <id> | weapons | god | ai | hitboxes | overlay | pause | step [n] | slowmo <x> | mods | errors | warnings\n"
                   "scripted input: press <action> | hold <action> <steps> | wait <steps> | sleep <seconds> | look <yaw> <pitch> | teleport x y z  (join with ;)";
        }

        if (cmd == "where" and p)
            return "player " + fmt(p->body.position.x, 2) + " " + fmt(p->body.position.y, 2) + " " + fmt(p->body.position.z, 2) + " " + actionName(p->action) +
                   "  steps " + std::to_string(world.steps) + "  sim " + fmt(simTime, 2) + " s  real " + fmt(std::chrono::duration<float>(std::chrono::steady_clock::now() - lastReload).count(), 2) + " s";

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

        if (cmd == "overlay")
        {
            showOverlay = not showOverlay;
            return "";
        }

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
            return world.equip(id) ? "wielding " + id : "no weapon " + id;
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
