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
#include <fstream>

#include "Input/input.h"
#include "Audio/audiosystem.h"

#include "Core/net/modfiles.h"
#include "Core/net/client.h"
#include "Core/net/server.h"
#include "Net/enginetransport.h"
#include "Networking/backend_sdl.h"
#include "Networking/network_system.h"
#include "Loaders/stb_image.h"
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
        glm::mat4 rotZ(float degrees) { return glm::rotate(glm::mat4(1.0f), degrees * DEG, glm::vec3(0, 0, 1)); }

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

            // The pose at a frame lands in yawFrom / pitchFrom / reach / rollFrom
            auto mix = [&](float yaw0, float pitch0, float reach0, float roll0, float yaw1, float pitch1, float reach1, float roll1, float t) {
                p.yawFrom = lerp(yaw0, yaw1, t);
                p.pitchFrom = lerp(pitch0, pitch1, t);
                p.reach = lerp(reach0, reach1, t);
                p.rollFrom = lerp(roll0, roll1, t);
            };

            if (frame < s)
            {
                // Wind-up: from rest to the start of the swing, slowing down at the end (a readable pose)
                mix(rest.yawFrom, rest.pitchFrom, 0.0f, rest.rollFrom, a.swing.yawFrom, a.swing.pitchFrom, 0.0f, a.swing.rollFrom, easeOut(frame / std::max(1.0f, s)));
            }
            else if (frame < s + act)
            {
                mix(a.swing.yawFrom, a.swing.pitchFrom, 0.0f, a.swing.rollFrom, a.swing.yawTo, a.swing.pitchTo, a.swing.reach, a.swing.rollTo,
                    easeOut((frame - s + 1.0f) / act));
            }
            else
            {
                const float t = easeInOut((frame - s - act) / r);
                mix(a.swing.yawTo, a.swing.pitchTo, a.swing.reach, a.swing.rollTo, rest.yawFrom, rest.pitchFrom, 0.0f, rest.rollFrom, t);
            }

            return p;
        }

        /**
         * How far a weapon that follows its swing (WeaponDef::followSwing) turns about its handle at a frame of
         * an attack, in degrees: its head (the model's +X side) towards where the swing goes. The swing's own
         * motion and the body's turn (a spin, a flip) give the way; it turns in from its rest roll over the
         * wind-up, the shorter way round, and back over the recovery.
         */
        float swingRoll(const AttackDef& a, float restRoll, float frame)
        {
            auto towards = [](float from, float to, float t) {
                const float d = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
                return from + d * t;
            };

            // In the weapon's frame a falling yaw moves the tip along +X, a rising pitch along +Y. The body's
            // turn counts as the opposite of the swing's (it turns the other way round)
            const float dyaw = a.swing.yawTo - a.swing.yawFrom - a.turn.yaw;
            const float dpitch = a.swing.pitchTo - a.swing.pitchFrom - a.turn.pitch;
            const float mx = -dyaw * std::cos(0.5f * (a.swing.pitchFrom + a.swing.pitchTo) * DEG);
            const float my = dpitch;

            // No motion to lead (a thrust, a still pose): it keeps its rest roll
            if (std::fabs(mx) < 1.0f and std::fabs(my) < 1.0f)
                return restRoll;

            const float roll = std::atan2(my, mx) / DEG;
            const float s = static_cast<float>(a.startup), act = static_cast<float>(a.active), r = static_cast<float>(std::max(1, a.recovery));

            if (frame < s)
                return towards(restRoll, roll, easeOut(frame / std::max(1.0f, s)));
            if (frame < s + act)
                return roll;
            return towards(roll, restRoll, easeInOut((frame - s - act) / r));
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

        /** What the camera sees: its side and near planes (no far one, the fog hides that), taken from projection * view. */
        struct ViewPlanes
        {
            glm::vec4 planes[5];

            explicit ViewPlanes(const glm::mat4& m)
            {
                auto row = [&m](int i) { return glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]); };
                const glm::vec4 w = row(3);
                const glm::vec4 all[5] = {w + row(0), w - row(0), w + row(1), w - row(1), w + row(2)};

                for (int i = 0; i < 5; ++i)
                    planes[i] = all[i] / glm::length(glm::vec3(all[i]));
            }

            /** Whether any of a sphere is in view. */
            bool sees(const glm::vec3& centre, float radius) const
            {
                for (const auto& p : planes)
                    if (glm::dot(glm::vec3(p), centre) + p.w < -radius)
                        return false;

                return true;
            }
        };

        /** Default bindings, used for any action the tuning's "controls" section leaves out. */
        const std::vector<std::pair<std::string, std::vector<std::string>>> DEFAULT_CONTROLS = {
            {"forward", {"W", "Up"}}, {"back", {"S", "Down"}}, {"left", {"A", "Left"}}, {"right", {"D", "Right"}},
            {"jump", {"Space"}}, {"dodge", {"Left Shift"}}, {"sprint", {"Left Shift"}},
            {"light", {"Mouse1", "J"}}, {"heavy", {"Mouse3", "K"}},
            {"freeCursor", {"Left Alt"}}, {"toggleView", {"F", "V"}},
            {"ability1", {"Q"}}, {"ability2", {"E"}}, {"ability3", {"G", "Mouse2"}}, {"ability4", {"R"}},
            {"ability5", {"1"}}, {"ability6", {"2"}}, {"ability7", {"3"}}, {"ability8", {"4"}},
            {"crouch", {"Left Ctrl", "C"}}, {"nextWeapon", {"Tab"}}, {"options", {"O"}}, {"lobby", {"L"}}, {"invite", {"I"}}, {"join", {"J"}}, {"multiplayer", {"M"}},
            // The spellbook's parts, while it is open: elements, forms, modifiers
            {"spellElement1", {"Z"}}, {"spellElement2", {"X"}}, {"spellElement3", {"C"}},
            {"spellForm1", {"A"}}, {"spellForm2", {"S"}}, {"spellForm3", {"D"}}, {"spellForm4", {"F"}},
            {"spellModifier1", {"Q"}}, {"spellModifier2", {"W"}}, {"spellModifier3", {"E"}}, {"spellModifier4", {"R"}}, {"spellModifier5", {"T"}},
        };

        // A controller's buttons (SDL's names, and "lefttrigger" / "righttrigger"); the tuning's "gamepad" section
        // replaces them. The left stick moves, the right one looks; Start is Esc (the options), and on the screens the
        // left stick moves a cursor, A clicks, B is Esc and the d-pad is the arrows
        const std::vector<std::pair<std::string, std::vector<std::string>>> DEFAULT_GAMEPAD = {
            {"jump", {"a"}}, {"dodge", {"b"}}, {"sprint", {"b"}}, {"light", {"x"}}, {"heavy", {"y"}},
            {"ability4", {"leftshoulder"}}, {"ability8", {"rightshoulder"}},
            {"ability5", {"lefttrigger"}}, {"ability6", {"righttrigger"}}, {"ability7", {"dpup"}},
            {"ability1", {"dpleft"}}, {"ability2", {"dpdown"}}, {"ability3", {"dpright"}},
            {"crouch", {"leftstick"}}, {"toggleView", {"rightstick"}}, {"nextWeapon", {"back"}},
        };

        /** A pad button as the HUD names it ("A", "LB", "RT", "UP"). */
        std::string padLabel(int button)
        {
            switch (button)
            {
                case SDL_CONTROLLER_BUTTON_A: return "A";
                case SDL_CONTROLLER_BUTTON_B: return "B";
                case SDL_CONTROLLER_BUTTON_X: return "X";
                case SDL_CONTROLLER_BUTTON_Y: return "Y";
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LB";
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RB";
                case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "L3";
                case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "R3";
                case SDL_CONTROLLER_BUTTON_DPAD_UP: return "UP";
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "DOWN";
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "LEFT";
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "RIGHT";
                case SDL_CONTROLLER_BUTTON_START: return "START";
                case SDL_CONTROLLER_BUTTON_BACK: return "BACK";
                case 100: return "LT";
                case 101: return "RT";
                default: return "?";
            }
        }
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
            m.uniformMap["uBlocks"] = UniformValue(0);
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

        // The spell zones: a flat disc of radius 0.5 facing up, and its edge, a ring (each zone's edge shows where zones overlap)
        {
            constexpr int SIDES = 40;
            MeshData disc, ring;
            auto vertex = [](MeshData& m, float x, float z) {
                m.vertices.insert(m.vertices.end(), {x, 0.0f, z, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f});
            };

            vertex(disc, 0.0f, 0.0f);
            for (int k = 0; k < SIDES; ++k)
            {
                const float a = static_cast<float>(k) * 6.2831853f / static_cast<float>(SIDES);
                vertex(disc, std::cos(a) * 0.5f, std::sin(a) * 0.5f);
                vertex(ring, std::cos(a) * 0.47f, std::sin(a) * 0.47f);
                vertex(ring, std::cos(a) * 0.5f, std::sin(a) * 0.5f);
            }

            // Wound counter clockwise seen from above
            for (int k = 0; k < SIDES; ++k)
            {
                const uint32_t next = static_cast<uint32_t>((k + 1) % SIDES);
                disc.indices.insert(disc.indices.end(), {0, 1 + next, 1 + static_cast<uint32_t>(k)});

                const uint32_t inner = 2 * static_cast<uint32_t>(k), outer = inner + 1, nextInner = 2 * next, nextOuter = nextInner + 1;
                ring.indices.insert(ring.indices.end(), {inner, nextInner, nextOuter, inner, nextOuter, outer});
            }

            discMesh = std::make_shared<GpuMesh>();
            discMesh->setData(std::move(disc));
            ringMesh = std::make_shared<GpuMesh>();
            ringMesh->setData(std::move(ring));
        }

        blockTextures = std::make_shared<GpuTextureArray>();
        worldMesh = std::make_shared<GpuMesh>();
        worldMesh->textures = blockTextures;

        ui.init(ecsRef, masterRenderer, screenW, screenH);

        // In a run, the spellbook builds only from what was bought
        spellbook.owned = [this](const std::string& id) {
            const Fighter* me = world.player();
            return not me or world.owns(*me, id);
        };

        masterRenderer->getCamera().setAspectRatio(aspect);

        lastReload = std::chrono::steady_clock::now();
        loadAll(true);

        if (options.lab)
            commandQueue.push_back("lab");

        for (const auto& c : options.startupCommands)
            commandQueue.push_back(c);
    }

    void ArenaGame::loadAll(bool first)
    {
        const auto start = std::chrono::steady_clock::now();

        const std::string oldArena = first ? "" : toJsonString(content.arena.shapes) + toJsonString(Json::makeNumber(content.arena.size.x * 10000 + content.arena.size.y * 100 + content.arena.size.z));

        content = loadContent(options.mods);
        tuning = readTuning(content.tuning, content.tuningFile, content.report);
        if (not serverMods)
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
                    server->hostName = options.playerName;
                    server->offerMods();

                    // Friends join over the internet when the router opens the ports (UPnP)
                    lanAddress = net::PortMapper::localAddress();
                    if (options.upnp)
                        portMapper.start(options.port, static_cast<uint16_t>(options.port + 1), "Voxel Arena", net::PortMapper::parsePorts(options.upnpPorts));
                }
            }

            camera.distance = tuning.camera.distance;
            camera.yaw = world.player() ? world.player()->yaw : 0.0f;

            // The spellbook: as it was left, else the tuning's ready-made spells
            if (not loadSpellbook(spellbookPath(), book))
                book = SpellbookData{};

            if (book.equipped.empty())
                for (const auto& id : tuning.playerSpells)
                    if (auto s = content.spells.find(id); s != content.spells.end())
                        book.equipped.push_back(s->second.recipe);

            book.equipped.resize(ArenaWorld::SPELL_SLOTS);
            applySpells();

            // The title (in multiplayer, the lobby): the arena or the training ground
            titleOpen = true;

            // The player's options, as they were left
            loadOptions(optionsPath(), prefs);
            readControls();   // with the player's own keys now

            autoPort = options.port;
            hostPortEdit = prefs.hostPort > 0 ? std::to_string(prefs.hostPort) : "";

            // The name the others see: --name, else the one kept in the options
            if ((options.playerName.empty() or options.playerName == "player") and not prefs.name.empty())
                options.playerName = prefs.name;
            if (server)
                server->hostName = options.playerName;
            if (client)
                client->setName(options.playerName);

            firstPerson = prefs.firstPerson;
            showOverlay = prefs.overlay;
            camera.pitch = firstPerson ? 5.0f : camera.pitch;
            applyOptions();

            std::ifstream bestFile(std::filesystem::path(spellbookPath()).parent_path() / "best.txt");
            bestFile >> bestScore;

            // The difficulty levels opened so far, and the one picked last
            std::ifstream levelsFile(std::filesystem::path(userDataDir()) / "levels.txt");
            levelsFile >> levelOpen >> levelPicked;
            pickLevel(levelPicked);

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

        loadBlockLooks();

        // A gem's shape is one of the UI's icons: a name it does not know shows a plain gem, and is said
        for (const auto& [id, u] : content.upgrades)
            if (not u.look.icon.empty() and ui::icon(u.look.icon).empty())
                content.report.warn("upgrades.json", 0, "boon \"" + id + "\": no icon \"" + u.look.icon + "\" for its gem (a plain gem instead)");

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

        if (lab)
        {
            lab->load(labScriptPath(), content, tuning);
            if (labOpen)
                labShow();
        }
    }

    void ArenaGame::loadBlockLooks()
    {
        // Model shaped blocks: their model fitted to one cell (its widest side across it), feet on the floor
        blockModels.assign(content.blocks.size(), MeshData{});

        for (const auto& b : content.blocks)
        {
            auto it = content.models.find(b.model);

            if (b.model.empty() or it == content.models.end())
                continue;

            const VoxelModel& model = *it->second;
            MeshOptions opt;
            opt.origin = model.pivot;
            opt.cellSize = 1.0f / static_cast<float>(std::max(model.grid.width(), model.grid.depth()));
            opt.aoStrength = 0.45f;
            blockModels[b.runtimeId] = buildMesh(model.grid, model.table, opt);
        }

        // Textures: one layer per png, all at the size of the biggest (the smaller ones scaled up, pixels kept)
        struct Image
        {
            int w = 0, h = 0;
            std::vector<uint8_t> rgba;
        };

        std::vector<Image> images;
        int size = 1;

        for (const auto& path : content.blockTextures)
        {
            Image img;
            int channels = 0;

            if (unsigned char* data = stbi_load(path.c_str(), &img.w, &img.h, &channels, STBI_rgb_alpha))
            {
                img.rgba.assign(data, data + static_cast<size_t>(img.w) * img.h * 4);
                stbi_image_free(data);
                size = std::max({size, img.w, img.h});
            }
            else
            {
                content.report.error(path, 0, std::string("cannot read this texture: ") + (stbi_failure_reason() ? stbi_failure_reason() : "unknown error"));
            }

            images.push_back(std::move(img));
        }

        size = std::min(size, 256);
        std::vector<uint8_t> layers(static_cast<size_t>(size) * size * 4 * images.size());

        for (size_t i = 0; i < images.size(); ++i)
        {
            const Image& img = images[i];
            uint8_t* out = &layers[static_cast<size_t>(size) * size * 4 * i];

            for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                uint8_t* px = out + (static_cast<size_t>(y) * size + x) * 4;

                // A png that did not load: a magenta and black checker, so it is seen
                if (img.rgba.empty())
                {
                    const uint8_t on = ((x * 4 / size) + (y * 4 / size)) % 2 == 0 ? 255 : 0;
                    px[0] = on;
                    px[1] = 0;
                    px[2] = on;
                    px[3] = 255;
                    continue;
                }

                const uint8_t* src = &img.rgba[(static_cast<size_t>(y * img.h / size) * img.w + x * img.w / size) * 4];
                std::copy(src, src + 4, px);
            }
        }

        blockTextures->setData(size, static_cast<int>(images.size()), std::move(layers));
    }

    MeshOptions ArenaGame::blockMeshOptions() const
    {
        MeshOptions opt;
        opt.edges = 0.15f;
        opt.blockModels = &blockModels;
        return opt;
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

        // The controller's buttons
        padControls.clear();
        const Json* pad = content.tuning.find("gamepad");
        for (const auto& [action, defaults] : DEFAULT_GAMEPAD)
        {
            std::vector<std::string> names = defaults;
            if (pad)
                if (const Json* v = pad->find(action))
                {
                    names.clear();
                    if (v->isString())
                        names.push_back(v->string);
                    else
                        for (const auto& e : v->array)
                            if (e.isString())
                                names.push_back(e.string);
                }

            for (const auto& n : names)
            {
                int button = n == "lefttrigger" ? PAD_LEFT_TRIGGER : n == "righttrigger" ? PAD_RIGHT_TRIGGER : SDL_GameControllerGetButtonFromString(n.c_str());
                if (button == SDL_CONTROLLER_BUTTON_INVALID)
                    content.report.warn(content.tuningFile, 0, "gamepad: unknown button \"" + n + "\" (a, b, x, y, leftshoulder, lefttrigger, dpup, start...)");
                else
                    padControls[action].push_back(button);
            }
        }

        // The player's own keys (the controls screen): each replaces its action's first key
        for (const auto& [chosen, key] : prefs.keys)
        {
            const std::string action = chosen;
            if (not controls.count(action) and std::none_of(DEFAULT_CONTROLS.begin(), DEFAULT_CONTROLS.end(), [&](const auto& d) { return d.first == action; }))
                continue;

            const Binding b = parse(key);
            if (b.scancode < 0 and b.mouseButton < 0)
                continue;

            auto& list = controls[action];
            if (list.empty())
                list.push_back(b);
            else
                list.front() = b;

            // Not twice in its list (its old second key may be the new first)
            for (size_t i = 1; i < list.size();)
                if (list[i].scancode == b.scancode and list[i].mouseButton == b.mouseButton)
                    list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
                else
                    ++i;
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
        padActive = false;
        keyPresses.push_back({event.key, repeat, event.mod});

        if (not repeat)
            stepPresses.insert(event.key);
    }

    void ArenaGame::onEvent(const OnSDLGamepadPressed& event)
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        const int button = static_cast<int>(event.button);
        if (not padDown.count(button))
        {
            padStepPresses.insert(button);
            padFramePresses.push_back(button);
        }
        padDown.insert(button);
        padId = event.id;
        padActive = true;
    }

    void ArenaGame::onEvent(const OnSDLGamepadReleased& event)
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        padDown.erase(static_cast<int>(event.button));
    }

    void ArenaGame::onEvent(const OnSDLGamepadAxisChanged& event)
    {
        if (event.axis >= 6)
            return;

        std::lock_guard<std::mutex> lock(inputMutex);
        const float v = std::clamp(static_cast<float>(event.value) / 32767.0f, -1.0f, 1.0f);
        padAxis[event.axis] = v;

        // A trigger is a button past half way
        if (event.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT or event.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        {
            const int button = event.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? PAD_LEFT_TRIGGER : PAD_RIGHT_TRIGGER;
            if (v > 0.5f and not padDown.count(button))
            {
                padDown.insert(button);
                padStepPresses.insert(button);
                padFramePresses.push_back(button);
            }
            else if (v < 0.3f)
                padDown.erase(button);
        }

        // A stick moved well off its rest: the controller is in use
        if (std::fabs(v) > 0.5f)
        {
            padActive = true;
            padId = event.id;
        }
    }

    bool ArenaGame::padHeld(const std::string& action) const
    {
        auto it = padControls.find(action);
        if (it == padControls.end())
            return false;
        for (const int b : it->second)
            if (padDown.count(b))
                return true;
        return false;
    }

    bool ArenaGame::padPressed(const std::string& action) const
    {
        auto it = padControls.find(action);
        if (it == padControls.end())
            return false;
        for (const int b : it->second)
            if (padStepPresses.count(b))
                return true;
        return false;
    }

    glm::vec2 ArenaGame::stick(int axisX, int axisY) const
    {
        const glm::vec2 raw(padAxis[axisX], padAxis[axisY]);
        const float length = glm::length(raw);
        constexpr float DEAD = 0.2f;
        if (length <= DEAD)
            return glm::vec2(0.0f);
        // Out of the dead zone, rising from 0 to 1, never past
        return raw / length * std::min(1.0f, (length - DEAD) / (1.0f - DEAD));
    }

    void ArenaGame::rumble(float strength, int ms)
    {
        if (not padActive or padId < 0 or prefs.shake <= 0.0f)
            return;
        if (SDL_GameController* pad = SDL_GameControllerFromInstanceID(padId))
        {
            const auto amount = static_cast<Uint16>(std::clamp(strength * prefs.shake, 0.0f, 1.0f) * 0xFFFF);
            SDL_GameControllerRumble(pad, amount, amount, static_cast<Uint32>(ms));
        }
    }

    void ArenaGame::padFrame(float frameSeconds)
    {
        std::vector<int> presses;
        glm::vec2 move(0.0f);
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            presses.swap(padFramePresses);
            move = stick(SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY);
        }

        if (not padActive)
            return;

        auto key = [&](int scancode) {
            std::lock_guard<std::mutex> lock(inputMutex);
            keyPresses.push_back({scancode, false, 0});
        };

        // On the screens: the left stick moves a cursor (the mouse's place: what is under it lights up), A clicks there,
        // B and Start are Esc, the d-pad is the arrows
        const bool screen = uiTakesClicks() and not consoleOpen;
        const bool opened = screen and not padOnScreen;
        padOnScreen = screen;

        if (screen)
        {
            // A screen just came up: the cursor starts in its middle
            if (padCursorX < 0.0f or opened)
            {
                padCursorX = screenW * 0.5f;
                padCursorY = screenH * 0.5f;
            }
            // Across the window's height in a little over a second at full tilt, slower when only nudged
            const float speed = screenH * 0.8f * frameSeconds;
            padCursorX = std::clamp(padCursorX + (move.x * std::fabs(move.x) * 0.75f + move.x * 0.25f) * speed, 0.0f, screenW - 1.0f);
            padCursorY = std::clamp(padCursorY + (move.y * std::fabs(move.y) * 0.75f + move.y * 0.25f) * speed, 0.0f, screenH - 1.0f);

            {
                std::lock_guard<std::mutex> lock(inputMutex);
                if (glm::length(move) > 0.0f or not presses.empty())
                {
                    mouseX = padCursorX;
                    mouseY = padCursorY;
                }
            }

            for (const int b : presses)
            {
                if (b == SDL_CONTROLLER_BUTTON_A)
                {
                    std::lock_guard<std::mutex> lock(inputMutex);
                    uiClicks.emplace_back(padCursorX, padCursorY);
                }
                else if (b == SDL_CONTROLLER_BUTTON_B or b == SDL_CONTROLLER_BUTTON_START)
                    key(SDL_SCANCODE_ESCAPE);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_UP)
                    key(SDL_SCANCODE_UP);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
                    key(SDL_SCANCODE_DOWN);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
                    key(SDL_SCANCODE_LEFT);
                else if (b == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
                    key(SDL_SCANCODE_RIGHT);
            }
            return;
        }

        // Playing: Start is Esc (the options); the actions the keys handle as keys (the view, the next weapon) go as their key
        for (const int b : presses)
        {
            if (b == SDL_CONTROLLER_BUTTON_START)
            {
                key(SDL_SCANCODE_ESCAPE);
                continue;
            }
            for (const char* action : {"toggleView", "nextWeapon"})
                if (auto p = padControls.find(action); p != padControls.end() and std::find(p->second.begin(), p->second.end(), b) != p->second.end())
                    if (auto k = controls.find(action); k != controls.end() and not k->second.empty() and k->second.front().scancode >= 0)
                        key(k->second.front().scancode);
        }
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

        mouseX = static_cast<float>(event.x);
        mouseY = static_cast<float>(event.y);
        if (std::abs(event.xrel) + std::abs(event.yrel) > 2)
        {
            padActive = false;
            padCursorX = padCursorY = -1.0f;
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
        // The engine's MouseClickSystem is the one reader of the press latch (Input::takePress) for the left
        // and right buttons: reading it here too lost every click that system took first. Its click event
        // comes once per press. The other buttons it leaves alone: execute() reads them.
        std::lock_guard<std::mutex> lock(inputMutex);

        // Rebinding: the button pressed is the new key, nothing else
        if (capturing)
        {
            rebindMouse = static_cast<int>(event.button);
            return;
        }

        // A screen up (options, spellbook, shop, title, the end of a run) takes the clicks
        if (uiTakesClicks())
        {
            uiClicks.emplace_back(event.pos.x, event.pos.y);
            mouseX = event.pos.x;
            mouseY = event.pos.y;
            return;
        }

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
        screenW = event.width;
        screenH = event.height;
        masterRenderer->getCamera().setAspectRatio(aspect);   // the UI follows on its next frame (drawUi)
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
        // The controller's (not while a screen takes it: there it is the cursor)
        if (padHeld(action) and not uiTakesClicks())
            return true;

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
        if (padPressed(action) and not uiTakesClicks())
            return true;

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

        // The mouse was let go (Esc in a browser, another window in front): the game pauses in its options
        if (window->takeCursorLockLost() and not (optionsOpen or bookOpen or shopOpen or titleOpen or endOpen or consoleOpen or labOpen))
            setOptionsOpen(true);

        bool altHeld = false;
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            altHeld = held("freeCursor");
        }

        const bool want = not altHeld and not consoleOpen and not cursorFree and not bookOpen and not optionsOpen and not shopOpen and not titleOpen and not endOpen and not armoryOpen and not difficultyOpen and not labOpen;

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

        // The engine sends clicks for the left and right buttons only: the middle and side buttons are read
        // here, the same way (the new key on the controls screen, or a press for the next step)
        if (input)
        {
            for (const int button : {SDL_BUTTON_MIDDLE, SDL_BUTTON_X1, SDL_BUTTON_X2})
            {
                if (not input->takePress(static_cast<MouseButton>(button)))
                    continue;

                std::lock_guard<std::mutex> lock(inputMutex);
                if (capturing)
                    rebindMouse = button;
                else if (not uiTakesClicks() and cursorLocked and (not window or window->isCursorCaptured()))
                    mousePresses.insert(button);
            }
        }

        // The controller: the screens' cursor, and its presses that are keys there
        padFrame(frameSeconds);

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
            // The debug layers, over any screen (a menu eats every other key); not while a key is being rebound
            if ((p.scancode == SDL_SCANCODE_F1 or p.scancode == SDL_SCANCODE_F2) and not (controlsOpen and not rebinding.empty()))
            {
                if (p.repeat)
                    continue;

                if (p.scancode == SDL_SCANCODE_F1)
                {
                    showOverlay = not showOverlay;
                    prefs.overlay = showOverlay;
                    saveOptions(optionsPath(), prefs);
                }
                else
                    showBoxes = not showBoxes;
                continue;
            }

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

            // The lab: F3 shows and hides its panel; while it is up, it takes its keys (a menu over it keeps them)
            if (p.scancode == SDL_SCANCODE_F3 and not p.repeat and not networked() and not controlsOpen)
            {
                setLabOpen(not labOpen);
                continue;
            }

            if (labOpen and not optionsOpen and not bookOpen)
            {
                bool replay = false;
                if (lab->key(p.scancode, replay))
                {
                    if (replay)
                        labShow();
                    continue;
                }
            }

            // Joining, over the title: typing goes to the field, Tab the other one, Enter joins, Esc closes
            if (titleOpen and joinOpen)
            {
                if (p.scancode == SDL_SCANCODE_ESCAPE)
                    joinOpen = false;
                else if (p.scancode == SDL_SCANCODE_TAB)
                    joinFocus = 1 - joinFocus;
                else if (p.scancode == SDL_SCANCODE_BACKSPACE)
                {
                    std::string& field = joinFocus == 0 ? joinAddress : joinName;
                    if (not field.empty())
                        field.pop_back();
                }
                else if (p.scancode == SDL_SCANCODE_RETURN or p.scancode == SDL_SCANCODE_KP_ENTER)
                    joinNow();
                else if (p.scancode == SDL_SCANCODE_V and (p.mod & KMOD_CTRL))
                    joinAction(screens::JOIN_PASTE);
                continue;
            }

            // The controls: the key pressed is the new one (Esc cancels); else Esc goes back to the options
            if (controlsOpen)
            {
                if (not rebinding.empty())
                {
                    if (p.scancode == SDL_SCANCODE_ESCAPE)
                        rebinding.clear();
                    else if (not p.repeat)
                    {
                        bindKey(rebinding, SDL_GetScancodeName(static_cast<SDL_Scancode>(p.scancode)));
                        rebinding.clear();
                    }
                    capturing = not rebinding.empty();
                }
                else if (p.scancode == SDL_SCANCODE_ESCAPE)
                    controlsOpen = false;
                continue;
            }

            // The options' name field has the keyboard: Enter keeps the name, Esc forgets the change
            if (optionsOpen and optionsScreen.naming)
            {
                if (p.scancode == SDL_SCANCODE_BACKSPACE)
                    optionsScreen.backspace();
                else if (p.scancode == SDL_SCANCODE_RETURN or p.scancode == SDL_SCANCODE_KP_ENTER)
                    rename(optionsScreen.commitName());
                else if (p.scancode == SDL_SCANCODE_ESCAPE)
                    optionsScreen.naming = false;
                continue;
            }

            if (optionsOpen)
            {
                if (p.scancode == SDL_SCANCODE_ESCAPE or (keyIs(p.scancode, "options") and not p.repeat))
                {
                    setOptionsOpen(false);
                    continue;
                }

                const int dx = p.scancode == SDL_SCANCODE_LEFT ? -1 : (p.scancode == SDL_SCANCODE_RIGHT ? 1 : 0);
                const int dy = p.scancode == SDL_SCANCODE_UP ? -1 : (p.scancode == SDL_SCANCODE_DOWN ? 1 : 0);

                if ((dx != 0 or dy != 0) and optionsScreen.key(dx, dy, prefs))
                {
                    applyOptions();
                    saveOptions(optionsPath(), prefs);
                }
                continue;
            }

            if (bookOpen)
            {
                if (p.scancode == SDL_SCANCODE_ESCAPE or (keyIs(p.scancode, "spellbook") and not spellbook.naming and not p.repeat))
                    setBookOpen(false);
                else if (p.scancode == SDL_SCANCODE_BACKSPACE)
                    bookChanged(spellbook.backspace(book));
                else if (p.scancode == SDL_SCANCODE_RETURN or p.scancode == SDL_SCANCODE_KP_ENTER)
                    bookChanged(spellbook.enter(content, book));
                else if (p.scancode == SDL_SCANCODE_DELETE and not spellbook.naming)
                    bookChanged(spellbook.clear(book));
                else if (not spellbook.naming and not p.repeat)
                {
                    // The part keys: elements, forms, modifiers
                    static const std::pair<const char*, int> groups[] = {{"spellElement", 3}, {"spellForm", 4}, {"spellModifier", 5}};
                    for (int g = 0; g < 3; ++g)
                        for (int i = 0; i < groups[g].second; ++i)
                            if (keyIs(p.scancode, groups[g].first + std::to_string(i + 1)))
                                bookChanged(spellbook.part(g, i, content, book));
                }
                continue;
            }

            if (p.repeat)
                continue;

            if (keyIs(p.scancode, "options"))
            {
                setOptionsOpen(true);
                continue;
            }

            if (titleOpen and not networked() and not inviteOpen and keyIs(p.scancode, "multiplayer"))
            {
                inviteOpen = true;
                inviteMessage.clear();
                continue;
            }

            if (titleOpen and not networked() and keyIs(p.scancode, "join"))
            {
                openJoin();
                text.clear();   // the key that opened it is not typed into it
                continue;
            }

            // Inviting, over the title: Esc closes, Enter hosts this game (alone)
            if (titleOpen and inviteOpen)
            {
                if (p.scancode == SDL_SCANCODE_ESCAPE or keyIs(p.scancode, "invite"))
                    inviteOpen = false;
                else if ((p.scancode == SDL_SCANCODE_RETURN or p.scancode == SDL_SCANCODE_KP_ENTER or p.scancode == SDL_SCANCODE_H) and not networked())
                    hostNow();
                else if (p.scancode == SDL_SCANCODE_BACKSPACE and not networked())
                {
                    if (not hostPortEdit.empty())
                        hostPortEdit.pop_back();
                    inviteMessage.clear();
                }
                else if (keyIs(p.scancode, "join") and not networked())
                {
                    inviteOpen = false;
                    openJoin();
                    text.clear();
                }
                continue;
            }

            if (titleOpen and keyIs(p.scancode, "invite"))
            {
                inviteOpen = true;
                inviteMessage.clear();
                continue;
            }

            // The title (the lobby): Enter the arena (a client: ready or not), T the training ground
            if (titleOpen)
            {
                if (p.scancode == SDL_SCANCODE_RETURN or p.scancode == SDL_SCANCODE_KP_ENTER)
                {
                    if (client)
                    {
                        bool mine = client->ready;
                        for (const auto& pl : client->lobby.players)
                            if (pl.playerId == client->playerId)
                                mine = pl.ready;
                        client->setReady(not mine);
                    }
                    else
                        startArena();
                }
                else if (p.scancode == SDL_SCANCODE_T)
                    titleOpen = false;
                else if (p.scancode == SDL_SCANCODE_ESCAPE)
                    setOptionsOpen(true);
                continue;
            }

            // A run: in the shop 1-4 buy, R rerolls, Enter is ready; at its end Enter plays again, T goes training
            if (world.inRun())
            {
                const RunPhase phase = world.run.phase;

                // The difficulty: 1-9 or the arrows and Enter play one, Backspace goes back (only the player who picks)
                if (difficultyOpen)
                {
                    const int levels = content.run.levels();
                    const int open = std::min(levels, levelOpenNow());

                    if (p.scancode >= SDL_SCANCODE_1 and p.scancode <= SDL_SCANCODE_9)
                        pendingPick = InputFrame::DIFFICULTY + static_cast<int>(p.scancode - SDL_SCANCODE_1);
                    else if (p.scancode == SDL_SCANCODE_LEFT or p.scancode == SDL_SCANCODE_UP)
                        difficultyCursor = std::max(0, difficultyCursor - 1);
                    else if (p.scancode == SDL_SCANCODE_RIGHT or p.scancode == SDL_SCANCODE_DOWN)
                        difficultyCursor = std::min(open - 1, difficultyCursor + 1);
                    else if (p.scancode == SDL_SCANCODE_RETURN or p.scancode == SDL_SCANCODE_KP_ENTER)
                        pendingPick = InputFrame::DIFFICULTY + difficultyCursor;
                    else if (p.scancode == SDL_SCANCODE_BACKSPACE)
                        pendingPick = InputFrame::BACK;
                    else if (p.scancode == SDL_SCANCODE_ESCAPE)
                        setOptionsOpen(true);
                    continue;
                }

                // The weapon choice: 1-9 take one; then the first spell (Backspace: back to the weapons)
                if (armoryOpen)
                {
                    const bool spellStep = armoryChoosingSpell();

                    if (p.scancode >= SDL_SCANCODE_1 and p.scancode <= SDL_SCANCODE_9)
                    {
                        pendingPick = (spellStep ? InputFrame::FIRST_SPELL : InputFrame::WEAPON) + static_cast<int>(p.scancode - SDL_SCANCODE_1);
                        armoryBack = false;
                    }
                    else if (p.scancode == SDL_SCANCODE_BACKSPACE and spellStep)
                        armoryBack = true;
                    else if (p.scancode == SDL_SCANCODE_ESCAPE)
                        setOptionsOpen(true);
                    continue;
                }

                if (shopOpen)
                {
                    const bool shift = (p.mod & KMOD_SHIFT) != 0;
                    const Fighter* me = world.player();
                    const int levelPicks = me and me->levelUps > 0 ? static_cast<int>(me->levelOffer.size()) : 0;

                    // The level ups first (1-3 take one), then the shop
                    if (levelPicks > 0)
                    {
                        if (p.scancode >= SDL_SCANCODE_1 and p.scancode <= SDL_SCANCODE_9 and static_cast<int>(p.scancode - SDL_SCANCODE_1) < levelPicks)
                            pendingPick = (shift ? InputFrame::BANISH : InputFrame::LEVEL) + static_cast<int>(p.scancode - SDL_SCANCODE_1);
                        else if (p.scancode == SDL_SCANCODE_R)
                            pendingPick = InputFrame::LEVEL_REROLL;
                        else if (p.scancode == SDL_SCANCODE_ESCAPE)
                            setOptionsOpen(true);
                        continue;
                    }

                    // Then the presents (1-3 take one)
                    const int presentPicks = me and me->presents > 0 ? static_cast<int>(me->presentOffer.size()) : 0;
                    if (presentPicks > 0)
                    {
                        if (p.scancode >= SDL_SCANCODE_1 and p.scancode <= SDL_SCANCODE_9 and static_cast<int>(p.scancode - SDL_SCANCODE_1) < presentPicks)
                            pendingPick = InputFrame::PRESENT + static_cast<int>(p.scancode - SDL_SCANCODE_1);
                        else if (p.scancode == SDL_SCANCODE_ESCAPE)
                            setOptionsOpen(true);
                        continue;
                    }

                    if (p.scancode >= SDL_SCANCODE_1 and p.scancode <= SDL_SCANCODE_4)
                        pendingPick = (shift ? InputFrame::LOCK : InputFrame::BUY) + static_cast<int>(p.scancode - SDL_SCANCODE_1);
                    else if (p.scancode == SDL_SCANCODE_R)
                        pendingPick = InputFrame::REROLL;
                    else if (p.scancode == SDL_SCANCODE_RETURN or p.scancode == SDL_SCANCODE_KP_ENTER)
                        pendingPick = InputFrame::START;
                    else if (p.scancode == SDL_SCANCODE_ESCAPE)
                        setOptionsOpen(true);
                    continue;
                }

                if ((phase == RunPhase::Victory or phase == RunPhase::Defeat) and (p.scancode == SDL_SCANCODE_RETURN or p.scancode == SDL_SCANCODE_KP_ENTER))
                {
                    pendingPick = InputFrame::START;
                    continue;
                }

                if ((phase == RunPhase::Victory or phase == RunPhase::Defeat) and p.scancode == SDL_SCANCODE_T and not client)
                {
                    world.stopRun();
                    print(runCommand("reset"));
                    continue;
                }
            }

            switch (p.scancode)
            {
                case SDL_SCANCODE_F5: print(runCommand("reload")); break;
                // Esc: the options, and the game waits (alone); Esc again goes back. Left Alt held frees the mouse
                case SDL_SCANCODE_ESCAPE: setOptionsOpen(true); break;
                default:
                    // Back to the title (the lobby), outside a run
                    if (keyIs(p.scancode, "lobby") and not world.inRun())
                    {
                        titleOpen = true;
                        break;
                    }

                    if (keyIs(p.scancode, "toggleView"))
                    {
                        setFirstPerson(not firstPerson);
                        prefs.firstPerson = firstPerson;
                        saveOptions(optionsPath(), prefs);
                        break;
                    }

                    // The next weapon (the number keys are abilities)
                    if (keyIs(p.scancode, "nextWeapon") and not content.weaponOrder.empty())
                    {
                        const Fighter* me = world.player();
                        const auto at = me ? std::find(content.weaponOrder.begin(), content.weaponOrder.end(), me->weapon) : content.weaponOrder.end();
                        const size_t n = content.weaponOrder.size();
                        size_t next = at == content.weaponOrder.end() ? 0 : (static_cast<size_t>(at - content.weaponOrder.begin()) + 1) % n;

                        // In a run: the next one owned
                        for (size_t k = 0; k < n and me and not world.owns(*me, content.weaponOrder[next]); ++k)
                            next = (next + 1) % n;

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

        // A screen's clicks; nothing reaches the game
        if (optionsOpen or bookOpen)
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            stepPresses.clear();
            mousePresses.clear();
        }

        if (optionsOpen and optionsScreen.naming and not text.empty())
            optionsScreen.type(text);

        // The Multiplayer panel's port: digits go there, so H and J stay its keys
        if (titleOpen and inviteOpen and not joinOpen and not networked() and not text.empty())
        {
            for (char c : text)
                if (c >= '0' and c <= '9' and hostPortEdit.size() < 5)
                    hostPortEdit += c;
            inviteMessage.clear();
        }

        if (joinOpen and not text.empty())
        {
            std::string& field = joinFocus == 0 ? joinAddress : joinName;
            for (char c : text)
                if (c >= 32 and c < 127 and field.size() < (joinFocus == 0 ? 64u : 24u) and not (joinFocus == 0 and c == ' '))
                    field += c;
            joinMessage.clear();
        }

        if (bookOpen and not text.empty() and spellbook.naming)
        {
            spellbook.type(text);
            bookChanged(false);
        }

        handleUiClicks();

        {
            int button = -1;
            {
                std::lock_guard<std::mutex> lock(inputMutex);
                std::swap(button, rebindMouse);
            }

            if (button > 0 and not rebinding.empty())
            {
                bindKey(rebinding, "Mouse" + std::to_string(button));
                rebinding.clear();
                capturing = false;
            }
        }

        updateShop();

        labTick(frameSeconds);
        runQueue();
        simulate(frameSeconds);
        updateCamera(frameSeconds);
        stepGaits(frameSeconds);
        stepLooks(frameSeconds);
        particles.step(frameSeconds, world.grid, content.blockTable);
        remeshIfNeeded();
        buildRenderCalls();
        updateCursorLock();

        // ---- Debug overlay ----
        const Fighter* p = world.player();

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
            o << dim << "ui       " << ui.piecesDrawn() << " pieces, " << ui.changesSent() << " changes (most in a frame lately " << ui.mostChanges() << ")\n";
            o << dim << "terrain  " << world.terrain.damagedCount() << " damaged, " << world.terrain.temporaryCount() << " temporary, " << world.terrain.falling.size() << " falling\n";
            o << dim << "content  " << content.mods.size() << " mods, " << content.report.errors.size() << " errors, " << content.report.warnings.size() << " warnings"
              << "   kills " << enemyKills << "   falls " << playerDeaths << "\n";
            o << dim << "F1 overlay  F2 hitboxes  F5 reload  ` console  " << keyName("nextWeapon") << " weapon  Esc mouse  wheel zoom";

            overlayText = o.str();
        }
        else
        {
            overlayText.clear();
        }

        drawUi(frameSeconds);
    }

    void ArenaGame::simulate(float frameSeconds)
    {
        // Hosting started from the title: its network is stepped here (the engine's runs on its own)
        if (ownNetwork)
            ownNetwork->execute();

        // Every frame, steps or not: a host alone in its lobby does not step, and a friend joining is
        // only heard here
        if (server)
            pollServer();

        // Paused while the spellbook is open, playing alone (a shared game goes on)
        if (not paused and not ((bookOpen or titleOpen or optionsOpen) and alone()))
            accumulator += static_cast<double>(frameSeconds) * timeScale * tuning.feel.gameSpeed;

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

                if (not consoleOpen and not bookOpen and not optionsOpen and not shopOpen and not labOpen)
                {
                    glm::vec2 move(0.0f);
                    move.y += held("forward") ? 1.0f : 0.0f;
                    move.y -= held("back") ? 1.0f : 0.0f;
                    move.x += held("right") ? 1.0f : 0.0f;
                    move.x -= held("left") ? 1.0f : 0.0f;

                    // The left stick: as far as it is pushed (keys and stick together, never past 1)
                    if (not uiTakesClicks())
                    {
                        const glm::vec2 s = stick(SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY);
                        move += glm::vec2(s.x, -s.y);
                    }

                    in.move = glm::length(move) > 1.0f ? glm::normalize(move) : move;
                    in.jumpHeld = held("jump");
                    in.sprintHeld = held("sprint");
                    in.jumpPressed = pressed("jump");
                    in.dodgePressed = pressed("dodge");
                    in.lightPressed = pressed("light");
                    in.heavyPressed = pressed("heavy");
                    in.lightHeld = held("light");
                    in.heavyHeld = held("heavy");
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
                padStepPresses.clear();
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
            in.lightHeld = in.lightHeld or injected("light");
            in.heavyHeld = in.heavyHeld or injected("heavy");
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
            if (labWaitSteps > 0)
                --labWaitSteps;

            in.equip = pendingEquip;
            pendingEquip = 0;
            in.pick = pendingPick;
            pendingPick = 0;

            in.cameraYaw = camera.yaw;
            in.cameraPitch = camera.pitch;
            in.faceCamera = firstPerson;

            // The abilities aim where the camera looks (the crosshair)
            in.aimOrigin = camera.position;
            in.aimDir = camera.forward();

            // The lab, seen from outside: the player fights towards the stage (face), wherever the camera turns
            if (labOpen and not firstPerson)
                if (const Fighter* me = world.player())
                {
                    const float pitch = glm::radians(labAimPitch);
                    in.cameraYaw = labFaceYaw;
                    in.cameraPitch = labAimPitch;
                    in.aimOrigin = me->body.position + glm::vec3(0, tuning.firstPerson.eyeHeight, 0);
                    in.aimDir = yawForward(labFaceYaw) * std::cos(pitch) - glm::vec3(0, std::sin(pitch), 0);
                }

            previous.clear();
            for (const auto& f : world.fighters)
                previous[f.id] = Pose{f.body.position, f.yaw, f.stepOffset};

            const float vyBefore = world.player() ? world.player()->body.velocity.y : 0.0f;
            const bool groundBefore = world.player() ? world.player()->body.onGround : true;

            // Who steps the world: the server (host), the prediction (client), or nobody else (single)
            if (server)
            {
                pollServer();
                server->step(&in);
            }
            else if (client)
            {
                pollClient();
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
            pollClient();
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
                    rumble(e.kind == "hit" ? 0.4f : 0.8f, e.kind == "hit" ? 140 : 260);
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

    void ArenaGame::pickLevel(int level)
    {
        levelOpen = std::clamp(levelOpen, 0, content.run.levels() - 1);
        levelPicked = std::clamp(level, 0, levelOpen);
        saveLevels();
    }

    void ArenaGame::saveLevels() const
    {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(spellbookPath()).parent_path(), ec);
        std::ofstream(std::filesystem::path(userDataDir()) / "levels.txt") << levelOpen << " " << levelPicked;
    }

    std::vector<std::string> ArenaGame::levelLines(int level) const
    {
        const DifficultyDef& d = content.run.difficulty(level);
        auto pc = [](float v) { return std::to_string(static_cast<int>(std::lround(std::fabs(v) * 100.0f))) + "%"; };

        std::vector<std::string> lines;
        if (d.health != 1.0f) lines.push_back("Enemies have " + pc(d.health - 1.0f) + (d.health > 1.0f ? " more" : " less") + " health");
        if (d.damage != 1.0f) lines.push_back("Enemies deal " + pc(d.damage - 1.0f) + (d.damage > 1.0f ? " more" : " less") + " damage");
        if (d.spawn != 1.0f) lines.push_back(pc(d.spawn - 1.0f) + (d.spawn > 1.0f ? " more" : " fewer") + " enemies");
        if (d.healthGrowth > 0.0f) lines.push_back("Enemy health grows " + pc(d.healthGrowth) + " more each wave");
        if (d.damageGrowth > 0.0f) lines.push_back("Enemy damage grows " + pc(d.damageGrowth) + " each wave");
        if (d.eliteChance > 0.0f) lines.push_back("+" + pc(d.eliteChance) + " elite chance");

        if (lines.empty())
            lines.push_back("The standard run");
        return lines;
    }

    std::string ArenaGame::arenaLine(int level) const
    {
        const int waves = content.run.wavesAt(level);
        int bosses = 0;
        for (const auto& [wave, id] : content.run.bosses)
            bosses += wave <= waves ? 1 : 0;

        const bool last = content.run.bosses.count(waves) > 0;
        std::string out = std::to_string(waves) + " timed waves, a shop between them";

        if (bosses > 1 and last)
            out += ", bosses midway and at the end";
        else if (bosses == 1 and last)
            out += ", a boss at the end";
        else if (bosses > 0)
            out += bosses == 1 ? ", a boss on the way" : ", bosses on the way";

        return out;
    }

    void ArenaGame::drawDifficulty()
    {
        DifficultyView v;
        const int open = levelOpenNow();

        for (int k = 0; k < content.run.levels(); ++k)
        {
            DifficultyView::Level l;
            l.name = content.run.difficulty(k).name;

            // "13 waves, a boss midway"
            const int waves = content.run.wavesAt(k);
            int bosses = 0;
            for (const auto& [wave, id] : content.run.bosses)
                bosses += wave <= waves ? 1 : 0;
            const bool last = content.run.bosses.count(waves) > 0;
            l.waves = std::to_string(waves) + " waves";
            if (bosses > 1 and last)
                l.waves += ", bosses midway and at the end";
            else if (bosses == 1)
                l.waves += last ? ", a boss at the end" : ", a boss midway";
            else if (bosses > 1)
                l.waves += ", " + std::to_string(bosses) + " bosses";

            l.lines = levelLines(k);
            if (k >= open)
                l.locked = "Win " + content.run.difficulty(k - 1).name + " to open";
            v.levels.push_back(l);
        }

        const Fighter* me = world.player();
        v.cursor = difficultyCursor;
        v.choosing = me and world.run.chooser == me->id;
        v.chooserName = playerNameOf(world.run.chooser);
        if (v.chooserName.empty())
            v.chooserName = "The host";
        screens::drawDifficulty(ui, v);
    }

    void ArenaGame::startArena(bool chooseLevel)
    {
        titleOpen = false;
        const uint32_t seed = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        world.run.difficulty = levelPicked;
        world.run.levelsOpen = levelOpen + 1;
        difficultyCursor = levelPicked;

        if (chooseLevel)
            world.chooseDifficulty(seed);
        else
            world.startRun(seed);
        world.setBrain(&brain);
        meshDirty = true;
    }

    void ArenaGame::runHud(Hud::View& v)
    {
        const RunState& r = world.run;
        const Fighter* me = world.player();

        // Outside a run, the next run is at the level picked here (a client's is the host's), among the ones open
        if (not client)
            world.run.levelsOpen = levelOpen + 1;
        if (not client and not world.inRun())
            world.run.difficulty = levelPicked;

        if (r.phase != lastPhase)
        {
            levelJustOpened.clear();

            // The difficulty chosen: kept for next time; the screen comes up on the last one played
            if (lastPhase == RunPhase::Difficulty and r.phase != RunPhase::Off and not client)
                pickLevel(r.difficulty);
            if (r.phase == RunPhase::Difficulty)
                difficultyCursor = std::clamp(r.difficulty, 0, std::max(0, levelOpenNow() - 1));

            if (r.phase == RunPhase::Victory and r.difficulty + 1 < content.run.levels() and r.difficulty + 1 > levelOpen)
            {
                levelOpen = r.difficulty + 1;
                levelJustOpened = content.run.difficulty(levelOpen).name;
                pickLevel(levelOpen);
            }
        }

        // A run just ended: the best score
        if (r.phase != lastPhase and (r.phase == RunPhase::Victory or r.phase == RunPhase::Defeat) and r.score > bestScore)
        {
            bestScore = r.score;
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(spellbookPath()).parent_path(), ec);
            std::ofstream(std::filesystem::path(spellbookPath()).parent_path() / "best.txt") << bestScore;
        }

        lastPhase = r.phase;

        if (r.phase != RunPhase::Fighting and r.phase != RunPhase::Shop)
            headlineWave = 0;

        if (titleOpen or not world.inRun() or r.phase == RunPhase::Armory or r.phase == RunPhase::Difficulty)
            return;

        // The top: the wave and its time, the shards and the level, the score
        v.run = true;
        v.wave = r.wave;
        v.waves = r.waves;
        v.timer = r.timer;
        v.bossWave = r.bossWave;
        v.score = r.score;
        v.arenaName = content.arenaById(content.runArenaId(r.wave, r.phase == RunPhase::Duel)).name;

        if (me)
        {
            v.shards = me->shards;
            v.level = me->level;
            v.xp = me->xp;
            v.xpNext = world.xpFor(me->level);
            v.levelUps = me->levelUps;
        }

        // The others, in co-op
        int mate = 0;
        for (const auto& f : world.fighters)
        {
            if (f.team != Team::Player or (me and f.id == me->id))
                continue;
            ++mate;
            const std::string name = playerNameOf(f.id);
            v.team.push_back({name.empty() ? "P" + std::to_string(mate + 1) : name, f.health / std::max(1.0f, f.maxHealth), not f.alive()});
        }

        // The boss's bar: enraged under its "angryAt"
        const Fighter* boss = r.bossId ? world.find(r.bossId) : nullptr;
        if (boss and boss->alive())
        {
            auto def = content.enemies.find(boss->type);
            v.boss = true;
            v.bossName = def != content.enemies.end() ? def->second.name : boss->type;
            v.bossHealth = boss->health / std::max(1.0f, boss->maxHealth);
            v.bossPoise = boss->poise / std::max(1.0f, boss->maxPoise);
            const float angryAt = def != content.enemies.end() ? static_cast<float>(def->second.params.num("angryAt", 0.0)) : 0.0f;
            v.enraged = angryAt > 0.0f and v.bossHealth < angryAt;

            // Its phases: lines on the bar, and while its shield is up, the crystals still standing
            if (def != content.enemies.end() and def->second.phases.any())
            {
                v.bossPhases = def->second.phases.at;
                v.bossShielded = boss->shielded;
                if (boss->shielded)
                    for (const auto& f : world.fighters)
                        v.crystalsLeft += f.type == def->second.phases.crystal and f.alive() ? 1 : 0;
            }
        }

        // The final duel: its name and a count down to the fight, then FIGHT
        if (r.phase == RunPhase::Duel)
        {
            v.duel = true;
            v.bossWave = false;
            if (r.warmup > 0.0f)
            {
                v.headline = "FINAL DUEL";
                v.countdown = static_cast<int>(std::ceil(r.warmup));
            }
            else if (content.run.duel.duration - r.timer < 1.0f)
            {
                v.headline = "FIGHT!";
            }
        }

        // The wave's end: a moment before the level ups and the shop
        if (r.phase == RunPhase::Shop and r.timer > 0.0f)
        {
            v.headline = "Wave cleared";
            if (me and me->levelUps > 0)
                v.subline = me->levelUps > 1 ? "Level up! x" + std::to_string(me->levelUps) : "Level up!";
        }

        // The wave's name as it starts (a boss's), its last seconds counted down
        if (r.phase == RunPhase::Fighting)
        {
            const RunDef& def = content.run;

            if (r.wave != headlineWave)
            {
                headlineWave = r.wave;
                waveStartedAt = simTime;
            }

            if (simTime - waveStartedAt < 1.5f)
            {
                auto b = def.bosses.find(r.wave);
                auto e = b != def.bosses.end() ? content.enemies.find(b->second) : content.enemies.end();
                v.headline = r.bossWave and e != content.enemies.end() ? e->second.name : "Wave " + std::to_string(r.wave);

                // Into another arena: its name under the wave's
                if (r.wave == 1 or content.runArenaId(r.wave, false) != content.runArenaId(r.wave - 1, false))
                    v.subline = v.arenaName;
            }
            else if (not r.bossWave and r.timer > 0.0f and r.timer <= 3.0f)
            {
                v.countdown = static_cast<int>(std::ceil(r.timer));
            }
        }
    }

    void ArenaGame::edgeArrows(Hud::View& v, const glm::mat4& proj, const glm::mat4& view)
    {
        const Fighter* me = world.player();

        spawnPings.erase(std::remove_if(spawnPings.begin(), spawnPings.end(), [&](const SpawnPing& p) {
            const Fighter* f = world.find(p.fighter);
            return p.left <= 0.0f or not f or not f->alive();
        }), spawnPings.end());

        if (not me or not world.inRun() or not me->alive())
            return;

        // Something near, out of view: an arrow at the left or right edge, level with where it is
        auto point = [&](const glm::vec3& at, Hud::Marker::Kind kind, const std::string& name) {
            const bool always = kind == Hud::Marker::Kind::Boss or kind == Hud::Marker::Kind::Crystal;
            if ((not always and glm::length(at - me->body.position) > 30.0f) or v.markers.size() >= 10)
                return;

            const glm::vec4 clip = proj * view * glm::vec4(at + glm::vec3(0, 0.8f, 0), 1.0f);
            if (clip.w > 0.1f)
            {
                const glm::vec3 ndc = glm::vec3(clip) / clip.w;
                if (std::fabs(ndc.x) <= 1.0f and std::fabs(ndc.y) <= 1.0f)
                    return;   // in view: its spawn column (or itself) shows
            }

            // Its way from the middle of the screen, from the camera's point of view (behind: still left or right)
            const glm::vec4 eye = view * glm::vec4(at, 1.0f);
            glm::vec2 dir(eye.x, -eye.y);
            if (glm::length(dir) < 1e-3f)
                dir = glm::vec2(1.0f, 0.0f);
            dir = glm::normalize(dir);

            Hud::Marker m;
            m.kind = kind;
            m.right = dir.x >= 0.0f;
            m.y = 360.0f + std::clamp(dir.y / std::max(0.2f, std::fabs(dir.x)), -1.0f, 1.0f) * 280.0f;
            m.name = name;
            m.distance = glm::length(at - me->body.position);
            v.markers.push_back(m);
        };

        auto bossName = [&](const std::string& type) {
            auto def = content.enemies.find(type);
            return def != content.enemies.end() ? def->second.name : type;
        };

        for (const auto& m : spawnMarks)
        {
            const auto kind = m.boss ? Hud::Marker::Kind::Boss : (m.elite ? Hud::Marker::Kind::Elite : Hud::Marker::Kind::Spawn);
            std::string name;
            if (m.boss)
                if (auto b = content.run.bosses.find(world.run.wave); b != content.run.bosses.end())
                    name = bossName(b->second);
            point(m.position, kind, name);
        }

        // A shielded boss's crystals, wherever they are (shown as crystals, not as new enemies)
        const Fighter* shielded = world.run.bossId ? world.find(world.run.bossId) : nullptr;
        auto bossDef = shielded ? content.enemies.find(shielded->type) : content.enemies.end();
        const std::string crystal = bossDef != content.enemies.end() ? bossDef->second.phases.crystal : "";

        if (shielded and shielded->shielded and not crystal.empty())
            for (const auto& f : world.fighters)
                if (f.type == crystal and f.alive())
                    point(f.body.position, Hud::Marker::Kind::Crystal, "Crystal");

        for (const auto& ping : spawnPings)
            if (const Fighter* f = world.find(ping.fighter); f and (crystal.empty() or f->type != crystal))
            {
                const bool boss = f->id == world.run.bossId;
                point(f->body.position, boss ? Hud::Marker::Kind::Boss : (f->elite ? Hud::Marker::Kind::Elite : Hud::Marker::Kind::Spawn), boss ? bossName(f->type) : "");
            }

        // The boss, out of view, always
        if (const Fighter* boss = world.run.bossId ? world.find(world.run.bossId) : nullptr; boss and boss->alive())
        {
            const bool pinged = std::any_of(spawnPings.begin(), spawnPings.end(), [&](const SpawnPing& p) { return p.fighter == boss->id; });
            if (not pinged)
                point(boss->body.position, Hud::Marker::Kind::Boss, bossName(boss->type));
        }
    }

    void ArenaGame::updateShop()
    {
        // Open while the run is in its shop (and nothing else is in front)
        const Fighter* me = world.player();
        // (after the moment a wave's end is left to sink in: run.json "intermission")
        shopOpen = me and world.run.phase == RunPhase::Shop and world.run.timer <= 0.0f and not titleOpen and not bookOpen and not optionsOpen;
        armoryOpen = me and world.run.phase == RunPhase::Armory and not titleOpen and not bookOpen and not optionsOpen;
        difficultyOpen = me and world.run.phase == RunPhase::Difficulty and not titleOpen and not bookOpen and not optionsOpen;

        if (shopOpen or armoryOpen or difficultyOpen)
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            stepPresses.clear();
            mousePresses.clear();
        }
    }

    void ArenaGame::setOptionsOpen(bool open)
    {
        if (not open and optionsScreen.naming)
            rename(optionsScreen.commitName());

        optionsOpen = open;
        optionsScreen.setOpen(open);

        if (not open)
        {
            controlsOpen = false;
            rebinding.clear();
            capturing = false;
        }
    }

    void ArenaGame::applyOptions()
    {
        camera.shakeScale = prefs.shake;
        showOverlay = prefs.overlay;
        ecsRef->sendEvent(SetMasterVolume{prefs.volume});

        // The render loop's cap (0: none); the ECS passes follow the frames
        if (window)
            window->renderFrameLimiter.setTargetFPS(prefs.maxFps);

        if (firstPerson != prefs.firstPerson)
            setFirstPerson(prefs.firstPerson);
        else
            masterRenderer->getCamera().setFovDegrees(fov());
    }

    void ArenaGame::applySpells()
    {
        if (client)
            client->setSpells(book.equipped);
        else if (Fighter* me = world.player())
            world.setSpells(*me, book.equipped);
    }

    void ArenaGame::setBookOpen(bool open)
    {
        bookOpen = open;
        spellbook.setOpen(open);
    }

    void ArenaGame::bookChanged(bool spellsChanged)
    {
        if (spellsChanged)
            applySpells();

        saveSpellbook(spellbookPath(), book);
    }

    EndView::Source ArenaGame::damageSource(const std::string& id, int damage) const
    {
        using namespace ui;
        auto upperFirst = [](std::string s) {
            if (not s.empty())
                s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
            return s;
        };

        // A weapon, a spell, a technique, an element combo, a status, an item, an enemy's or an elite's blast
        if (auto w = content.weapons.find(id); w != content.weapons.end())
            return {w->second.name, damage, KIND_WEAPON};
        if (auto sp = content.spells.find(id); sp != content.spells.end())
            return {sp->second.name, damage, elementColour(sp->second.recipe.element)};
        if (auto a = content.abilities.find(id); a != content.abilities.end())
            return {a->second.name, damage, KIND_SKILL};
        if (auto c = content.combos.find(id); c != content.combos.end())
            return {c->second.name, damage, hex(0xC89CFF)};
        if (auto st = content.statuses.find(id); st != content.statuses.end())
            return {upperFirst(st->second.name), damage, KIND_ELEMENT};
        if (auto u = content.upgrades.find(id); u != content.upgrades.end())
            return {u->second.name, damage, KIND_STAT};
        if (auto en = content.enemies.find(id); en != content.enemies.end())
            return {en->second.name + " blasts", damage, WARNING};
        if (auto af = content.affixes.find(id); af != content.affixes.end())
            return {af->second.name + " elites", damage, WARNING};
        if (id == "burst")
            return {"Item bursts", damage, KIND_STAT};
        return {upperFirst(id.substr(id.find(':') == std::string::npos ? 0 : id.find(':') + 1)), damage, MUTED};
    }

    void ArenaGame::playEvent(const WorldEvent& e)
    {
        // A blast: its shockwave over the ground (the event is at its heart, 0.6 m up; its strength is its radius)
        if (e.kind == "blast")
            shockwaves.push_back({e.position - glm::vec3(0, 0.57f, 0), std::max(0.5f, e.strength), 0.0f});

        // The player's own hits: their damage rises off what they hit (a crit's event comes just before its hit's)
        if (e.kind == "crit")
            critOn = e.fighter;
        else if (e.kind == "hit" or e.kind == "stagger" or e.kind == "kill")
        {
            const Fighter* me = world.player();
            const bool crit = critOn == e.fighter;
            critOn = 0;

            if (prefs.damageNumbers and me and e.source == me->id and e.fighter != me->id and e.strength >= 0.5f)
            {
                // A quick run of hits on the same one adds up into one number
                auto same = std::find_if(damageNumbers.begin(), damageNumbers.end(), [&](const DamageNumber& n) { return n.target == e.fighter and n.age < 0.12f; });
                if (same != damageNumbers.end())
                {
                    same->value += e.strength;
                    same->crit = same->crit or crit;
                    same->kill = same->kill or e.kind == "kill";
                }
                else
                {
                    std::uniform_real_distribution<float> jitter(-0.35f, 0.35f);
                    damageNumbers.push_back({e.fighter, e.position + glm::vec3(jitter(lookRng), 0.2f, jitter(lookRng)), e.strength, 0.0f, crit, e.kind == "kill"});
                    if (damageNumbers.size() > 48)
                        damageNumbers.erase(damageNumbers.begin());
                }
            }
        }

        // A run's spawn points, shown until the enemies come out
        if (e.kind == "spawn_warn" or e.kind == "boss_warn" or e.kind == "elite_warn")
            spawnMarks.push_back({e.position, e.strength, e.kind == "boss_warn", e.kind == "elite_warn"});
        else if ((e.kind == "spawn" or e.kind == "boss_spawn") and e.fighter)
            spawnPings.push_back({e.fighter, e.kind == "boss_spawn" ? 4.0f : 2.5f});

        // A block set alight burns for `strength` seconds: flames until then (or until it is gone)
        if (e.kind == "ignite")
            burningLooks.push_back({glm::ivec3(glm::floor(e.position)), e.strength, 0.0f});

        auto it = effects.find(e.kind);

        // Along a line (a volt arc): the particles spread from one end to the other, and a bolt of light along it
        if (it != effects.end() and it->second.along and e.line)
        {
            const EffectDef& look = it->second;
            const Fighter* p = world.player();
            const glm::vec3 middle = (e.position + e.to) * 0.5f;
            playSound(look.sound, middle, p ? glm::length(middle - p->body.position) : 0.0f);

            // The player's own beam in first person leaves from the hand (a chain's jump leaves from who it jumps from)
            const bool fromHand = firstPerson and p and e.source == p->id and glm::length(e.position - (p->body.position + glm::vec3(0, p->body.height * 0.6f, 0))) < 1.2f;
            const glm::vec3 from = fromHand ? handPoint() : e.position;

            if (look.beam.width > 0.0f)
                strikes.push_back({from, e.to, look.beam, 0.0f, static_cast<uint32_t>(lookRng()), fromHand});

            ParticlePreset one = look.particles;
            one.count = 1;
            std::uniform_real_distribution<float> jitter(-0.12f, 0.12f);

            for (int k = 0; k < look.particles.count; ++k)
            {
                const float t = (static_cast<float>(k) + 0.5f) / static_cast<float>(look.particles.count);
                const glm::vec3 at = glm::mix(from, e.to, t);

                // None right in front of the eye: big blocks over the view
                if (fromHand and glm::length(at - camera.position) < 2.0f)
                    continue;

                particles.burst(one, at + glm::vec3(jitter(lookRng), jitter(lookRng), jitter(lookRng)), e.color);
            }

            return;
        }

        // What only its player hears (a shard picked up, a level up, a pick, a buy): the others see it, silent
        static const std::set<std::string> personal = {"pickup", "present", "level_up", "weapon_pick", "level_reroll", "banish", "buy"};
        const Fighter* me = world.player();
        const bool someoneElses = me and e.fighter != 0 and e.fighter != me->id and personal.count(e.kind);

        playEffect(e.kind, e.position, e.color, someoneElses);
    }

    glm::vec3 ArenaGame::handPoint() const
    {
        const glm::vec3 forward = camera.forward();
        glm::vec3 right = glm::cross(forward, glm::vec3(0, 1, 0));
        right = glm::length(right) > 1e-4f ? glm::normalize(right) : glm::vec3(1, 0, 0);
        const glm::vec3 up = glm::cross(right, forward);
        return camera.position + forward * 0.55f + right * 0.22f - up * 0.2f;
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

    const glm::vec3* ArenaGame::effectTint(const std::string& effect) const
    {
        auto it = effects.find(effect);
        return it != effects.end() and it->second.hasTint ? &it->second.tint : nullptr;
    }

    std::shared_ptr<GpuMesh> ArenaGame::iconMesh(const std::string& icon)
    {
        auto& mesh = iconMeshes[icon];
        if (mesh)
            return mesh;

        // The 7 x 7 pixel icon, two voxels deep, its top row up; white, the gem's colour is its tint
        VoxelGrid grid(7, 7, 2);
        grid.outsideSolid = false;
        const auto& rows = ui::icon(icon);
        for (size_t r = 0; r < rows.size() and r < 7; ++r)
            for (size_t c = 0; c < rows[r].size() and c < 7; ++c)
                if (rows[r][c] == '#')
                    for (int z = 0; z < 2; ++z)
                        grid.set(static_cast<int>(c), 6 - static_cast<int>(r), z, 1);

        BlockTable table(2);
        table[0].visible = false;
        MeshOptions opt;
        opt.aoStrength = 0.25f;

        mesh = std::make_shared<GpuMesh>();
        mesh->setData(buildMesh(grid, table, opt));
        return mesh;
    }

    void ArenaGame::stepMeters()
    {
        // The dummies' damage meter: health lost since the step before, added up while hits keep coming (3 s without
        // one starts it over). Health coming back (a dummy mends) is not damage
        for (const auto& f : world.fighters)
        {
            if (f.team != Team::Enemy)
                continue;
            auto def = content.enemies.find(f.type);
            if (def == content.enemies.end() or not def->second.regenerates)
                continue;

            DamageMeter& m = meters[f.id];
            if (m.health >= 0.0f and f.health < m.health)
            {
                if (simTime - m.last > 3.0f)
                {
                    m.total = 0.0f;
                    m.first = simTime;
                }
                m.total += m.health - f.health;
                m.last = simTime;
            }
            m.health = f.health;
        }

        for (auto it = meters.begin(); it != meters.end();)
            it = world.find(it->first) ? std::next(it) : meters.erase(it);
    }

    void ArenaGame::stepLooks(float frameSeconds)
    {
        for (auto& w : shockwaves)
            w.age += frameSeconds;
        shockwaves.erase(std::remove_if(shockwaves.begin(), shockwaves.end(), [](const Shockwave& w) { return w.age >= 0.45f; }), shockwaves.end());

        for (auto& st : strikes)
            st.age += frameSeconds;
        strikes.erase(std::remove_if(strikes.begin(), strikes.end(), [](const StrikeLook& st) { return st.age >= st.look.life; }), strikes.end());

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

        // What the last frame drew that sheds particles: a weapon's element, a Legendary item. Each point has its timer,
        // started at a random moment (points that came together do not burst together)
        std::set<std::tuple<int, std::string, int>> drawn;
        for (const auto& lp : lookPoints)
        {
            auto it = effects.find(lp.effect);
            if (it == effects.end() or it->second.every <= 0.0f or it->second.particles.count <= 0)
                continue;

            const auto key = std::make_tuple(lp.fighter, lp.effect, lp.slot);
            drawn.insert(key);
            auto [timer, fresh] = lookTimers.emplace(key, 0.0f);
            if (fresh)
                timer->second = unit(lookRng) * it->second.every;
            timer->second += frameSeconds * lp.rate;

            while (timer->second >= it->second.every)
            {
                timer->second -= it->second.every;
                particles.burst(it->second.particles, lp.at, it->second.particles.color);
            }
        }
        for (auto it = lookTimers.begin(); it != lookTimers.end();)
            it = drawn.count(it->first) ? std::next(it) : lookTimers.erase(it);

        // Timers of statuses gone
        for (auto it = statusTimers.begin(); it != statusTimers.end();)
        {
            if (it->first.first < 0)
            {
                const bool still = std::any_of(world.zones.begin(), world.zones.end(), [&](const Zone& z) { return z.id == -it->first.first; });
                it = still ? std::next(it) : statusTimers.erase(it);
                continue;
            }

            const Fighter* f = world.find(it->first.first);
            const bool still = f and std::any_of(f->statuses.begin(), f->statuses.end(), [&](const StatusState& s) { return s.id == it->first.second; });
            it = still ? std::next(it) : statusTimers.erase(it);
        }

        // Spell zones: their element's particles all over them
        for (const auto& z : world.zones)
        {
            auto look = effects.find(z.heal > 0.0f ? std::string("zone:heal") : std::string("zone:") + elementName(z.element));

            if (look == effects.end() or look->second.every <= 0.0f or look->second.particles.count <= 0)
                continue;

            float& timer = statusTimers[{-z.id, "zone"}];
            timer += frameSeconds;

            while (timer >= look->second.every)
            {
                timer -= look->second.every;
                const float a = unit(lookRng) * 6.2831853f, r = std::sqrt(unit(lookRng)) * z.radius;
                particles.burst(look->second.particles, z.position + glm::vec3(std::cos(a) * r, 0.1f, std::sin(a) * r), look->second.particles.color);
            }
        }

        // Damage numbers: up and gone in a moment
        for (auto& n : damageNumbers)
            n.age += frameSeconds;
        damageNumbers.erase(std::remove_if(damageNumbers.begin(), damageNumbers.end(), [](const DamageNumber& n) { return n.age > 0.9f; }), damageNumbers.end());

        // Spawn points: shown until they come out (or the run moved on)
        for (auto& m : spawnMarks)
            m.left -= frameSeconds;
        for (auto& ping : spawnPings)
            ping.left -= frameSeconds;
        spawnMarks.erase(std::remove_if(spawnMarks.begin(), spawnMarks.end(), [&](const SpawnMark& m) { return m.left <= 0.0f or world.run.phase != RunPhase::Fighting; }), spawnMarks.end());

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

    void ArenaGame::playSound(const std::string& sound, const glm::vec3& at, float distance)
    {
        // Where it is from where the camera looks: to the side pans it, behind makes it a little quieter.
        // The player's own sounds (steps, swings) stay in the middle
        float pan = 0.0f, behind = 0.0f;
        glm::vec3 to = at - camera.position;
        to.y = 0.0f;

        if (distance > 1.5f and glm::length(to) > 0.5f)
        {
            glm::vec3 forward = camera.forward();
            forward.y = 0.0f;
            forward = glm::length(forward) > 1e-4f ? glm::normalize(forward) : glm::vec3(0, 0, 1);
            to = glm::normalize(to);

            const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
            pan = glm::dot(to, right);
            behind = std::max(0.0f, -glm::dot(to, forward));

            // Fades in over the first metres, so something close by does not jump from ear to ear (fade, not near: a windows.h macro)
            const float fade = std::clamp((distance - 1.5f) / 2.5f, 0.0f, 1.0f);
            pan *= fade;
            behind *= fade;
        }

        sounds.play(ecsRef, sound, distance, pan, behind);
    }

    void ArenaGame::playEffect(const std::string& kind, const glm::vec3& at, const glm::vec3& color, bool silent)
    {
        auto it = effects.find(kind);

        if (it == effects.end())
            return;

        const EffectDef& e = it->second;
        const Fighter* p = world.player();
        const float distance = p ? glm::length(at - p->body.position) : 0.0f;

        if (not silent)
            playSound(e.sound, at, distance);

        if (e.particles.count > 0)
            particles.burst(e.particles, at, color);

        if (e.shake > 0.0f and distance < 12.0f)
            camera.addTrauma(e.shake * (1.0f - distance / 12.0f));

        // What comes with it (one level: an "also" of an "also" is not followed)
        for (const auto& more : e.also)
            if (auto m = effects.find(more); m != effects.end())
            {
                playSound(m->second.sound, at, distance);
                if (m->second.particles.count > 0)
                    particles.burst(m->second.particles, at, color);
            }
    }

    void ArenaGame::feedback()
    {
        const Fighter* p = world.player();
        stepMeters();

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
            {
                hurtFlash = std::max(hurtFlash, h.killed or h.staggered ? 1.0f : 0.6f);
                rumble(h.killed or h.staggered ? 0.8f : 0.4f, h.killed or h.staggered ? 260 : 140);
            }

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

        dx *= prefs.sensitivity;
        dy *= prefs.sensitivity * (prefs.invertY ? -1.0f : 1.0f);

        // The right stick looks: up to 240 degrees a second across, 170 up and down, finer near its middle (the mouse's
        // sensitivity and invert Y apply)
        glm::vec2 look(0.0f);
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            look = stick(SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY);
        }
        if (glm::length(look) > 0.0f and not uiTakesClicks() and not consoleOpen)
        {
            const float perPixel = std::max(1e-3f, tuning.camera.sensitivity);
            const float k = prefs.sensitivity * frameSeconds / perPixel;
            const float px = look.x * std::fabs(look.x) * 240.0f * k;
            const float py = look.y * std::fabs(look.y) * 170.0f * k * (prefs.invertY ? -1.0f : 1.0f);
            if (firstPerson)
                camera.look(px, py, tuning.camera.sensitivity, tuning.firstPerson.minPitch, tuning.firstPerson.maxPitch);
            else
                camera.look(px, py, tuning.camera);
        }

        // The lab: the mouse is free. A left drag over the arena turns the camera, the wheel over the panel scrolls
        // its list, and the camera may turn round the player by itself (orbit)
        if (labOpen)
        {
            float mx = 0.0f, my = 0.0f;
            {
                std::lock_guard<std::mutex> lock(inputMutex);
                mx = mouseX;
                my = mouseY;
            }

            if (lab->covers(ui, mx, my))
            {
                lab->scroll(zoom);
                zoom = 0.0f;
            }
            else if (zoom != 0.0f)
                labOwnCamera = true;

            const bool down = input and input->isButtonPressed(static_cast<MouseButton>(SDL_BUTTON_LEFT));
            if (not down)
                labDrag = LabDrag::None;
            else if (labDrag == LabDrag::None)
            {
                labDrag = lab->covers(ui, mx, my) ? LabDrag::OnPanel : LabDrag::Turning;   // a press on the panel turns nothing
                labDragX = mx;
                labDragY = my;

                // The player takes the camera: it stops turning by itself, and the next picks leave it there
                if (labDrag == LabDrag::Turning)
                {
                    labOrbitChoice = LabOrbitChoice::Off;
                    labOwnCamera = true;
                }
            }
            else if (labDrag == LabDrag::Turning)
            {
                camera.yaw += (mx - labDragX) * 0.35f;
                camera.pitch = glm::clamp(camera.pitch + (my - labDragY) * 0.25f, firstPerson ? tuning.firstPerson.minPitch : tuning.camera.minPitch,
                                          firstPerson ? tuning.firstPerson.maxPitch : tuning.camera.maxPitch);
                labDragX = mx;
                labDragY = my;
            }

            if (not firstPerson)
                camera.yaw += labTurn() * frameSeconds;
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

            // The lab: the player in the middle of what its panel leaves of the screen (the pivot that far to the
            // player's left), and the camera free to go further out
            const Tuning::Camera kept = tuning.camera;
            if (labOpen)
            {
                const float shift = lab->coverRight(ui) / std::max(1.0f, screenW);
                tuning.camera.shoulder = -camera.currentDistance * std::tan(glm::radians(fov()) * 0.5f) * aspect * shift;
                tuning.camera.maxDistance = 20.0f;
            }
            camera.update(frameSeconds, head, inside, world.grid, content.blockTable, tuning, simTime);
            tuning.camera = kept;
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

        // Climbing out of a hole: the hands up the wall in turn, the legs pushing
        if (f.climbTo >= 0.0f)
        {
            const float t = simTime * 7.0f + static_cast<float>(f.id);
            if (arm)
                return forward(150.0f + 22.0f * std::sin(t + (left ? 0.0f : 3.14159f)));
            return forward(30.0f * std::sin(t + (left ? 3.14159f : 0.0f)));
        }

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

            mesh->textures = blockTextures;
            mesh->setData(buildMesh(piece.cells, content.blockTable, blockMeshOptions()));
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
        // Playing with a controller: its button
        if (padActive)
            if (auto p = padControls.find(action); p != padControls.end() and not p->second.empty())
                return padLabel(p->second.front());

        auto it = controls.find(action);

        if (it == controls.end() or it->second.empty())
            return "?";

        const Binding& b = it->second.front();
        return keyLabel(b.scancode, b.mouseButton);
    }

    std::string ArenaGame::keyLabel(int scancode, int mouseButton) const
    {
        if (scancode < 0)
        {
            switch (mouseButton)
            {
                case 1: return "LMB";
                case 2: return "MMB";
                case 3: return "RMB";
                default: return "M" + std::to_string(mouseButton);
            }
        }

        // The letter the keyboard shows (Z on an AZERTY's W key); the bindings are by position (scancodes)
        std::string name = SDL_GetKeyName(SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(scancode)));
        if (name.empty())
            name = SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode));

        static const std::map<std::string, std::string> shorter = {
            {"Left Shift", "LShift"}, {"Right Shift", "RShift"}, {"Left Ctrl", "LCtrl"}, {"Right Ctrl", "RCtrl"},
            {"Left Alt", "LAlt"}, {"Right Alt", "RAlt"}, {"Return", "Enter"}, {"Backspace", "Bksp"},
        };
        if (auto it = shorter.find(name); it != shorter.end())
            name = it->second;
        return name;
    }

    const std::vector<std::pair<std::string, std::string>>& ArenaGame::controlRows()
    {
        // The rebindable ones, as the controls screen lists them (the left column first)
        static const std::vector<std::pair<std::string, std::string>> rows = {
            {"forward", "Move forward"}, {"back", "Move back"}, {"left", "Move left"}, {"right", "Move right"},
            {"jump", "Jump"}, {"dodge", "Dodge / sprint"}, {"light", "Light attack"}, {"heavy", "Heavy attack"},
            {"nextWeapon", "Next weapon"},
            {"ability5", "Spell 1"}, {"ability6", "Spell 2"}, {"ability7", "Spell 3"},
            {"ability4", "Weapon skill 1"}, {"ability8", "Weapon skill 2"},
            {"ability1", "Raise wall"}, {"ability2", "Dig pit"}, {"ability3", "Mine"},
        };
        return rows;
    }

    void ArenaGame::bindKey(const std::string& action, const std::string& key)
    {
        auto nameOf = [](const Binding& b) {
            return b.scancode >= 0 ? std::string(SDL_GetScancodeName(static_cast<SDL_Scancode>(b.scancode))) : "Mouse" + std::to_string(b.mouseButton);
        };

        const std::string old = controls.count(action) and not controls[action].empty() ? nameOf(controls[action].front()) : "";

        // Another action of the list on that key gets this one's old key: they swap
        for (const auto& [other, label] : controlRows())
        {
            if (other == action or not controls.count(other) or controls[other].empty() or nameOf(controls[other].front()) != key)
                continue;

            if (not old.empty())
            {
                prefs.keys[other] = old;
                if (other == "dodge")
                    prefs.keys["sprint"] = old;
            }
            controlsMessage = label + " moved to " + old;
        }

        prefs.keys[action] = key;
        if (action == "dodge")
            prefs.keys["sprint"] = key;   // the roll and the sprint share their key

        saveOptions(optionsPath(), prefs);
        readControls();
    }

    void ArenaGame::drawControls()
    {
        auto up = [](std::string t) {
            for (auto& c : t)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return t;
        };

        ControlsView v;
        v.split = 9;
        v.message = controlsMessage;

        for (const auto& [action, label] : controlRows())
        {
            ControlsView::Row row;
            row.label = label;
            row.waiting = rebinding == action;

            if (auto it = controls.find(action); it != controls.end() and not it->second.empty())
            {
                row.key = up(keyLabel(it->second.front().scancode, it->second.front().mouseButton));
                if (it->second.size() > 1)
                    row.also = up(keyLabel(it->second[1].scancode, it->second[1].mouseButton));
            }
            else
                row.key = "-";

            v.rows.push_back(row);
        }

        screens::drawControls(ui, v);
    }

    // --------------------------------------------------------------------------------------------
    // The UI: the HUD and the screens, drawn anew each frame
    // --------------------------------------------------------------------------------------------

    namespace
    {
        std::string upper(std::string s)
        {
            for (auto& c : s)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return s;
        }

        std::string localId(const std::string& id)
        {
            const size_t colon = id.find(':');
            return colon == std::string::npos ? id : id.substr(colon + 1);
        }
    }

    bool ArenaGame::alone() const
    {
        return not networked() or (server and server->peerCount() == 0);
    }

    void ArenaGame::rename(const std::string& name)
    {
        if (name.empty() or name == prefs.name)
            return;

        prefs.name = name;
        saveOptions(optionsPath(), prefs);
        options.playerName = name;

        if (server)
            server->hostName = name;
        if (client)
            client->setName(name);
    }

    void ArenaGame::toTitleClicked()
    {
        // In a run: a second click to be sure (the run is lost; hosting, it ends for everyone)
        if (world.inRun() and not client and not titleArmed())
        {
            titleArmedUntil = uiTime + 3.0f;
            return;
        }

        setOptionsOpen(false);

        // A player who joined leaves: back to its own title
        if (client)
        {
            leaveGame();
            return;
        }

        if (world.inRun())
            world.stopRun();

        titleOpen = true;
        titleArmedUntil = -1.0f;
    }

    void ArenaGame::quitClicked()
    {
        if (not quitArmed())
        {
            quitArmedUntil = uiTime + 3.0f;
            return;
        }

        // A client says goodbye first; the router's ports close as the game goes (PortMapper)
        if (client)
            leaveGame();

        if (window)
            window->receivedQuitRequest();
    }

    bool ArenaGame::uiTakesClicks() const
    {
        return optionsOpen or bookOpen or shopOpen or titleOpen or endOpen or armoryOpen or difficultyOpen or labOpen;
    }

    std::string ArenaGame::playerNameOf(int id) const
    {
        if (client)
            return client->nameOf(id);

        if (server)
            for (const auto& pl : server->lobby().players)
                if (pl.playerId == id)
                    return pl.name;

        return "";
    }

    std::string ArenaGame::statusName(const std::string& id) const
    {
        std::string name = localId(id);
        std::replace(name.begin(), name.end(), '_', ' ');   // flame_weapon: "Flame weapon"
        if (not name.empty())
            name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        return name;
    }

    std::vector<Hud::Status> ArenaGame::statusesOf(const Fighter& f) const
    {
        std::vector<Hud::Status> out;
        for (const auto& st : f.statuses)
        {
            // A weapon imbued with a status shows in that status's colour
            auto def = content.statuses.find(st.id);
            const std::string& look = def != content.statuses.end() and not def->second.imbue.empty() ? def->second.imbue : st.id;
            out.push_back({localId(look), statusName(st.id), st.stacks});
        }
        return out;
    }

    void ArenaGame::playerHud(Hud::View& v, const Fighter& p)
    {
        v.player = true;
        v.alive = p.alive();
        v.crosshair = p.alive();
        v.health = p.health;
        v.maxHealth = p.maxHealth;
        v.stamina = p.stamina / std::max(1.0f, tuning.stamina.max);
        v.exhausted = p.exhausted;
        v.mana = p.mana / std::max(1.0f, tuning.mana.max);
        v.manaPoints = p.mana;
        v.dashes = world.infiniteStamina ? static_cast<float>(world.maxDashes(p)) : p.dashCharges;
        v.maxDashes = world.maxDashes(p);
        v.nextWeaponKey = upper(keyName("nextWeapon"));
        v.statuses = statusesOf(p);

        // A charge held (the bow's draw, a charged skill): how far it is
        if (p.action == Action::Attack)
            if (const AttackDef* a = world.currentAttack(p); a and a->charge.at >= 0 and a->charge.frames > 0 and p.chargeSlot != -1 and p.frame <= a->charge.at + 1)
                v.draw = p.chargeRatio(a);

        // The mana dims when not even the cheapest spell is affordable
        float cheapest = 1e9f;
        for (int slot = 0; slot < ArenaWorld::SPELL_SLOTS; ++slot)
            if (const auto sp = world.spellOf(world.spellRecipe(p, slot)); sp->valid())
                cheapest = std::min(cheapest, sp->mana);
        v.manaLow = p.mana < cheapest and cheapest < 1e8f and not world.infiniteMana;

        // The weapons owned (in a run: bought)
        for (const auto& id : content.weaponOrder)
            if (world.owns(p, id))
                v.weapons.emplace_back(content.weapons.at(id).name, id == p.weapon);

        // The ability keys: spells, then terrain and weapon skills
        const float cooldownScale = std::max(0.3f, 1.0f - p.boon.cooldown);

        for (int i = 0; i < InputFrame::ABILITIES; ++i)
        {
            const std::string key = upper(keyName("ability" + std::to_string(i + 1)));

            if (const int slot = world.spellSlotAt(i); slot >= 0)
            {
                // In a run: the spells the level ups gave (a key with none yet is left out)
                const bool given = world.inRun();
                if (given ? slot >= static_cast<int>(p.runSpells.size()) : slot >= static_cast<int>(book.equipped.size()))
                    continue;

                const SpellRecipe r = world.spellRecipe(p, slot);
                const auto sp = world.spellOf(r);

                Hud::SpellKey k;
                k.key = key;
                k.name = given ? runSpellName(content, p.runSpells[static_cast<size_t>(slot)]) : spellName(content, book, r);
                k.element = r.element;
                k.mana = static_cast<int>(std::lround(sp->mana * std::max(0.3f, 1.0f - p.boon.manaCost)));

                const bool locked = not sp->valid();

                const float left = p.abilityCooldown[i];
                if (locked)
                    k.state = Hud::SpellKey::State::Locked;
                else if (left > 0.0f)
                {
                    k.state = Hud::SpellKey::State::Cooldown;
                    k.seconds = left;
                    k.progress = 1.0f - left / std::max(0.01f, sp->cooldown * cooldownScale);
                }
                else if (p.mana < sp->mana and not world.infiniteMana)
                    k.state = Hud::SpellKey::State::NoMana;

                v.spells.push_back(k);
                continue;
            }

            const AbilityDef* a = world.ability(i);
            if (not a)
                continue;

            Hud::Skill sk;
            sk.key = key;
            sk.name = a->name;
            sk.cost = static_cast<int>(std::lround(a->staminaCost));
            // Held to charge: only while its attack is winding up and waiting on the key (a charge skill's)
            sk.charging = p.chargeSlot == i and p.action == Action::Attack and p.attack == "@" + a->id and a->attack.charge.at >= 0 and
                          p.frame <= a->attack.charge.at + 1;

            const float left = p.abilityCooldown[i];
            if (left > 0.0f)
            {
                sk.seconds = left;
                sk.progress = 1.0f - left / std::max(0.01f, a->cooldown * cooldownScale);
            }

            sk.noStamina = a->staminaCost > 0.0f and p.stamina < tuning.stamina.minToAct and not world.infiniteStamina;
            v.skills.push_back(sk);
        }
    }

    void ArenaGame::enemyHud(Hud::View& v)
    {
        // Bars over the enemies, projected by hand: the engine camera matrices belong to the render thread
        const glm::mat4 proj = glm::perspective(fov() * DEG, aspect, 0.1f, 500.0f);
        const glm::mat4 view = glm::lookAt(camera.position, camera.position + camera.forward(), glm::vec3(0, 1, 0));

        // In the final duel, the other players get bars (and names) too; in any game with others, the
        // players get their names over them (allies with their health, yours in third person)
        const Fighter* me = world.player();
        const bool duel = world.run.phase == RunPhase::Duel;

        for (const auto& f : world.fighters)
        {
            const bool mine = me and f.id == me->id;
            const bool rival = duel and f.team == Team::Player and not mine;
            const bool ally = networked() and not duel and f.team == Team::Player and not mine;
            const bool self = networked() and mine and not firstPerson;
            if ((f.team != Team::Enemy and not rival and not ally and not self) or not f.alive() or v.enemies.size() >= 32)
                continue;

            const auto prev = previous.find(f.id);
            const glm::vec3 pos = prev != previous.end() ? glm::mix(prev->second.position, f.body.position, alpha) : f.body.position;
            const glm::vec4 clip = proj * view * glm::vec4(pos + glm::vec3(0, f.body.height + 0.15f, 0), 1.0f);

            if (clip.w <= 0.1f or glm::length(pos - camera.position) > 22.0f)
                continue;

            const glm::vec3 ndc = glm::vec3(clip) / clip.w;

            if (std::fabs(ndc.x) > 1.1f or std::fabs(ndc.y) > 1.1f)
                continue;

            Hud::EnemyBar bar;
            bar.head = glm::vec2((ndc.x * 0.5f + 0.5f) * screenW, (0.5f - ndc.y * 0.5f) * screenH);
            bar.health = f.health / std::max(1.0f, f.maxHealth);
            bar.poise = f.poise / std::max(1.0f, f.maxPoise);
            bar.staggered = f.action == Action::Stagger;
            bar.elite = f.elite;
            bar.boss = f.team == Team::Enemy and f.id == world.run.bossId;
            for (const auto& id : f.affixes)
                if (auto a = content.affixes.find(id); a != content.affixes.end())
                    bar.affixes.push_back({a->second.name, ui::Colour(a->second.colour.r * 255.0f, a->second.colour.g * 255.0f, a->second.colour.b * 255.0f, 255.0f)});
            bar.statuses = statusesOf(f);
            if (rival or ally or self)
                bar.name = playerNameOf(f.id);
            if (self and bar.name.empty())
                bar.name = options.playerName;
            bar.ally = ally;
            bar.self = self;

            // A dummy's damage meter, for a few seconds after the last hit: what it took, and a second over the hits
            if (auto m = meters.find(f.id); m != meters.end() and m->second.total >= 0.5f and simTime - m->second.last < 4.0f)
            {
                const float over = m->second.last - m->second.first;
                bar.meter = std::to_string(std::lround(m->second.total)) + " damage";
                if (over >= 0.5f)
                    bar.meter += ", " + std::to_string(std::lround(m->second.total / over)) + " a second";
            }
            v.enemies.push_back(bar);
        }

        // The player's damage numbers, rising and fading
        for (const auto& n : damageNumbers)
        {
            const glm::vec3 at = n.at + glm::vec3(0, 0.3f + 1.4f * n.age - 0.6f * n.age * n.age, 0);
            const glm::vec4 clip = proj * view * glm::vec4(at, 1.0f);
            if (clip.w <= 0.1f or glm::length(at - camera.position) > 30.0f)
                continue;
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (std::fabs(ndc.x) > 1.05f or std::fabs(ndc.y) > 1.05f)
                continue;

            Hud::Number shown;
            shown.at = glm::vec2((ndc.x * 0.5f + 0.5f) * screenW, (0.5f - ndc.y * 0.5f) * screenH);
            shown.text = std::to_string(std::max(1L, std::lround(n.value)));
            shown.big = n.crit or n.kill;
            shown.crit = n.crit;
            shown.alpha = std::clamp((0.9f - n.age) / 0.3f, 0.0f, 1.0f);
            shown.pop = std::clamp(1.0f - n.age / 0.12f, 0.0f, 1.0f);
            v.numbers.push_back(shown);
        }

        edgeArrows(v, proj, view);
    }

    void ArenaGame::handleUiClicks()
    {
        std::vector<glm::vec2> clicks;
        {
            std::lock_guard<std::mutex> lock(inputMutex);
            clicks.swap(uiClicks);
        }

        for (const auto& c : clicks)
        {
            const int action = ui.hitAt(c.x, c.y);

            if (action == 0)
                continue;

            // A screen's blank space (its blocker): nothing, but a name being typed in the options is kept
            if (action < 0)
            {
                if (optionsOpen and optionsScreen.naming)
                    rename(optionsScreen.commitName());
                continue;
            }

            if (lab and Lab::owns(action))
            {
                bool replay = false;
                std::string command;
                lab->click(action, replay, command);
                if (not command.empty())
                    print(runCommand(command));
                if (replay)
                    labShow();
                continue;
            }

            if (controlsOpen)
            {
                if (action >= screens::CONTROLS_ROW and action < screens::CONTROLS_ROW + static_cast<int>(controlRows().size()))
                {
                    rebinding = controlRows()[static_cast<size_t>(action - screens::CONTROLS_ROW)].first;
                    controlsMessage.clear();
                    capturing = true;
                }
                else if (action == screens::CONTROLS_RESET)
                {
                    prefs.keys.clear();
                    saveOptions(optionsPath(), prefs);
                    readControls();
                    controlsMessage = "Back to the default keys";
                }
                else if (action == screens::CONTROLS_BACK)
                {
                    controlsOpen = false;
                    rebinding.clear();
                    capturing = false;
                }
                continue;
            }

            if (optionsOpen)
            {
                // A click away from the name field keeps what was typed
                if (optionsScreen.naming and action != OptionsScreen::NAME)
                    rename(optionsScreen.commitName());

                if (action == OptionsScreen::NAME)
                {
                    if (not optionsScreen.naming)
                        optionsScreen.startNaming(prefs.name);
                    continue;
                }

                if (action == OptionsScreen::QUIT)
                {
                    quitClicked();
                    continue;
                }

                if (action == OptionsScreen::TO_TITLE)
                {
                    toTitleClicked();
                    continue;
                }

                if (action == OptionsScreen::CONTROLS)
                {
                    controlsOpen = true;
                    controlsMessage.clear();
                    continue;
                }

                bool closed = false;
                if (optionsScreen.act(action, prefs, closed))
                {
                    applyOptions();
                    saveOptions(optionsPath(), prefs);
                }
                if (closed)
                    setOptionsOpen(false);
                continue;
            }

            if (bookOpen)
            {
                bool closed = false;
                bookChanged(spellbook.act(action, content, book, closed));
                if (closed)
                    setBookOpen(false);
                continue;
            }

            if (shopOpen)
            {
                pendingPick = action;
                continue;
            }

            if (difficultyOpen)
            {
                if (action == screens::DIFFICULTY_BACK)
                    pendingPick = InputFrame::BACK;
                else if (action >= screens::DIFFICULTY_PICK)
                    pendingPick = InputFrame::DIFFICULTY + (action - screens::DIFFICULTY_PICK);
                continue;
            }

            if (armoryOpen)
            {
                if (action == screens::ARMORY_BACK)
                    armoryBack = true;
                else if (action >= screens::ARMORY_SPELL and action < screens::ARMORY_BACK)
                    pendingPick = InputFrame::FIRST_SPELL + (action - screens::ARMORY_SPELL);
                else if (action >= screens::ARMORY_PICK)
                {
                    pendingPick = InputFrame::WEAPON + (action - screens::ARMORY_PICK);
                    armoryBack = false;
                }
                continue;
            }

            if (titleOpen and joinOpen)
            {
                joinAction(action);
                continue;
            }

            if (titleOpen and inviteOpen)
            {
                if (action == screens::INVITE_JOIN)
                {
                    inviteOpen = false;
                    openJoin();
                }
                else
                    inviteAction(action);
                continue;
            }

            if (titleOpen)
            {
                switch (action)
                {
                    case screens::TITLE_INVITE:
                    case screens::TITLE_MULTIPLAYER:
                        inviteOpen = true;
                        inviteMessage.clear();
                        break;
                    case screens::TITLE_JOIN:
                        if (not networked())
                            openJoin();
                        break;
                    case screens::LOBBY_LEAVE:
                        leaveGame();
                        break;
                    case screens::TITLE_QUIT:
                        quitClicked();
                        break;
                    case screens::TITLE_ARENA:
                        if (not client)
                            startArena();
                        break;
                    case screens::LOBBY_READY:
                        if (client)
                        {
                            bool mine = client->ready;
                            for (const auto& pl : client->lobby.players)
                                if (pl.playerId == client->playerId)
                                    mine = pl.ready;
                            client->setReady(not mine);
                        }
                        break;
                    case screens::TITLE_TRAINING: titleOpen = false; break;
                    case screens::TITLE_SPELLBOOK: setBookOpen(true); break;
                    case screens::TITLE_OPTIONS: setOptionsOpen(true); break;
                    default: break;
                }
                continue;
            }

            if (endOpen)
            {
                if (action == screens::END_AGAIN)
                    pendingPick = InputFrame::START;
                else if (action == screens::END_TRAINING and not networked())
                {
                    world.stopRun();
                    print(runCommand("reset"));
                }
            }
        }
    }

    bool ArenaGame::hostNow()
    {
        if (networked())
            return true;

        // The port the player chose, or the game's own and a look for one on the router
        const bool chosen = not hostPortEdit.empty();
        const uint16_t port = chosen ? net::PortMapper::parseHostPort(hostPortEdit) : autoPort;

        if (port == 0)
        {
            inviteMessage = "A port is a number from 1 to 65534";
            return false;
        }

        options.port = port;

        // A server owned by the game: the engine cannot add a system once running
        NetworkConfig net;
        net.isServer = true;
        net.tcpPort = options.port;
        net.udpLocalPort = static_cast<uint16_t>(options.port + 1);
        net.udpPeerPort = static_cast<uint16_t>(options.port + 1);
        net.emitEvents = false;
        net.autoReconnect = false;

        auto* backend = new SdlNetworkBackend(net);
        auto network = std::make_unique<NetworkSystem>(backend, net);
        network->init();

        if (not backend->isReady())
        {
            inviteMessage = "Could not open port " + std::to_string(options.port) + " (is something else on this machine using it?)";
            return false;
        }

        manualPort = chosen;
        prefs.hostPort = chosen ? port : 0;
        saveOptions(optionsPath(), prefs);

        ownBackend = backend;
        ownNetwork = std::move(network);
        options.network = ownNetwork.get();
        options.mode = ArenaOptions::Mode::Host;

        transport = std::make_unique<net::EngineTransport>(options.network);
        server = std::make_unique<net::NetServer>(world, content, *transport, false);
        server->hostName = options.playerName;
        server->offerMods();

        // A chosen port is the only one asked for: it may be forwarded by hand, and then the router refuses
        lanAddress = net::PortMapper::localAddress();
        if (options.upnp)
            portMapper.start(options.port, static_cast<uint16_t>(options.port + 1), "Voxel Arena",
                             chosen ? std::vector<uint16_t>{port} : net::PortMapper::parsePorts(options.upnpPorts));

        inviteMessage.clear();
        print("hosting on port " + std::to_string(options.port) + " (TCP) and " + std::to_string(options.port + 1) + " (UDP)");
        return true;
    }

    void ArenaGame::openJoin()
    {
        joinOpen = true;
        joinFocus = 0;
        joinMessage.clear();
        joinAddress = prefs.lastJoin;
        joinName = options.playerName == "player" ? prefs.name : options.playerName;
    }

    void ArenaGame::joinAction(int action)
    {
        switch (action)
        {
            case screens::JOIN_ADDRESS: joinFocus = 0; break;
            case screens::JOIN_NAME: joinFocus = 1; break;
            case screens::JOIN_GO: joinNow(); break;
            case screens::JOIN_CLOSE: joinOpen = false; break;
            case screens::JOIN_PASTE:
            {
                // What their invite copied: an address, or the whole command
                char* clip = SDL_GetClipboardText();
                std::string pasted = clip ? clip : "";
                SDL_free(clip);

                if (const size_t at = pasted.find("--join"); at != std::string::npos)
                    pasted = pasted.substr(at + 6);
                pasted.erase(std::remove_if(pasted.begin(), pasted.end(), [](char c) { return c == ' ' or c == '\n' or c == '\r' or c == '\t'; }), pasted.end());

                if (pasted.empty())
                    joinMessage = "Nothing to paste: copy their address first";
                else
                {
                    joinAddress = pasted.substr(0, 64);
                    joinFocus = 0;
                    joinMessage.clear();
                }
                break;
            }
            default: break;
        }
    }

    void ArenaGame::resetLocalState()
    {
        // What this game kept of the world it was showing: gone with it
        previous.clear();
        gaits.clear();
        spawnMarks.clear();
        spawnPings.clear();
        statusTimers.clear();
        burningLooks.clear();
        pendingPick = 0;
        pendingEquip = 0;
        clientPhase = RunPhase::Off;
        duelSeen = RunPhase::Off;
        lastPhase = RunPhase::Off;
        headlineWave = 0;
        hurtFlash = 0.0f;
        meshDirty = true;
        accumulator = 0.0;
    }

    bool ArenaGame::joinNow()
    {
        if (networked())
            return false;

        // "address" or "address:port"
        std::string address = joinAddress;
        uint16_t port = 7777;

        if (const size_t colon = address.rfind(':'); colon != std::string::npos)
        {
            const std::string digits = address.substr(colon + 1);
            if (digits.empty() or digits.size() > 5 or not std::all_of(digits.begin(), digits.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
            {
                joinMessage = "The port after the : is a number (7777)";
                return false;
            }
            port = static_cast<uint16_t>(std::stoi(digits));
            address = address.substr(0, colon);
        }

        if (address.empty())
        {
            joinMessage = "Type the host's address first";
            return false;
        }

        if (not joinName.empty())
            options.playerName = joinName;

        NetworkConfig net;
        net.isServer = false;
        net.peerAddress = address;
        net.tcpPort = port;
        net.udpLocalPort = 0;
        net.udpPeerPort = static_cast<uint16_t>(port + 1);
        net.emitEvents = false;
        net.autoReconnect = false;
        net.udpSilenceMs = 1000;     // snapshots come many times a second: silence means UDP is not getting through

        auto* backend = new SdlNetworkBackend(net);
        auto network = std::make_unique<NetworkSystem>(backend, net);
        network->init();

        if (not backend->isReady())
        {
            joinMessage = "The network could not start on this machine";
            return false;
        }

        // Kept for next time
        prefs.lastJoin = joinAddress;
        prefs.name = joinName;
        saveOptions(optionsPath(), prefs);

        // This game becomes a client of theirs: its world is the server's from now on
        if (world.inRun())
            world.stopRun();

        ownBackend = backend;
        ownNetwork = std::move(network);
        options.network = ownNetwork.get();
        options.mode = ArenaOptions::Mode::Client;
        options.address = address;
        options.port = port;

        resetLocalState();
        transport = std::make_unique<net::EngineTransport>(options.network);
        client = std::make_unique<net::NetClient>(world, content, tuning, *transport, options.playerName);
        applySpells();

        joinOpen = false;
        inviteOpen = false;
        titleOpen = true;   // the lobby, connecting
        print("joining " + address + ":" + std::to_string(port));
        return true;
    }

    void ArenaGame::pollClient()
    {
        client->poll(netNowMs());

        if (client->state == net::NetClient::State::ModsArrived)
            applyServerMods();
    }

    void ArenaGame::pollServer()
    {
        server->poll();

        for (const auto& line : server->log)
        {
            print(line);
            netLog.push_back(line);
        }
        server->log.clear();

        while (netLog.size() > 2)
            netLog.pop_front();
    }

    void ArenaGame::applyServerMods()
    {
        const net::ModBundle bundle = std::move(client->mods);
        const std::string dir = (std::filesystem::temp_directory_path() / "voxelarena-servermods").string();

        std::string error;

        if (not net::writeModFiles(bundle.files, dir, error))
        {
            client->refuse("the server's mods were refused: " + error);
            return;
        }

        print("loading the server's mods (" + std::to_string(bundle.files.size()) + " files)");

        ownMods = options.mods;
        options.mods = ModSources{{dir}, ""};   // exactly the server's, none turned off
        serverMods = true;
        world.setBrain(nullptr);    // the scripts of this game's own mods no longer fit its content

        loadAll(false);

        if (content.fingerprint != bundle.fingerprint or not content.report.errors.empty())
        {
            restoreOwnMods();
            world.setBrain(&brain);
            client->refuse("the server's mods do not load here (is this game up to date?)");
            return;
        }

        // A client takes its names and tuning from the content when it is made: a new one, same connection
        resetLocalState();
        client = std::make_unique<net::NetClient>(world, content, tuning, *transport, options.playerName);
        client->rejoin();
        applySpells();
    }

    void ArenaGame::restoreOwnMods()
    {
        if (not serverMods)
            return;

        serverMods = false;
        options.mods = ownMods;
        loadAll(false);
    }

    void ArenaGame::leaveGame()
    {
        if (not networked())
            return;

        if (client)
        {
            client->leave();
            if (ownNetwork)
                ownNetwork->execute();   // the goodbye goes out before the network does
        }

        client.reset();
        server.reset();
        transport.reset();
        portMapper.stop();
        manualPort = false;
        ownNetwork.reset();
        ownBackend = nullptr;
        options.network = nullptr;
        options.mode = ArenaOptions::Mode::Single;
        netLog.clear();
        restoreOwnMods();

        // A game alone again, in the training ground, from its title
        resetLocalState();
        world.start(content, tuning);
        world.authority = true;
        world.setBrain(&brain);
        applySpells();

        if (const Fighter* p = world.player())
            camera.yaw = p->yaw;

        titleOpen = true;
        print("playing alone");
    }

    void ArenaGame::inviteAction(int action)
    {
        auto copy = [&](const std::string& text) {
            SDL_SetClipboardText(text.c_str());
            inviteMessage = "Copied: " + text;
        };

        const std::string port = ":" + std::to_string(options.port);
        const auto st = portMapper.status();
        const std::string lan = st.lanIp.empty() ? lanAddress : st.lanIp;

        // Online, the port the router opened (it may not be the game's: a Freebox opens above 45000)
        const bool online = (st.state == net::PortMapper::State::Open or manualPort) and not st.externalIp.empty();
        const std::string outside = st.externalIp + ":" + std::to_string(st.externalPort ? st.externalPort : options.port);

        switch (action)
        {
            case screens::INVITE_HOST: hostNow(); break;
            case screens::INVITE_COPY_ONLINE: if (online) copy(outside); break;
            case screens::INVITE_COPY_LAN: copy((client ? options.address : lan) + port); break;
            case screens::INVITE_COPY_JOIN:
            {
                copy("VoxelArena --join " + (client ? options.address + port : (online ? outside : lan + port)));
                break;
            }
            case screens::INVITE_CLOSE: inviteOpen = false; break;
            default: break;
        }
    }

    void ArenaGame::drawInvite()
    {
        InviteView v;
        v.hosting = server != nullptr;
        v.joined = client != nullptr;
        v.port = options.port;
        v.portEdit = hostPortEdit;
        v.manualPort = manualPort;
        v.time = uiTime;
        v.message = inviteMessage;
        v.players = static_cast<int>(std::count_if(world.fighters.begin(), world.fighters.end(), [](const Fighter& f) { return f.team == Team::Player; }));

        if (client)
        {
            v.lanAddress = options.address;
        }
        else
        {
            const auto st = portMapper.status();
            v.lanAddress = st.lanIp.empty() ? lanAddress : st.lanIp;
            v.externalAddress = st.externalIp;
            v.externalPort = st.externalPort;
            v.routerError = st.error;
            switch (st.state)
            {
                case net::PortMapper::State::Off: v.router = LobbyView::Router::Off; break;
                case net::PortMapper::State::Searching: v.router = LobbyView::Router::Searching; break;
                case net::PortMapper::State::Open: v.router = LobbyView::Router::Open; break;
                case net::PortMapper::State::NoRouter: case net::PortMapper::State::Failed: v.router = LobbyView::Router::Closed; break;
            }
        }

        screens::drawInvite(ui, v);
    }

    void ArenaGame::drawArmory()
    {
        const Fighter* me = world.player();
        ArmoryView v;
        v.picked = me and me->ready;
        v.players = 0;

        for (const auto& f : world.fighters)
            if (f.team == Team::Player)
            {
                ++v.players;
                v.ready += f.ready ? 1 : 0;
            }

        for (const auto& id : content.weaponOrder)
        {
            const WeaponDef& w = content.weapons.at(id);
            ArmoryView::Weapon card;
            card.name = w.name;
            card.role = w.role;
            card.text = w.text;
            card.regen = w.regen;
            card.light = w.lightText;
            card.heavy = w.heavyText;
            card.icon = offerIcon(content, id);
            card.chosen = me and me->armed and me->weapon == id;

            // Its skills: the weapon skills made for its type
            for (const auto& [aid, a] : content.abilities)
                if (std::find(a.weapons.begin(), a.weapons.end(), w.type) != a.weapons.end())
                    card.skills.push_back(a.name);

            v.weapons.push_back(card);
        }

        // Then the first spell
        v.choosingSpell = armoryChoosingSpell();
        if (me and content.weapons.count(me->weapon))
            v.weapon = content.weapons.at(me->weapon).name;

        for (const auto& id : me and not me->firstSpellOffer.empty() ? me->firstSpellOffer : content.run.startChoices)
        {
            const SpellDef& sp = content.spells.at(id);
            ArmoryView::Spell card;
            card.name = sp.name;
            card.element = sp.recipe.element;
            card.text = (sp.text.empty() ? "" : sp.text + "\n") + describe(content, compileSpell(content, sp.recipe));
            card.chosen = me and not me->runSpells.empty() and me->runSpells.front().id == id;
            v.spells.push_back(card);
        }

        // The hovered card's attacks: after a moment, fading in (as the shop's tooltips)
        const auto now = std::chrono::steady_clock::now();
        const float hoveredFor = std::chrono::duration<float>(now - armoryHoverSince).count();
        v.tipCard = armoryHover;
        v.tipAlpha = armoryHover >= 0 ? std::clamp((hoveredFor - 0.35f) / 0.12f, 0.0f, 1.0f) : 0.0f;

        const int hovered = screens::drawArmory(ui, v);
        if (hovered != armoryHover)
        {
            armoryHover = hovered;
            armoryHoverSince = now;
        }
    }

    bool ArenaGame::armoryChoosingSpell() const
    {
        const Fighter* me = world.player();
        return me and me->armed and not content.run.startChoices.empty() and not armoryBack;
    }

    void ArenaGame::drawLobby()
    {
        LobbyView v;
        v.isHost = server != nullptr;
        // Leave: a client always; a host when it started hosting from the title (the engine's own network cannot be closed)
        v.canLeave = client != nullptr or (server != nullptr and ownNetwork != nullptr);
        v.port = options.port;
        v.manualPort = manualPort;
        v.time = uiTime;
        v.log.assign(netLog.begin(), netLog.end());
        v.quitArmed = quitArmed();

        net::LobbyMsg lobby;

        if (server)
        {
            lobby = server->lobby();
            v.lanAddress = lanAddress;

            const auto st = portMapper.status();
            v.externalAddress = st.externalIp;
            v.externalPort = st.externalPort;
            v.routerError = st.error;
            if (st.lanIp.size())
                v.lanAddress = st.lanIp;

            switch (st.state)
            {
                case net::PortMapper::State::Off: v.router = LobbyView::Router::Off; break;
                case net::PortMapper::State::Searching: v.router = LobbyView::Router::Searching; break;
                case net::PortMapper::State::Open: v.router = LobbyView::Router::Open; break;
                case net::PortMapper::State::NoRouter: case net::PortMapper::State::Failed: v.router = LobbyView::Router::Closed; break;
            }
        }
        else if (client)
        {
            lobby = client->lobby;
            v.joined = options.address;
            v.dedicated = lobby.dedicated;

            switch (client->state)
            {
                case net::NetClient::State::Connecting: v.status = "Connecting to " + options.address + "\xE2\x80\xA6"; break;
                case net::NetClient::State::Joining: v.status = "Joining\xE2\x80\xA6"; break;
                case net::NetClient::State::ModsArrived: v.status = "Loading the server's mods\xE2\x80\xA6"; break;
                case net::NetClient::State::Refused: v.status = "Refused: " + client->refusal; v.statusBad = true; break;
                case net::NetClient::State::Lost: v.status = "Connection lost"; v.statusBad = true; break;
                case net::NetClient::State::Playing: break;
            }
        }

        const int me = client ? client->playerId : (world.player() ? world.player()->id : 0);
        for (const auto& pl : lobby.players)
        {
            v.players.push_back({pl.name, pl.host, pl.playerId == me, pl.ready, pl.pingMs});
            if (pl.playerId == me)
                v.youReady = pl.ready;
        }

        screens::drawLobby(ui, v);
    }

    void ArenaGame::drawUi(float frameSeconds)
    {
        if (ui.width() != screenW or ui.height() != screenH)
            ui.resize(screenW, screenH);

        {
            std::lock_guard<std::mutex> lock(inputMutex);
            ui.mouse(mouseX, mouseY);
        }

        uiTime += frameSeconds;
        hurtFlash = std::max(0.0f, hurtFlash - frameSeconds * 3.0f);   // fades in about a third of a second

        Hud::View v;
        v.time = uiTime;
        v.hurt = hurtFlash * prefs.hitFlash;

        const Fighter* p = world.player();
        const RunPhase phase = world.run.phase;

        // A client: the host started the run (the lobby goes), or went back from its end (it comes back)
        if (client)
        {
            if (world.inRun() and clientPhase == RunPhase::Off)
                titleOpen = false;
            if (phase == RunPhase::Off and clientPhase != RunPhase::Off)
                titleOpen = true;
            clientPhase = phase;
        }

        // Back from choosing the difficulty: the title (the lobby) again
        if (not client and lastPhase == RunPhase::Difficulty and phase == RunPhase::Off)
            titleOpen = true;

        // The final duel begins: the view turns to the middle, and the others
        if (p and phase == RunPhase::Duel and duelSeen != RunPhase::Duel)
        {
            const glm::vec3 toCentre = content.arenaById(content.runArenaId(world.run.wave, true)).centre - p->body.position;
            if (glm::length(glm::vec2(toCentre.x, toCentre.z)) > 0.5f)
                camera.yaw = glm::degrees(std::atan2(toCentre.x, toCentre.z));
        }
        duelSeen = phase;

        endOpen = p and world.inRun() and (phase == RunPhase::Victory or phase == RunPhase::Defeat) and not titleOpen;

        if (p)
        {
            playerHud(v, *p);
            runHud(v);

            if (not p->alive())
            {
                if (phase == RunPhase::Duel)
                    v.message = "Out of the duel";
                else
                    v.message = world.inRun() ? (phase != RunPhase::Defeat ? "You fell: back up next wave" : "") : "You fell";
            }
            else if (paused)
            {
                v.message = "Paused";
                v.messageDim = true;
            }
        }
        else if (client and not titleOpen)
        {
            switch (client->state)
            {
                case net::NetClient::State::Connecting: v.message = "Connecting..."; break;
                case net::NetClient::State::Joining: v.message = "Joining..."; break;
                case net::NetClient::State::ModsArrived: v.message = "Loading the server's mods..."; break;
                case net::NetClient::State::Refused: v.message = "Refused: " + client->refusal; break;
                case net::NetClient::State::Lost: v.message = "Connection lost"; break;
                case net::NetClient::State::Playing: break;
            }
        }

        enemyHud(v);

        ui.begin();

        // Under an open modal (options, spellbook, invite, join), nothing lights up under the mouse
        const bool modal = optionsOpen or bookOpen or (titleOpen and (inviteOpen or joinOpen));
        if (modal)
            ui.mouse(-1.0f, -1.0f);

        // The lab shows the enemies' bars, the damage numbers and the attacks' warnings, not the player's own HUD
        if (labOpen)
        {
            v.player = false;
            v.hurt = 0.0f;
        }

        if (not titleOpen)
            hud.draw(ui, v, frameSeconds);

        // The screens, the one in front last
        if (titleOpen and networked())
        {
            drawLobby();
        }
        else if (titleOpen)
        {
            TitleView t;
            t.arenaLine = "Timed waves of enemies, a shop between them, bosses";
            t.best = bestScore;
            t.waitingForHost = client != nullptr;
            t.time = uiTime;
            t.quitArmed = quitArmed();
            screens::drawTitle(ui, t);
        }
        // The modal on top gets the mouse back
        auto mouseBack = [&]() {
            std::lock_guard<std::mutex> lock(inputMutex);
            ui.mouse(mouseX, mouseY);
        };

        if (titleOpen and (joinOpen or inviteOpen) and not optionsOpen and not bookOpen)
            mouseBack();

        if (titleOpen and joinOpen)
        {
            JoinView jv;
            jv.address = joinAddress;
            jv.name = joinName;
            jv.message = joinMessage;
            jv.focus = joinFocus;
            jv.time = uiTime;
            screens::drawJoin(ui, jv);
        }
        else if (titleOpen and inviteOpen)
            drawInvite();
        else if (not titleOpen)
            inviteOpen = joinOpen = false;

        if (endOpen and not titleOpen)
        {
            const RunState& r = world.run;
            EndView e;
            e.won = phase == RunPhase::Victory;
            e.wave = r.wave;
            e.waves = r.waves;
            e.score = r.score;
            e.best = bestScore;
            e.kills = r.kills;
            e.level = p->level;
            e.time = r.elapsed;
            e.solo = not networked();
            e.difficulty = content.run.levels() > 1 ? content.run.difficulty(r.difficulty).name : "";
            e.opened = levelJustOpened;
            // The players in the order they joined; one without a name is P and its place
            std::vector<const Fighter*> players;
            for (const auto& f : world.fighters)
                if (f.team == Team::Player)
                    players.push_back(&f);
            std::sort(players.begin(), players.end(), [](const Fighter* a, const Fighter* b) { return a->id < b->id; });

            auto nameOf = [&](int id) {
                std::string name = playerNameOf(id);
                for (size_t k = 0; k < players.size() and name.empty(); ++k)
                    if (players[k]->id == id)
                        name = "P" + std::to_string(k + 1);
                return name.empty() ? "P?" : name;
            };

            if (r.champion != 0)
            {
                e.youChampion = r.champion == p->id;
                e.champion = nameOf(r.champion);
            }
            if (auto b = content.run.bosses.find(r.waves); b != content.run.bosses.end())
                if (auto d = content.enemies.find(b->second); d != content.enemies.end())
                    e.boss = "the " + d->second.name;

            // The player's damage, by what dealt it
            {
                std::vector<std::pair<std::string, float>> bySource(p->stats.bySource.begin(), p->stats.bySource.end());
                std::sort(bySource.begin(), bySource.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
                for (const auto& [id, damage] : bySource)
                    if (damage >= 0.5f)
                        e.sources.push_back(damageSource(id, static_cast<int>(std::lround(damage))));
            }

            // Two players or more: what each one did

            if (players.size() >= 2)
            {
                for (const Fighter* f : players)
                {
                    EndView::Recap row;
                    row.name = nameOf(f->id);
                    row.you = f->id == p->id;
                    row.champion = r.champion != 0 and f->id == r.champion;
                    row.level = f->level;
                    row.kills = f->stats.kills;
                    row.damage = static_cast<int>(std::lround(f->stats.damage));
                    row.healing = static_cast<int>(std::lround(f->stats.healing));
                    row.shards = f->stats.shards;
                    row.xp = f->stats.xp;
                    row.presents = f->stats.presents;
                    row.items = f->stats.items;
                    row.downs = f->stats.downs;
                    e.recap.push_back(row);
                }
            }
            screens::drawEnd(ui, e);
        }

        if (shopOpen and p)
            shopScreen.draw(ui, content, world, *p);

        if (armoryOpen and p)
            drawArmory();

        if (difficultyOpen and p)
            drawDifficulty();

        if (labOpen)
        {
            Lab::View lv;
            lv.loop = lab->loop;
            lv.slow = timeScale < 1.0f;
            lv.boxes = showBoxes;
            lv.reach = showReach;
            lv.firstPerson = firstPerson;
            lv.orbit = labTurn() != 0.0f;
            lv.busy = labPlaying;
            lv.caption = labCaption;
            lab->draw(ui, lv);
        }

        if (bookOpen or optionsOpen)
            mouseBack();

        if (bookOpen)
        {
            // The spell keys' names, in the order of their slots
            std::vector<std::string> keys(book.equipped.size());
            for (int i = 0; i < InputFrame::ABILITIES; ++i)
                if (const int slot = world.spellSlotAt(i); slot >= 0 and slot < static_cast<int>(keys.size()))
                    keys[static_cast<size_t>(slot)] = upper(keyName("ability" + std::to_string(i + 1)));

            spellbook.draw(ui, content, book, alone(), keys, [this](int group, int index) {
                static const char* names[] = {"spellElement", "spellForm", "spellModifier"};
                const std::string action = names[std::clamp(group, 0, 2)] + std::to_string(index + 1);
                return controls.count(action) ? upper(keyName(action)) : std::string();
            });
        }

        if (optionsOpen)
        {
            if (controlsOpen)
                ui.mouse(-1.0f, -1.0f);
            optionsScreen.draw(ui, prefs, alone(), not (titleOpen or endOpen), quitArmed(), titleArmed());

            if (controlsOpen)
            {
                mouseBack();
                drawControls();
            }
        }

        std::string banner;
        if (not content.report.errors.empty())
            banner = std::to_string(content.report.errors.size()) + " content error(s), first: " + content.report.errors[0].str() + "   (console: errors)";

        // The controller's cursor on a screen: an arrow at the point the left stick moved to
        if (padActive and uiTakesClicks() and padCursorX >= 0.0f)
        {
            ui.section("pad-cursor");
            ui.anchor(ui::Anchor::TopLeft);
            const float x = ui.baseX(padCursorX), y = ui.baseY(padCursorY);
            for (int r = 0; r < 9; ++r)
            {
                const auto s = ui.scope(static_cast<uint64_t>(r));
                const float w = static_cast<float>(r < 6 ? r + 1 : 9 - r) * 2.0f;
                ui.rect(x - 1, y + static_cast<float>(r) * 2.0f - 1, w + 2, 4, ui::INK);
            }
            for (int r = 0; r < 9; ++r)
            {
                const auto s = ui.scope(static_cast<uint64_t>(100 + r));
                const float w = static_cast<float>(r < 6 ? r + 1 : 9 - r) * 2.0f;
                ui.rect(x, y + static_cast<float>(r) * 2.0f, w, 2, ui::GOLD);
            }
        }

        std::vector<std::string> lines(consoleLog.begin(), consoleLog.end());
        hud.devLayers(ui, overlayText, consoleOpen, lines, consoleLine, banner);

        ui.end();
    }

    void ArenaGame::remeshIfNeeded()
    {
        if (world.arenaBuilds != meshedArenaBuilds)
        {
            meshedArenaBuilds = world.arenaBuilds;
            meshDirty = true;
        }

        if (not meshDirty and world.grid.revision() == meshedRevision and world.terrain.revision() == meshedTerrainRevision)
            return;

        // Damaged blocks darken as they get closer to breaking
        MeshOptions opt = blockMeshOptions();
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

        // Only what is in view is drawn: a late wave has a couple of hundred enemies all around
        const ViewPlanes seen(glm::perspective(fov() * DEG, aspect, 0.1f, 500.0f) * view);

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
        std::vector<float> gems;            // the items circling the players (a plain gem)
        std::map<std::string, std::vector<float>> badges;   // ... shaped as an icon, by icon
        std::vector<float> reachRings;      // reach: how far each attack reaches
        lookPoints.clear();

        // Shadows: a soft dark disc on the ground under each fighter and projectile, fainter and a little wider the
        // higher it is, gone past 10 m: where it is over the ground, and how high, read at a glance
        std::vector<float> shadows;
        auto shadow = [&](const glm::vec3& at, float radius, float strength) {
            const RayHit ground = raycast(world.grid, content.blockTable, at + glm::vec3(0, 0.25f, 0), glm::vec3(0, -1, 0), 10.25f);
            if (not ground.hit)
                return;
            const float height = std::max(0.0f, ground.distance - 0.25f);
            const float fade = strength * (1.0f - height / 10.0f);
            if (fade <= 0.02f)
                return;
            const float r = radius * (1.0f + 0.08f * height);
            const glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(at.x, at.y + 0.25f - ground.distance + 0.015f, at.z)) * glm::scale(glm::mat4(1.0f), glm::vec3(r * 2.0f, 1.0f, r * 2.0f));
            pushInstance(shadows, view, m, glm::vec4(0.02f, 0.03f, 0.05f, fade), glm::vec4(0.0f));
        };

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

            // Its blast went off: nothing of it is left
            if (f.blownUp)
                continue;

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

            // A blast counting down on it, 0..1: its lit fuse (an attack that ends in its death: the first half), then,
            // dead, until its blast goes off (the second half)
            float countdown = -1.0f;
            const BlastDef* ownBlast = nullptr;
            if (f.team == Team::Enemy)
            {
                if (auto d = content.enemies.find(f.type); d != content.enemies.end() and d->second.blast.any())
                    ownBlast = &d->second.blast;
                if (attack and attack->dies and f.alive())
                    countdown = 0.5f * glm::clamp(frame / std::max(1.0f, static_cast<float>(attack->startup + attack->active)), 0.0f, 1.0f);
                else if (not f.alive())
                    for (const auto& z : world.zones)
                        if (z.blasting() and z.owner == f.id)
                            countdown = 0.5f + 0.5f * glm::clamp(1.0f - z.remaining / std::max(STEP_SECONDS, z.duration), 0.0f, 1.0f);
            }

            // Out of view: none of it is drawn. The sphere takes in the weapon's reach and a body thrown about by
            // its action; a heavy attack's tell lands further ahead on the ground, and the first person view model
            // is always in front of the eye
            if (f.id != world.localPlayerId and not (attack and attack->tellHeavy) and
                not seen.sees(pos + glm::vec3(0, h * 0.5f, 0), std::max(h, f.body.width) + 1.5f))
                continue;

            // Its shadow (the player's own in first person has no body to cast it)
            if (not (firstPerson and f.team == Team::Player and f.id == world.localPlayerId))
                shadow(pos, std::max(0.35f, f.body.width * 0.82f), 0.46f);

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
                else if (f.action == Action::Dead and countdown >= 0.0f)
                {
                    // Dead with its blast to come: slumped forward on its knees, what it carries (a keg, its fuse) up
                    body = body * glm::translate(glm::mat4(1.0f), glm::vec3(0, -h * 0.18f, 0)) * rotX(38.0f);
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

            glm::mat4 body = bodyAt(frame);

            // Counting down: it shakes, harder, and swells as it comes
            if (countdown >= 0.0f)
            {
                const float u = countdown, jitter = 0.04f * u;
                const glm::vec3 shake(jitter * std::sin(simTime * 61.0f + static_cast<float>(f.id)), 0.0f, jitter * std::cos(simTime * 53.0f + static_cast<float>(f.id) * 1.7f));
                const float swell = 1.0f + 0.24f * u * u + 0.04f * u * std::sin(simTime * 38.0f);
                body = glm::translate(glm::mat4(1.0f), shake) * body * glm::translate(glm::mat4(1.0f), glm::vec3(0, h * 0.4f, 0)) *
                       glm::scale(glm::mat4(1.0f), glm::vec3(swell)) * glm::translate(glm::mat4(1.0f), glm::vec3(0, -h * 0.4f, 0));
            }

            const glm::mat4 modelMatrix = body * glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), -model.pivot);

            // ... and sparks fly off its fuse (effects.json "fuse"; a blast's "fuse" is where it is on the model)
            if (countdown >= 0.0f)
            {
                const glm::vec3 fuse = ownBlast and ownBlast->hasFuse ? ownBlast->fuse : glm::vec3(model.pivot.x, static_cast<float>(model.grid.height()), model.pivot.z);
                lookPoints.push_back({f.id, "fuse", static_cast<int>(lookPoints.size()), glm::vec3(modelMatrix * glm::vec4(fuse, 1.0f))});
            }

            // Hit flash (white), wind-up glow (orange, growing), stagger (yellow pulse)
            glm::vec4 flash(0.0f);

            const EffectDef* look = statusLook(f);

            // A weapon skill being charged: a glow that builds (full: it pulses)
            const bool charging = attack and attack->charge.at >= 0 and f.chargeFrames > 0 and frame <= static_cast<float>(attack->charge.at + 1);
            const float charge = f.chargeRatio(attack);

            // A technique held to charge (a shadow dash) glows the body; a weapon's own draw (a bow's) glows the weapon only
            const bool weaponCharge = charging and (f.chargeSlot == Fighter::CHARGE_LIGHT or f.chargeSlot == Fighter::CHARGE_HEAVY);
            const glm::vec4 chargeGlow(0.65f, 0.85f, 1.0f, charge >= 1.0f ? 0.55f + 0.2f * std::sin(simTime * 20.0f) : 0.15f + 0.4f * charge);

            if (countdown >= 0.0f)
            {
                // A blast coming: it blinks red and white-hot, faster and faster
                const float u = countdown, rate = 2.0f + 16.0f * u * u;
                const bool hot = std::fmod(simTime * rate + static_cast<float>(f.id) * 0.3f, 1.0f) < 0.5f;
                flash = hot ? glm::vec4(1.0f, 0.95f, 0.75f, 0.55f + 0.3f * u) : glm::vec4(1.0f, 0.2f, 0.08f, 0.4f + 0.25f * u);
            }
            else if (charging and not weaponCharge)
                flash = chargeGlow;
            else if (f.flash > 0)
                flash = glm::vec4(1.0f, 1.0f, 1.0f, 0.85f * static_cast<float>(f.flash) / static_cast<float>(std::max(1, tuning.feel.hitFlashFrames)));
            else if (f.shielded)
                flash = glm::vec4(0.55f, 0.9f, 1.0f, 0.35f + 0.15f * std::sin(simTime * 6.0f));
            else if (look)
                flash = glm::vec4(look->tint, heldStill(f) ? 0.6f : 0.32f + 0.12f * std::sin(simTime * 9.0f + static_cast<float>(f.id)));
            else if (attack and f.team == Team::Enemy and frame < attack->startup)
                flash = attack->tellHeavy ? glm::vec4(1.0f, 0.12f, 0.08f, 0.65f * easeInOut(frame / std::max(1.0f, static_cast<float>(attack->startup))))
                                          : glm::vec4(1.0f, 0.45f, 0.1f, 0.55f * easeInOut(frame / std::max(1.0f, static_cast<float>(attack->startup))));
            else if (f.action == Action::Stagger)
                flash = glm::vec4(1.0f, 0.9f, 0.3f, 0.25f + 0.15f * std::sin(frame * 0.6f));
            else if (f.elite and f.alive())
            {
                // An elite glows its affix's colour (gold without one); with two, it goes from one to the other
                glm::vec3 glow(1.0f, 0.75f, 0.2f);
                if (not f.affixes.empty())
                {
                    const float t = simTime * 0.7f + static_cast<float>(f.id) * 0.37f;
                    const size_t n = f.affixes.size(), at = static_cast<size_t>(t) % n;
                    auto colourOf = [&](size_t k) {
                        auto a = content.affixes.find(f.affixes[k]);
                        return a != content.affixes.end() ? a->second.colour : glow;
                    };
                    const float blend = n > 1 ? glm::smoothstep(0.75f, 1.0f, t - std::floor(t)) : 0.0f;
                    glow = glm::mix(colourOf(at), colourOf((at + 1) % n), blend);
                }
                flash = glm::vec4(glow, 0.34f + 0.12f * std::sin(simTime * 4.0f + static_cast<float>(f.id)));
            }

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

            // The items it has: worn (a piece on a body part, moving with it) or circling it (a gem)
            if (f.team == Team::Player and not viewModel and not f.boons.empty())
            {
                // Each item once, glowing by the rarest it was taken at
                std::map<std::string, int> rarest;
                for (const auto& b : f.boons)
                {
                    auto [it, fresh] = rarest.emplace(b.id, b.rarity);
                    if (not fresh)
                        it->second = std::max(it->second, b.rarity);
                }

                int gemCount = 0, gem = 0;
                for (const auto& [id, rarity] : rarest)
                    if (auto u = content.upgrades.find(id); u != content.upgrades.end() and u->second.look.orbit)
                        ++gemCount;

                const Gait& gait = gaits[f.id];

                for (const auto& [id, rarity] : rarest)
                {
                    auto u = content.upgrades.find(id);
                    if (u == content.upgrades.end() or u->second.look.empty())
                        continue;

                    const LookDef& look = u->second.look;
                    const bool flashing = flash.a > 0.0f;

                    // Its rarity: a glow (effects.json "rarity:rare"...), pulsing for Epic, pulsing harder and gold for
                    // Legendary; those two shed sparkles too
                    const char* rarityLook = rarity >= 3 ? "rarity:legendary" : rarity == 2 ? "rarity:epic" : rarity == 1 ? "rarity:rare" : nullptr;
                    const glm::vec3* rarityTint = rarityLook ? effectTint(rarityLook) : nullptr;
                    glm::vec4 glow = flash;
                    if (not flashing and rarityTint)
                    {
                        const float pulse = rarity >= 3 ? 0.42f + 0.2f * std::sin(simTime * 4.0f) : rarity == 2 ? 0.24f + 0.1f * std::sin(simTime * 2.2f) : 0.15f;
                        glow = glm::vec4(*rarityTint, pulse);
                    }
                    const bool sparkles = rarity >= 2 and rarityLook and f.alive();

                    for (const auto& piece : look.pieces)
                    {
                        auto pieceModel = content.models.find(piece.model);
                        if (pieceModel == content.models.end())
                            continue;

                        // On its body part, posed as the part is
                        glm::mat4 local(1.0f);
                        if (model.parts.size() > 1 and f.alive())
                            for (const auto& part : model.parts)
                                if (part.name == piece.part)
                                    local = glm::translate(glm::mat4(1.0f), part.joint) * partPose(part.name, f, gait, frame) * glm::translate(glm::mat4(1.0f), -part.joint);

                        const glm::mat4 m = modelMatrix * local * glm::translate(glm::mat4(1.0f), piece.at) * rotY(piece.yaw) *
                                            glm::scale(glm::mat4(1.0f), glm::vec3(piece.scale / scale)) * glm::translate(glm::mat4(1.0f), -pieceModel->second->pivot);
                        pushInstance(instances[piece.model], view, m, tint, glow);

                        if (sparkles)
                        {
                            const VoxelGrid& g = pieceModel->second->grid;
                            const glm::vec3 middle(static_cast<float>(g.width()) * 0.5f, static_cast<float>(g.height()) * 0.5f, static_cast<float>(g.depth()) * 0.5f);
                            lookPoints.push_back({f.id, rarityLook, static_cast<int>(lookPoints.size()), glm::vec3(m * glm::vec4(middle, 1.0f))});
                        }
                    }

                    if (look.orbit and f.alive())
                    {
                        // Rings of at most eight round the body, every other one higher or lower and turning the other way:
                        // many items stay apart. A shaped gem (an icon) faces the camera, so its shape always reads
                        constexpr int PER_RING = 8;
                        static const float ringHeights[] = {0.0f, 0.24f, -0.24f, 0.46f};
                        const int ring = gem / PER_RING, inRing = std::min(PER_RING, gemCount - ring * PER_RING);
                        const float turn = ring % 2 == 0 ? 1.0f : -1.0f;
                        const float a = turn * simTime * 1.2f + 6.2831853f * static_cast<float>(gem % PER_RING) / static_cast<float>(std::max(1, inRing)) + static_cast<float>(ring) * 0.39f;
                        const float radius = 0.8f + 0.05f * static_cast<float>(ring);
                        const glm::vec3 at = pos + glm::vec3(std::sin(a) * radius, h * 0.62f + ringHeights[std::min(ring, 3)] + 0.05f * std::sin(simTime * 3.0f + static_cast<float>(gem)), std::cos(a) * radius);
                        const glm::vec4 shine(look.colour, not flashing and rarity > 0 ? 0.6f : 0.42f);

                        if (const auto badge = look.icon.empty() ? nullptr : iconMesh(look.icon); badge and badge->faces() > 0)
                        {
                            const glm::mat4 m = glm::translate(glm::mat4(1.0f), at) * rotY(camera.viewYaw + 180.0f) * rotZ(10.0f * std::sin(simTime * 2.0f + static_cast<float>(gem))) *
                                                glm::scale(glm::mat4(1.0f), glm::vec3(0.032f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-3.5f, -3.5f, -1.0f));
                            pushInstance(badges[look.icon], view, m, glm::vec4(look.colour, 1.0f), rarityTint and not flashing and rarity >= 2 ? glm::vec4(glm::mix(look.colour, *rarityTint, 0.5f), glow.a + 0.3f) : shine);
                        }
                        else
                        {
                            const glm::mat4 m = glm::translate(glm::mat4(1.0f), at) * rotY(simTime * 140.0f + static_cast<float>(gem) * 40.0f) * rotX(45.0f) *
                                                glm::rotate(glm::mat4(1.0f), 45.0f * DEG, glm::vec3(0, 0, 1)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.085f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f));
                            pushInstance(gems, view, m, glm::vec4(look.colour, 1.0f), shine);
                        }

                        if (sparkles)
                            lookPoints.push_back({f.id, rarityLook, static_cast<int>(lookPoints.size()), at});
                        ++gem;
                    }
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
                        pose.rollFrom = weapon.rest.rollFrom;

                        if (attack)
                            pose = poseAt(*attack, weapon.rest, fr);

                        // Hand in the body frame; x is the fighter's right, which is the model's -X
                        glm::vec3 hand(-tuning.player.hand.x, tuning.player.hand.y, tuning.player.hand.z);

                        // Held out in the fist, where the arm points (a bow at the shoulder): the end of the arm,
                        // turned with it
                        if (attack and weapon.followArm)
                            for (const auto& part : model.parts)
                                if (part.name == "arm_r")
                                {
                                    const glm::vec3 shoulder = (part.joint - model.pivot) * scale;
                                    const float down = (part.length - 0.5f) * scale;
                                    hand = shoulder + glm::vec3(partPose(part.name, f, gaits[f.id], fr) * glm::vec4(hand.x - shoulder.x, -down, 0.0f, 0.0f));
                                }

                        glm::mat4 handFrame = bodyThen * glm::translate(glm::mat4(1.0f), hand) *
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
                            const glm::vec3 held = weapon.hasView ? weapon.view : fp.weapon;
                            handFrame = glm::translate(glm::mat4(1.0f), camera.position) * rotY(camera.viewYaw + around) * rotX(camera.viewPitch) *
                                        glm::scale(glm::mat4(1.0f), glm::vec3(fp.viewModelScale)) *
                                        glm::translate(glm::mat4(1.0f), glm::vec3(-held.x, held.y, held.z)) * rotZ(weapon.viewTilt);
                        }

                        const float roll = attack and weapon.followSwing ? swingRoll(*attack, weapon.rest.rollFrom, fr) : pose.rollFrom;

                        return handFrame *
                            rotY(-pose.yawFrom) * rotX(-pose.pitchFrom) * rotZ(roll) *
                            glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, pose.reach)) *
                            glm::scale(glm::mat4(1.0f), glm::vec3(weapon.modelScale)) *
                            glm::translate(glm::mat4(1.0f), -weapon.grip);
                    };

                    const VoxelModel& wm = *weaponModel->second;
                    const float bladeFrom = weapon.grip.z + 1.0f;
                    const float bladeLength = std::max(1.0f, static_cast<float>(wm.grid.depth()) - bladeFrom);
                    const glm::mat4 held = weaponAt(frame, body);

                    // The element its branch gives it (Flame blade): the weapon glows it, and it leaves the blade
                    // (effects.json "weapon:fire"...), so the branch shows before the first hit
                    const BranchDef* branch = world.weaponBranch(f);
                    const std::string elementLook = branch and branch->element != Element::None ? std::string("weapon:") + elementName(branch->element) : std::string();
                    const glm::vec3* elementTint = elementLook.empty() ? nullptr : effectTint(elementLook);

                    glm::vec4 weaponGlow = weaponCharge ? chargeGlow : (f.flash > 0 or charging ? flash * 0.5f : glm::vec4(0.0f));
                    if (weaponGlow.a <= 0.0f and elementTint)
                        weaponGlow = glm::vec4(*elementTint, 0.32f + 0.12f * std::sin(simTime * 5.0f));
                    pushInstance(instances[weapon.model], view, held, WHITE, weaponGlow);

                    // A blade sheds along its length; a bow (ranged: no blade) from one point, and much less
                    const bool ranged = weapon.role == "ranged";
                    if (not elementLook.empty())
                        for (const float along : {0.5f, 1.0f})
                            if (not ranged or along > 0.75f)
                                lookPoints.push_back({f.id, elementLook, static_cast<int>(lookPoints.size()),
                                                      glm::vec3(held * glm::vec4(weapon.grip.x, weapon.grip.y, bladeFrom + bladeLength * along, 1.0f)), ranged ? 0.4f : 1.0f});

                    // The slash: ghosts of the blade along the last frames of the swing, fading, while it
                    // can hit (and a moment after), so the reach and the arc of the attack read at a glance.
                    // In the attack's element (a branch's: Firestorm's flames), and as long as its area makes its reach
                    constexpr float TRAIL = 5.0f;

                    // (an attack that only shoots, a bow's arrow, has no slash: a ghost of the bow flashed white on release)
                    if (prefs.trails and attack and not attack->hitboxes.empty() and frame >= static_cast<float>(attack->startup) and
                        frame < static_cast<float>(attack->startup + attack->active + 2))
                    {
                        const float oldest = std::max(static_cast<float>(attack->startup) - 1.0f, frame - TRAIL);

                        glm::vec3 colour(0.9f, 0.95f, 1.0f);
                        if (attack->element != Element::None)
                        {
                            const ui::Colour e = ui::elementColour(elementName(attack->element));
                            colour = glm::mix(colour, glm::vec3(e) / 255.0f, 0.8f);
                        }

                        // The attack as written, to see how much wider its area made it (levels, a branch, items)
                        const AttackDef* written = nullptr;
                        if (not f.attack.empty() and f.attack[0] == '@')
                        {
                            if (auto a = content.abilities.find(f.attack.substr(1)); a != content.abilities.end())
                                written = &a->second.attack;
                        }
                        else
                            written = weapon.moveset.find(f.attack);
                        float wider = 1.0f;
                        if (written and not written->hitboxes.empty() and not attack->hitboxes.empty() and written->hitboxes[0].size.x > 0.0f)
                            wider = glm::clamp(attack->hitboxes[0].size.x / written->hitboxes[0].size.x, 1.0f, 3.0f);

                        for (float fr = frame; fr >= oldest; fr -= 0.34f)
                        {
                            const float age = (frame - fr) / TRAIL;
                            const float thick = 1.2f * std::sqrt(wider);
                            const glm::mat4 ghost = weaponAt(fr, bodyAt(fr)) * glm::translate(glm::mat4(1.0f), glm::vec3(weapon.grip.x - thick * 0.5f, weapon.grip.y - thick * 0.5f, bladeFrom)) *
                                                    glm::scale(glm::mat4(1.0f), glm::vec3(thick, thick, bladeLength * wider));
                            // Softer in first person: right in front of the eye
                            pushInstance(trail, view, ghost, glm::vec4(colour, (viewModel ? 0.28f : 0.5f) * (1.0f - age)), glm::vec4(0.0f));
                        }
                    }
                }
            }

            // Reach (reach): how far the attack can hit, a ring on the ground round the fighter from its start until
            // it stops hitting: its furthest hitbox, its lunge and its healing
            if (showReach and attack and f.alive() and frame < static_cast<float>(attack->startup + attack->active))
            {
                float reach = 0.0f;
                for (const auto& box : attack->hitboxes)
                    for (const float z : {box.offset.z - box.size.z * 0.5f, box.offset.z + box.size.z * 0.5f})
                        reach = std::max(reach, glm::length(glm::vec2(std::fabs(box.offset.x) + box.size.x * 0.5f, z)));
                if (reach > 0.0f)
                    reach += attack->lunge * static_cast<float>(std::max(0, attack->lungeTo - attack->lungeFrom)) / 60.0f;
                reach = std::max({reach, attack->heal.radius, attack->healZone.radius});

                if (reach > 0.0f)
                {
                    glm::vec3 colour = f.team == Team::Player ? glm::vec3(1.0f, 0.84f, 0.47f) : glm::vec3(1.0f, 0.3f, 0.2f);
                    if (attack->element != Element::None)
                        colour = glm::vec3(ui::elementColour(elementName(attack->element))) / 255.0f;
                    const bool striking = frame >= static_cast<float>(attack->startup);
                    const glm::mat4 m = glm::translate(glm::mat4(1.0f), pos + glm::vec3(0, 0.03f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(reach * 2.0f, 1.0f, reach * 2.0f));
                    pushInstance(reachRings, view, m, glm::vec4(colour, striking ? 0.9f : 0.45f), glm::vec4(0.0f));
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

        // A shielded boss: a ring of light round its feet, and a beam to it from each crystal holding the shield
        {
            std::vector<float> rings, beams;

            for (const auto& f : world.fighters)
            {
                auto def = f.shielded and f.alive() ? content.enemies.find(f.type) : content.enemies.end();
                if (def == content.enemies.end())
                    continue;

                glm::vec3 pos = f.body.position;
                if (auto it = previous.find(f.id); it != previous.end())
                    pos = glm::mix(it->second.position, f.body.position, alpha);

                const float pulse = 0.5f + 0.5f * std::sin(simTime * 6.0f);
                const float r = std::max(f.body.width, 1.0f) * 1.4f;
                const glm::vec4 light(0.55f, 0.9f, 1.0f, 1.0f);

                if (seen.sees(pos, r))
                    pushInstance(rings, view, glm::translate(glm::mat4(1.0f), pos + glm::vec3(0, 0.06f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(r * 2.0f, 1.0f, r * 2.0f)),
                                 glm::vec4(glm::vec3(light), 0.45f + 0.35f * pulse), glm::vec4(0.0f));

                const glm::vec3 to = pos + glm::vec3(0, f.body.height * 0.6f, 0);
                for (const auto& c : world.fighters)
                {
                    if (c.type != def->second.phases.crystal or not c.alive())
                        continue;

                    const glm::vec3 from = c.body.position + glm::vec3(0, c.body.height * 0.8f, 0);
                    const glm::vec3 d = to - from;
                    const float length = glm::length(d);
                    if (length < 0.1f)
                        continue;

                    const glm::vec3 dir = d / length;
                    const float yaw = std::atan2(dir.x, dir.z) / DEG;
                    const float pitch = -std::asin(glm::clamp(dir.y, -1.0f, 1.0f)) / DEG;
                    const float width = 0.06f + 0.03f * pulse;
                    const glm::mat4 m = glm::translate(glm::mat4(1.0f), from) * rotY(yaw) * rotX(pitch) * glm::scale(glm::mat4(1.0f), glm::vec3(width, width, length)) *
                                        glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, -0.5f, 0.0f));
                    pushInstance(beams, view, m, glm::vec4(glm::vec3(light), 0.35f + 0.3f * pulse), glm::vec4(glm::vec3(light), 0.6f));
                }
            }

            add(ringMesh, debugMaterial, OpacityType::Normal, std::move(rings));
            add(boxMesh, debugMaterial, OpacityType::Normal, std::move(beams));
        }

        // Spell zones: a disc on the ground in the element's colour (effects.json "zone:<element>"), fading at the end.
        // Each a little above the one before it, so zones that overlap never share a plane (they would flicker)
        std::vector<float> zoneDiscs, zoneRings;
        for (size_t i = 0; i < world.zones.size(); ++i)
        {
            const Zone& z = world.zones[i];
            if (not seen.sees(z.position, z.radius))
                continue;

            const float lift = 0.04f + 0.012f * static_cast<float>(i % 12);
            const glm::mat4 m = glm::translate(glm::mat4(1.0f), z.position + glm::vec3(0, lift, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(z.radius * 2.0f, 1.0f, z.radius * 2.0f));

            // A blast about to go off: a red ring, filling in from the middle as it comes
            if (z.blasting())
            {
                const float progress = glm::clamp(1.0f - z.remaining / std::max(0.01f, z.duration), 0.0f, 1.0f);
                const float pulse = 0.5f + 0.5f * std::sin(simTime * 18.0f);
                const glm::mat4 fill = glm::translate(glm::mat4(1.0f), z.position + glm::vec3(0, lift + 0.002f, 0)) *
                                       glm::scale(glm::mat4(1.0f), glm::vec3(z.radius * 2.0f * progress, 1.0f, z.radius * 2.0f * progress));
                pushInstance(zoneDiscs, view, m, glm::vec4(1.0f, 0.15f, 0.1f, 0.14f), glm::vec4(0.0f));
                pushInstance(zoneDiscs, view, fill, glm::vec4(1.0f, 0.2f, 0.1f, 0.3f), glm::vec4(0.0f));
                pushInstance(zoneRings, view, glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.004f, 0)) * m, glm::vec4(1.0f, 0.2f, 0.12f, 0.7f + 0.3f * pulse), glm::vec4(0.0f));
                continue;
            }

            auto look = effects.find(z.heal > 0.0f ? std::string("zone:heal") : std::string("zone:") + elementName(z.element));
            const glm::vec3 tint = look != effects.end() and look->second.hasTint ? look->second.tint : glm::vec3(1.0f);
            const float fade = glm::clamp(z.remaining / 0.6f, 0.0f, 1.0f) * glm::clamp((z.duration - z.remaining) / 0.2f + 0.3f, 0.0f, 1.0f);
            pushInstance(zoneDiscs, view, m, glm::vec4(tint, (z.heal > 0.0f ? 0.3f : 0.18f) * fade), glm::vec4(0.0f));   // a healing zone shows more: you go to it
            // The enemies' ground hurts the players: its edge is red
            const glm::vec3 edge = z.team == Team::Enemy ? glm::mix(tint, glm::vec3(1.0f, 0.15f, 0.1f), 0.6f) : tint;
            pushInstance(zoneRings, view, glm::translate(glm::mat4(1.0f), glm::vec3(0, 0.004f, 0)) * m, glm::vec4(edge, 0.7f * fade), glm::vec4(0.0f));
        }
        // Blasts going off: a ring sweeping out to the radius, thicker as it goes, and a white-hot flash at the heart
        for (const auto& w : shockwaves)
        {
            const float t = glm::clamp(w.age / 0.45f, 0.0f, 1.0f), out = easeOut(glm::clamp(w.age / 0.3f, 0.0f, 1.0f));
            const float r = w.radius * (0.2f + 0.85f * out);
            for (const float band : {1.0f, 0.93f, 0.86f})
            {
                const glm::mat4 m = glm::translate(glm::mat4(1.0f), w.at + glm::vec3(0, 0.03f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(r * 2.0f * band, 1.0f, r * 2.0f * band));
                pushInstance(zoneRings, view, m, glm::vec4(1.0f, 0.75f + 0.2f * band, 0.45f, (1.0f - t) * 0.9f), glm::vec4(0.0f));
            }
            if (w.age < 0.15f)
            {
                const glm::mat4 m = glm::translate(glm::mat4(1.0f), w.at + glm::vec3(0, 0.035f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(w.radius * 1.4f, 1.0f, w.radius * 1.4f));
                pushInstance(zoneDiscs, view, m, glm::vec4(1.0f, 0.92f, 0.7f, 0.55f * (1.0f - w.age / 0.15f)), glm::vec4(0.0f));
            }
        }

        add(discMesh, debugMaterial, OpacityType::Normal, std::move(zoneDiscs));
        add(ringMesh, debugMaterial, OpacityType::Normal, std::move(zoneRings));

        // A run's spawn points while they are warned of: a red column, pulsing, over a patch of ground
        for (const auto& m : spawnMarks)
        {
            const float size = m.boss ? 2.4f : 1.0f, pulse = 0.5f + 0.5f * std::sin(simTime * 12.0f);
            const glm::vec4 colour = m.boss    ? glm::vec4(0.75f, 0.2f, 1.0f, 0.35f + 0.2f * pulse)
                                     : m.elite ? glm::vec4(1.0f, 0.78f, 0.2f, 0.35f + 0.2f * pulse)
                                               : glm::vec4(1.0f, 0.2f, 0.15f, 0.3f + 0.2f * pulse);
            const glm::mat4 column = glm::translate(glm::mat4(1.0f), m.position) * glm::scale(glm::mat4(1.0f), glm::vec3(size * 0.5f, 3.0f, size * 0.5f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0, -0.5f));
            const glm::mat4 patch = glm::translate(glm::mat4(1.0f), m.position + glm::vec3(0, 0.03f, 0)) * glm::scale(glm::mat4(1.0f), glm::vec3(size * 1.6f, 0.04f, size * 1.6f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, 0, -0.5f));
            pushInstance(danger, view, column, colour, glm::vec4(0.0f));
            pushInstance(danger, view, patch, colour * glm::vec4(1, 1, 1, 0.8f), glm::vec4(0.0f));
        }

        add(boxMesh, debugMaterial, OpacityType::Normal, std::move(trail));
        add(boxMesh, debugMaterial, OpacityType::Normal, std::move(danger));
        add(boxMesh, litMaterial, OpacityType::Opaque, std::move(gems));
        for (auto& [icon, list] : badges)
            add(iconMesh(icon), litMaterial, OpacityType::Opaque, std::move(list));
        add(ringMesh, debugMaterial, OpacityType::Normal, std::move(reachRings));

        // Projectiles, along their flight
        for (const auto& pr : world.projectiles)
        {
            auto modelIt = content.models.find(pr.def.model);

            if (modelIt == content.models.end())
                continue;

            if (not seen.sees(pr.position, 1.5f))
                continue;

            const VoxelModel& m = *modelIt->second;
            const glm::vec3 dir = glm::length(pr.velocity) > 1e-4f ? glm::normalize(pr.velocity) : glm::vec3(0, 0, 1);
            const float yaw = std::atan2(dir.x, dir.z) / DEG;
            const float pitch = -std::asin(glm::clamp(dir.y, -1.0f, 1.0f)) / DEG;
            const glm::vec3 centre(m.grid.width() * 0.5f, m.grid.height() * 0.5f, m.grid.depth() * 0.5f);
            const glm::vec3 at = pr.position + pr.velocity * (alpha * STEP_SECONDS);

            const glm::mat4 mm = glm::translate(glm::mat4(1.0f), at) * rotY(yaw) * rotX(pitch) * glm::scale(glm::mat4(1.0f), glm::vec3(pr.def.modelScale)) * glm::translate(glm::mat4(1.0f), -centre);
            pushInstance(instances[pr.def.model], view, mm, WHITE, glm::vec4(0.0f));
            shadow(at, std::max(0.12f, pr.def.radius * 1.4f), 0.3f);
        }
        add(discMesh, debugMaterial, OpacityType::Normal, std::move(shadows));

        // A run's shards: little spinning crystals, bobbing where they lie, streaking to whoever picks them up
        if (not world.pickups.empty())
        {
            std::vector<float> d;

            for (const auto& pk : world.pickups)
            {
                if (not seen.sees(pk.position, 1.0f))
                    continue;

                // A present: a gold box with a red ribbon, turning slowly, bigger than any shard
                if (pk.present)
                {
                    const float bob = pk.target ? 0.0f : 0.1f * std::sin(simTime * 2.5f + static_cast<float>(pk.id));
                    const glm::vec3 at = pk.position + pk.velocity * (alpha * STEP_SECONDS) + glm::vec3(0, 0.15f + bob, 0);
                    const glm::mat4 turn = glm::translate(glm::mat4(1.0f), at) * rotY(simTime * 60.0f + static_cast<float>(pk.id) * 37.0f);
                    const glm::mat4 box = turn * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f));
                    const glm::mat4 band = turn * glm::scale(glm::mat4(1.0f), glm::vec3(0.54f, 0.54f, 0.14f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f));
                    const glm::mat4 band2 = turn * glm::scale(glm::mat4(1.0f), glm::vec3(0.14f, 0.54f, 0.54f)) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f));
                    const float glow = 0.25f + 0.15f * std::sin(simTime * 5.0f);
                    pushInstance(d, view, box, glm::vec4(1.0f, 0.8f, 0.25f, 1.0f), glm::vec4(1.0f, 0.85f, 0.4f, glow));
                    pushInstance(d, view, band, glm::vec4(0.85f, 0.15f, 0.2f, 1.0f), glm::vec4(0.0f));
                    pushInstance(d, view, band2, glm::vec4(0.85f, 0.15f, 0.2f, 1.0f), glm::vec4(0.0f));
                    continue;
                }

                const float s = 0.16f + 0.04f * static_cast<float>(std::min(pk.value, 5));
                const float bob = pk.target ? 0.0f : 0.08f * std::sin(simTime * 3.0f + static_cast<float>(pk.id));
                const glm::vec3 at = pk.position + pk.velocity * (alpha * STEP_SECONDS) + glm::vec3(0, bob, 0);
                const glm::mat4 m = glm::translate(glm::mat4(1.0f), at) * rotY(simTime * 140.0f + static_cast<float>(pk.id) * 37.0f) * rotX(45.0f) *
                                    glm::scale(glm::mat4(1.0f), glm::vec3(s)) * glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f));
                pushInstance(d, view, m, glm::vec4(0.62f, 0.45f, 1.0f, 1.0f), glm::vec4(0.7f, 0.55f, 1.0f, 0.35f));
            }

            add(boxMesh, litMaterial, OpacityType::Opaque, std::move(d));
        }

        // Beams' bolts: a bright core in a soft glow, fading; lightning zigzags and flickers
        if (not strikes.empty())
        {
            std::vector<float> d;

            auto segment = [&](const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& tint, const glm::vec4& flash) {
                const glm::vec3 dd = b - a;
                const float length = glm::length(dd);
                if (length < 1e-3f)
                    return;
                const glm::vec3 dir = dd / length;
                const float yaw = std::atan2(dir.x, dir.z) / DEG;
                const float pitch = -std::asin(glm::clamp(dir.y, -1.0f, 1.0f)) / DEG;
                const glm::mat4 m = glm::translate(glm::mat4(1.0f), a) * rotY(yaw) * rotX(pitch) * glm::scale(glm::mat4(1.0f), glm::vec3(width, width, length)) *
                                    glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, -0.5f, 0.0f));
                pushInstance(d, view, m, tint, flash);
            };

            for (const auto& st : strikes)
            {
                const glm::vec3 from = st.fromHand ? handPoint() : st.from;
                const glm::vec3 line = st.to - from;
                const float length = glm::length(line);
                if (length < 0.05f or not seen.sees((from + st.to) * 0.5f, length * 0.5f + 1.0f))
                    continue;

                // The points along it: lightning off the line by up to `jagged` (none at the ends), redrawn 30 times a second
                const int pieces = st.look.jagged > 0.0f ? std::clamp(static_cast<int>(length / 0.7f), 2, 32) : (st.fromHand ? 6 : 1);
                const glm::vec3 dir = line / length;
                glm::vec3 side = glm::cross(dir, glm::vec3(0, 1, 0));
                side = glm::length(side) > 1e-4f ? glm::normalize(side) : glm::vec3(1, 0, 0);
                const glm::vec3 lift = glm::cross(side, dir);
                std::mt19937 rng(st.seed + static_cast<uint32_t>(st.age * 30.0f));
                std::uniform_real_distribution<float> u(-1.0f, 1.0f);

                std::vector<glm::vec3> points = {from};
                for (int k = 1; k < pieces; ++k)
                {
                    const float t = static_cast<float>(k) / static_cast<float>(pieces);
                    const float swing = st.look.jagged * std::sin(t * 3.14159265f);
                    points.push_back(from + line * t + (side * u(rng) + lift * u(rng)) * swing);
                }
                points.push_back(st.to);

                // Bright at once, then fading out; thinner as it goes
                const float t = st.age / st.look.life;
                const float fade = 1.0f - t * t;
                const float width = st.look.width * (1.0f - 0.5f * t);
                const glm::vec3 core = glm::mix(st.look.color, glm::vec3(1.0f), 0.6f);

                for (size_t k = 0; k + 1 < points.size(); ++k)
                {
                    // From the hand: thin by the eye, full width a couple of metres on
                    const float taper = st.fromHand ? glm::clamp(glm::length((points[k] + points[k + 1]) * 0.5f - from) / 2.5f, 0.2f, 1.0f) : 1.0f;
                    segment(points[k], points[k + 1], width * taper, glm::vec4(core, fade), glm::vec4(core, 1.0f));
                    segment(points[k], points[k + 1], width * taper * 3.0f, glm::vec4(st.look.color, 0.3f * fade), glm::vec4(st.look.color, 1.0f));
                }
            }

            add(boxMesh, debugMaterial, OpacityType::Normal, std::move(d));
        }

        // Particles: little lit cubes, shrinking away
        if (not particles.all().empty())
        {
            std::vector<float> d;
            d.reserve(particles.all().size() * GpuMesh::INSTANCE_FLOATS);

            for (const auto& q : particles.all())
            {
                if (not seen.sees(q.position, q.size))
                    continue;

                const float s = q.size * std::sqrt(std::max(0.0f, q.life / q.maxLife));
                const glm::mat4 m = glm::translate(glm::mat4(1.0f), q.position - glm::vec3(s * 0.5f)) * glm::scale(glm::mat4(1.0f), glm::vec3(s));
                pushInstance(d, view, m, glm::vec4(q.color, 1.0f), glm::vec4(q.color, q.glow));
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

        // The lab's scene, at the same time, with its own waits
        while (labWaitSteps <= 0 and std::chrono::steady_clock::now() >= labSleepUntil and not labQueue.empty())
        {
            const std::string line = labQueue.front();
            labQueue.pop_front();

            runningLab = true;
            const std::string said = runCommand(line);
            runningLab = false;
            print(said);
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
            return "reload | tune <key> [value] | tunes [prefix] | spawn <enemy> [count] | elite <enemy> [affix ...] | clear | reset\n"
                   "place x y z <block|air> | fill x0 y0 z0 x1 y1 z1 <block|air>\n"
                   "view [first|third] | reach [on|off] | weapon <id> | item <id|all> [rarity] | unitems | grow <weapon|technique> <level> [branch] | bot | weapons | god | stamina | mana | ai | hitboxes | overlay | pause | step [n] | slowmo <x> | pacer [n|off [fps]] | players | spell <1-3> <recipe> | spells | spellbook | arena | map [arena] | training | difficulty <n> | wave <n> | duel | timeup | join <address> | leave | shards <n> | kill | hurt <damage> | options | mods | errors | warnings\n"
                   "the lab: lab [on|off] (F3) | lab show <id> [level] [branch] | cam <yaw> <pitch> [distance] | orbit [deg/s] | face <yaw> [pitch] | put <enemy> x y z [yaw] | caption <text>\n"
                   "scripted input: mouse [on|off] | click <x> <y> | hover <x> <y> | press <action> | hold <action> <steps> | wait <steps> | sleep <seconds> | look <yaw> <pitch> | teleport x y z  (join with ;)";
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
            (runningLab ? labWaitSteps : waitSteps) = args.size() > 1 ? std::max(0, std::atoi(args[1].c_str())) : 60;
            return "";
        }

        if (cmd == "sleep")
        {
            // Real time, unlike wait: lets a script look at a paused frame
            const float seconds = args.size() > 1 ? static_cast<float>(std::atof(args[1].c_str())) : 1.0f;
            (runningLab ? labSleepUntil : sleepUntil) = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(seconds * 1000.0f));
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

        if (cmd == "click" and args.size() > 2)
        {
            // A click on the UI at window pixels, as the mouse would make it
            std::lock_guard<std::mutex> lock(inputMutex);
            uiClicks.emplace_back(static_cast<float>(std::atof(args[1].c_str())), static_cast<float>(std::atof(args[2].c_str())));
            mouseX = uiClicks.back().x;
            mouseY = uiClicks.back().y;
            return "";
        }

        if (cmd == "hover" and args.size() > 2)
        {
            // The mouse over the UI at window pixels, no click (a tooltip shows)
            std::lock_guard<std::mutex> lock(inputMutex);
            mouseX = static_cast<float>(std::atof(args[1].c_str()));
            mouseY = static_cast<float>(std::atof(args[2].c_str()));
            return "";
        }

        if (cmd == "pad")
        {
            // A controller's events, as SDL sends them (scripts, tests): pad press a | pad release a | pad axis leftx 0.8
            if (args.size() < 3)
                return "pad press|release <button> (a, b, x, y, leftshoulder, lefttrigger, dpup, start...) | pad axis <leftx|lefty|rightx|righty|lefttrigger|righttrigger> <-1..1>";

            if (args[1] == "axis")
            {
                const SDL_GameControllerAxis axis = SDL_GameControllerGetAxisFromString(args[2].c_str());
                if (axis == SDL_CONTROLLER_AXIS_INVALID or args.size() < 4)
                    return "pad axis: no axis " + args[2];
                const float v = std::clamp(static_cast<float>(std::atof(args[3].c_str())), -1.0f, 1.0f);
                onEvent(OnSDLGamepadAxisChanged{0, static_cast<unsigned int>(axis), static_cast<int>(v * 32767.0f)});
                return "";
            }

            const SDL_GameControllerButton button = SDL_GameControllerGetButtonFromString(args[2].c_str());
            if (button == SDL_CONTROLLER_BUTTON_INVALID)
                return "pad: no button " + args[2];
            if (args[1] == "press")
                onEvent(OnSDLGamepadPressed{0, static_cast<unsigned int>(button)});
            else
                onEvent(OnSDLGamepadReleased{0, static_cast<unsigned int>(button)});
            return "";
        }

        if (cmd == "mouse")
        {
            mouseLook = args.size() > 1 ? args[1] != "off" : not mouseLook;
            cursorFree = not mouseLook;   // off also lets go of the cursor: a scripted run must not hold the user's mouse
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

        // ---- The lab, and the commands its scenes are made of ----
        if (cmd == "lab")
        {
            if (networked())
                return "lab: only in a game alone";

            // lab show <id> [level] [branch]: the lab open on that
            if (args.size() > 2 and args[1] == "show")
            {
                setLabOpen(true);
                if (not lab->pick(args[2], args.size() > 3 ? std::atoi(args[3].c_str()) : 1, args.size() > 4 ? std::atoi(args[4].c_str()) : -1))
                    return "lab: nothing called " + args[2];
                labShow();
                return "";
            }

            setLabOpen(args.size() > 1 ? args[1] != "off" : not labOpen);
            return labOpen ? "the lab: " + lab->status : "the lab is hidden (F3 shows it)";
        }

        if (cmd == "cam")
        {
            // The camera from outside: where it looks from (yaw, pitch) and how far
            if (args.size() < 3)
                return "cam <yaw> <pitch> [distance]: the third person camera, at once";
            if (runningLab and labOwnCamera)
                return "";   // the player moved the camera: a scene leaves it be
            camera.yaw = static_cast<float>(std::atof(args[1].c_str()));
            camera.pitch = glm::clamp(static_cast<float>(std::atof(args[2].c_str())), tuning.camera.minPitch, tuning.camera.maxPitch);
            if (args.size() > 3)
                camera.distance = camera.currentDistance = glm::clamp(static_cast<float>(std::atof(args[3].c_str())), 1.0f, 20.0f);
            return "";
        }

        if (cmd == "orbit")
        {
            // A number: how fast the scene turns the camera round the player (0 stops it). Alone: the player turns it
            // off or on, and that stays over the scenes until the next time
            if (args.size() > 1)
                labOrbit = static_cast<float>(std::atof(args[1].c_str()));
            else
                labOrbitChoice = labTurn() != 0.0f ? LabOrbitChoice::Off : LabOrbitChoice::On;
            return labTurn() != 0.0f ? "orbit " + fmt(labTurn(), 0) + " degrees a second" : "orbit off";
        }

        if (cmd == "face")
        {
            // Where the player turns and fights towards in the lab, whatever the camera does
            if (args.size() < 2)
                return "face <yaw> [pitch]: where the player's attacks go in the lab, and how far down its spells aim (" + fmt(labAimPitch, 0) + ")";
            labFaceYaw = static_cast<float>(std::atof(args[1].c_str()));
            if (args.size() > 2)
                labAimPitch = glm::clamp(static_cast<float>(std::atof(args[2].c_str())), -60.0f, 60.0f);
            if (p)
                p->yaw = labFaceYaw;
            if (firstPerson)
            {
                camera.yaw = labFaceYaw;
                camera.pitch = labAimPitch;
            }
            return "";
        }

        if (cmd == "caption")
        {
            labCaption = line.size() > 8 ? line.substr(8) : "";
            return "";
        }

        if (cmd == "unitems" and p)
        {
            // The items taken back: the player as it starts
            p->boons.clear();
            world.sumBoons(*p);
            p->maxHealth = world.playerMaxHealth(*p);
            p->health = p->maxHealth;
            return "items taken back";
        }

        if (cmd == "grow")
        {
            // A weapon's or a technique's level and branch, as a run's level ups give them (in the lab, outside a run too)
            if (args.size() < 3 or not p)
                return "grow <weapon|technique> <level> [branch]: its levels and branch, as a run grows them (branch -1: none)";
            if (client)
                return "grow: only the host can do that";

            const std::string id = resolveId(args[1], "base");
            if (not content.weapons.count(id) and not content.abilities.count(id))
                return "grow: no weapon or technique " + id;

            auto it = std::find_if(p->runSkills.begin(), p->runSkills.end(), [&](const Fighter::RunSpell& rs) { return rs.id == id; });
            if (it == p->runSkills.end())
                it = p->runSkills.insert(p->runSkills.end(), Fighter::RunSpell{id, 1, -1});
            it->level = std::max(1, std::atoi(args[2].c_str()));
            it->branch = args.size() > 3 ? std::atoi(args[3].c_str()) : -1;
            if (not world.inRun() and not world.growsOutsideRun)
                return id + " level " + std::to_string(it->level) + ": it counts in a run, or in the lab";
            return id + " level " + std::to_string(it->level) + (it->branch >= 0 ? ", branch " + std::to_string(it->branch) : "");
        }

        if (cmd == "put" and args.size() > 4)
        {
            // An enemy at a point, facing yaw (spawn puts it in front of the camera); with affixes, an elite
            if (client)
                return "put: only the host can do that";
            const std::string id = resolveId(args[1], "base");
            if (not content.enemies.count(id))
                return "no enemy " + id;

            std::vector<std::string> affixes;
            for (size_t k = 6; k < args.size(); ++k)
            {
                const std::string affix = resolveId(args[k], "base");
                if (not content.affixes.count(affix))
                    return "no affix " + affix;
                affixes.push_back(affix);
            }

            const glm::vec3 at(std::atof(args[2].c_str()), std::atof(args[3].c_str()), std::atof(args[4].c_str()));
            const int spawned = world.spawnEnemy(id, at, args.size() > 5 ? static_cast<float>(std::atof(args[5].c_str())) : 180.0f);
            if (Fighter* e = world.find(spawned); e and not affixes.empty())
            {
                e->elite = true;
                e->affixes = affixes;
                e->maxHealth *= 2.5f * (1.0f + world.affixSum(*e, &AffixDef::health));
                e->health = e->maxHealth;
            }
            return "";
        }

        if (cmd == "players")
            return netStatus();

        // An idle extra player: the final duel and the players' recap, tried alone
        if (cmd == "bot")
        {
            if (networked())
                return "bot: only in a game alone";
            const int id = world.addPlayer(1000 + static_cast<int>(world.fighters.size()));
            if (Fighter* bot = world.find(id))
                bot->body.position = world.player() ? world.player()->body.position + glm::vec3(2.0f, 0.0f, 0.0f) : bot->body.position;
            return "bot: player " + std::to_string(id) + " added (it stands still)";
        }

        // Spells: the player's own, so a client may change them too
        if (cmd == "arena")
        {
            if (client)
                return "arena: the host starts the run (Enter at the end of one plays again)";

            // Straight to wave 1 at the level picked (scripts, tests); "arena choose": the difficulty screen first
            const bool choose = args.size() > 1 and args[1] == "choose";
            startArena(choose);
            return choose ? "the arena: choose the difficulty" : "the arena: wave 1 at " + content.run.difficulty(levelPicked).name;
        }

        if (cmd == "difficulty")
        {
            std::string out;
            for (int k = 0; k < content.run.levels(); ++k)
                out += std::string(k ? ", " : "") + std::to_string(k) + " " + content.run.difficulty(k).name + (k > levelOpen ? " (locked)" : "");

            if (args.size() < 2)
                return "difficulty <n>: open and pick a level (" + out + ")";
            if (client or world.inRun())
                return "difficulty: only the host, outside a run";

            levelOpen = std::max(levelOpen, std::atoi(args[1].c_str()));
            pickLevel(std::atoi(args[1].c_str()));
            world.run.difficulty = levelPicked;
            return "difficulty " + content.run.difficulty(levelPicked).name + ": " + arenaLine(levelPicked);
        }

        if (cmd == "item")
        {
            Fighter* me = world.player();
            if (args.size() < 2 or not me)
                return "item <id> [rarity 0-3]: give yourself a shop item (all: every item)";

            std::vector<std::string> ids;
            for (const auto& [id, u] : content.upgrades)
                if (args[1] == "all" or id == args[1] or localId(id) == args[1])
                    ids.push_back(id);
            if (ids.empty())
                return "item: no item \"" + args[1] + "\"";

            const int rarity = args.size() > 2 ? std::clamp(std::atoi(args[2].c_str()), 0, 3) : 0;
            for (const auto& id : ids)
                world.takeBoon(*me, {id, rarity});
            return "item: " + std::to_string(ids.size()) + " given";
        }

        if (cmd == "wave")
        {
            if (not world.inRun() or args.size() < 2)
                return "wave <n>: straight to a wave of the run (start one with: arena)";

            world.skipToWave(std::atoi(args[1].c_str()));
            return "wave " + std::to_string(world.run.wave);
        }

        if (cmd == "host")
        {
            if (networked())
                return "host: already in a game (leave first)";

            if (args.size() > 1)
                hostPortEdit = args[1] == "auto" ? "" : args[1];

            return hostNow() ? "" : "host: " + inviteMessage;
        }

        if (cmd == "map")
        {
            if (args.size() < 2)
            {
                std::string out = "map <arena|training>: the training ground becomes that arena. Arenas:";
                for (const auto& [id, a] : content.arenas)
                    out += "\n  " + id + "  " + a.name + "  " + std::to_string(a.size.x) + "x" + std::to_string(a.size.z);
                out += "\nthe run's:";
                for (const auto& pick : content.run.arenas)
                    out += " " + pick.arena + " (wave " + std::to_string(pick.from) + ")";
                if (not content.run.duel.arena.empty())
                    out += ", duel: " + content.run.duel.arena;
                return out;
            }
            if (client)
                return "map: the host's arena";
            if (world.inRun())
                return "map: outside a run (a run's arenas come with its waves)";

            std::string id = args[1] == "training" ? "" : resolveId(args[1], "base");
            if (not id.empty() and not content.arenas.count(id))
                return "no arena " + id + " (map: the list)";

            world.showArena(id);
            meshDirty = true;
            return id.empty() ? "the training arena" : content.arenas.at(id).name;
        }

        if (cmd == "join")
        {
            if (args.size() < 2)
                return "join <address[:port]>: join a friend's game (the title's J does it too)";
            if (networked())
                return "join: already in a game (leave first)";

            joinAddress = args[1];
            if (joinName.empty())
                joinName = options.playerName == "player" ? prefs.name : options.playerName;
            return joinNow() ? "" : "join: " + joinMessage;
        }

        if (cmd == "leave")
        {
            if (not networked())
                return "leave: not in a game with others";
            leaveGame();
            return "";
        }

        if (cmd == "duel")
        {
            const auto players = std::count_if(world.fighters.begin(), world.fighters.end(), [](const Fighter& f) { return f.team == Team::Player; });
            if (players < 2)
                return "duel: it takes two players or more";

            if (not world.inRun() or world.run.phase == RunPhase::Difficulty)
                startArena(false);
            world.beginDuel();
            return "the final duel";
        }

        if (cmd == "timeup")
        {
            if (world.run.phase == RunPhase::Duel)
            {
                world.run.warmup = 0.0f;
                world.run.timer = 0.0f;
                return "the duel's time is up";
            }

            if (world.run.phase != RunPhase::Fighting)
                return "timeup: only while a wave is on";

            // A boss wave ends with its boss
            if (Fighter* boss = world.run.bossId ? world.find(world.run.bossId) : nullptr)
            {
                boss->health = 0.01f;
                world.applyStatus(*boss, "base:burn");
            }

            world.run.timer = 0.0f;
            return "the wave's time is up";
        }

        if (cmd == "kill")
        {
            // A real death for each (it counts, and drops its shards in a run): burning, at its last breath
            int n = 0;
            for (auto& f : world.fighters)
                if (f.team == Team::Enemy and f.alive())
                {
                    f.health = 0.01f;
                    world.applyStatus(f, "base:burn");
                    ++n;
                }
            return std::to_string(n) + " enemies fall";
        }

        if (cmd == "hurt")
        {
            // A blow from you on every enemy, as any blow lands: a boss stops at its phases, a shield takes nothing
            if (not p or args.size() < 2)
                return "hurt <damage>: a blow from you on every enemy";

            AttackDef blow;
            blow.name = "hurt";
            blow.damage = std::max(0.0f, static_cast<float>(std::atof(args[1].c_str())));
            int n = 0;
            for (size_t i = 0; i < world.fighters.size(); ++i)
                if (world.fighters[i].team == Team::Enemy and world.fighters[i].alive())
                {
                    world.applyHit(*p, world.fighters[i], blow);
                    ++n;
                }
            return std::to_string(n) + " enemies hit";
        }

        if (cmd == "shards")
        {
            if (not p or args.size() < 2)
                return "shards <n>: that many more shards (a run's money, and XP)";

            world.collect(*p, std::max(0, std::atoi(args[1].c_str())));
            return std::to_string(p->shards) + " shards, level " + std::to_string(p->level);
        }

        if (cmd == "training")
        {
            if (client)
                return "training: only the host can do that";

            titleOpen = false;
            world.stopRun();
            return runCommand("reset");
        }

        if (cmd == "options")
        {
            setOptionsOpen(not optionsOpen);
            return "";
        }

        if (cmd == "spellbook")
        {
            setBookOpen(not bookOpen);
            return "";
        }

        if (cmd == "spells")
        {
            std::string out;
            for (size_t i = 0; i < book.equipped.size(); ++i)
            {
                const auto s = world.spellOf(book.equipped[i]);
                out += "[" + std::to_string(i + 1) + "] " + spellName(content, book, book.equipped[i]) + ": " + recipeText(content, book.equipped[i]) +
                       (s->valid() ? "  (" + fmt(s->mana, 0) + " mana)\n" : "  (" + s->error + ")\n");
            }

            out += "ready-made:";
            for (const auto& [id, s] : content.spells)
                out += " " + id;
            out += "\nkeywords:";
            for (const auto& [id, k] : content.keywords)
                out += " " + k.name;
            return out;
        }

        if (cmd == "spell")
        {
            if (args.size() < 3)
                return "spell <1-3> <element> <keywords...> (spell 1 fire bolt zone), or spell <1-3> <ready-made id> [level [branch]]";

            const int slot = std::atoi(args[1].c_str()) - 1;

            if (slot < 0 or slot >= static_cast<int>(book.equipped.size()))
                return "spell: the keys are 1 to " + std::to_string(book.equipped.size());

            SpellRecipe recipe;
            std::string error;

            const bool grown = args.size() >= 4 and args.size() <= 5 and content.spells.count(resolveId(args[2], "base")) and
                               std::all_of(args.begin() + 3, args.end(), [](const std::string& a) { return not a.empty() and (std::isdigit(static_cast<unsigned char>(a.back())) != 0); });

            if (grown)
            {
                Fighter::RunSpell rs;
                rs.id = resolveId(args[2], "base");
                rs.level = std::max(1, std::atoi(args[3].c_str()));
                rs.branch = args.size() > 4 ? std::atoi(args[4].c_str()) : -1;
                recipe = world.spellAt(rs, p);

                if (not p or client)
                    return "spell: a grown spell is only for a game alone or the host";
                std::vector<SpellRecipe> spells = p->spells;
                spells.resize(std::max(spells.size(), static_cast<size_t>(slot + 1)));
                spells[static_cast<size_t>(slot)] = recipe;
                world.setSpells(*p, spells);
                return "[" + args[1] + "] " + rs.id + " level " + std::to_string(rs.level) + (rs.branch >= 0 ? " branch " + std::to_string(rs.branch) : "") + ": " +
                       recipeText(content, recipe) + "  (" + fmt(world.spellOf(recipe)->mana, 0) + " mana)";
            }

            if (auto s = content.spells.find(resolveId(args[2], "base")); s != content.spells.end() and args.size() == 3)
            {
                recipe = s->second.recipe;
            }
            else if (not parseRecipe(content, std::vector<std::string>(args.begin() + 2, args.end()), recipe, error))
            {
                return "spell: " + error;
            }

            book.equipped[static_cast<size_t>(slot)] = recipe;
            bookChanged(true);
            return "[" + args[1] + "] " + recipeText(content, recipe) + "  (" + fmt(world.spellOf(recipe)->mana, 0) + " mana): " + describe(content, *world.spellOf(recipe));
        }

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
        if (client and (cmd == "reload" or cmd == "spawn" or cmd == "elite" or cmd == "clear" or cmd == "reset" or cmd == "place" or cmd == "fill" or
                        cmd == "god" or cmd == "stamina" or cmd == "mana" or cmd == "ai" or cmd == "tune" or cmd == "wave" or cmd == "teleport" or cmd == "pause" or
                        cmd == "step" or cmd == "slowmo" or cmd == "shards" or cmd == "timeup" or cmd == "kill" or cmd == "hurt" or cmd == "duel" or cmd == "item"))
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

        if (cmd == "elite")
        {
            // An elite in front of you with the affixes named (none: one picked as a run would)
            if (args.size() < 2)
                return "elite <enemy> [affix ...]; affixes: " + [&] { std::string s; for (const auto& [id, a] : content.affixes) s += id + " "; return s; }();

            const std::string id = resolveId(args[1], "base");
            if (not content.enemies.count(id))
                return "no enemy " + id;

            std::vector<std::string> affixes;
            for (size_t k = 2; k < args.size(); ++k)
            {
                const std::string affix = resolveId(args[k], "base");
                if (not content.affixes.count(affix))
                    return "no affix " + affix;
                affixes.push_back(affix);
            }
            if (affixes.empty())
                affixes = world.rollAffixes(1);

            const glm::vec3 ahead = p ? p->body.position + yawForward(camera.yaw) * 5.0f + glm::vec3(0, 1.0f, 0) : content.arena.playerSpawn;
            const int spawned = world.spawnEnemy(id, glm::clamp(ahead, glm::vec3(1.5f), glm::vec3(world.grid.width() - 1.5f, world.grid.height() - 2.0f, world.grid.depth() - 1.5f)),
                                                 camera.yaw + 180.0f);
            if (Fighter* e = world.find(spawned))
            {
                e->elite = true;
                e->affixes = affixes;
                e->maxHealth *= 2.5f * (1.0f + world.affixSum(*e, &AffixDef::health));
                e->health = e->maxHealth;
            }

            std::string named;
            for (const auto& a : affixes)
                named += " " + content.affixes.at(a).name;
            return "spawned an elite " + id + ":" + named;
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
            world.godMode = args.size() > 1 ? args[1] == "on" : not world.godMode;
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
            world.aiEnabled = args.size() > 1 ? args[1] == "on" : not world.aiEnabled;
            return std::string("AI ") + (world.aiEnabled ? "on" : "off");
        }

        if (cmd == "reach")
        {
            showReach = args.size() > 1 ? args[1] == "on" : not showReach;
            return std::string("reach rings ") + (showReach ? "shown" : "hidden");
        }

        if (cmd == "hitboxes")
        {
            showBoxes = args.size() > 1 ? args[1] == "on" : not showBoxes;
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
            for (const auto& m : content.modsOff)
                out += m.id + " (turned off)  " + m.dir + "\n";
            if (not serverMods)
                out += "your mods go in " + userModsDir() + "\n";
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

    // --------------------------------------------------------------------------------------------
    // The lab
    // --------------------------------------------------------------------------------------------

    std::string ArenaGame::labScriptPath() const
    {
        if (not options.labScript.empty())
            return options.labScript;

#ifdef VOXELARENA_LAB_DIR
        // From the source tree: an edited scene plays again as soon as it is saved, no rebuild
        std::error_code ec;
        if (std::filesystem::exists(std::string(VOXELARENA_LAB_DIR) + "/lab.pg", ec))
            return std::string(VOXELARENA_LAB_DIR) + "/lab.pg";
#endif

        return "lab/lab.pg";
    }

    void ArenaGame::setLabOpen(bool open)
    {
        if (open and networked())
            return;

        if (open and not lab)
        {
            lab = std::make_unique<Lab>();
            lab->load(labScriptPath(), content, tuning);
        }

        if (open == labOpen)
            return;

        labOpen = open;

        if (open)
        {
            // Levels and branches count outside a run from now on: the lab grows what it shows
            world.growsOutsideRun = true;
            titleOpen = false;
            if (firstPerson)
                setFirstPerson(false);
            labShow();
        }
        else
        {
            labOrbit = 0.0f;
            labCaption.clear();
            labPlaying = false;
        }
    }

    void ArenaGame::labShow()
    {
        if (not lab or not labOpen)
            return;

        // What the last scene still had to do goes
        labQueue.clear();
        labWaitSteps = 0;
        labSleepUntil = std::chrono::steady_clock::now();
        injectedHolds.clear();
        injectedPresses.clear();
        labCaption.clear();
        labOrbit = 0.0f;

        // Another tab: its scenes place the camera again
        if (lab->kindShown() != labShownKind)
        {
            labOwnCamera = false;
            labShownKind = lab->kindShown();
        }

        // The stage, then the scene of the pick
        std::vector<std::string> lines = lab->stage();
        for (auto& l : lab->show())
            lines.push_back(std::move(l));

        for (auto& l : lines)
            labQueue.push_back(std::move(l));

        labPlaying = not lines.empty();
        labIdle = 0.0f;
    }

    void ArenaGame::labTick(float frameSeconds)
    {
        if (not lab)
            return;

        if (lab->scriptChanged(frameSeconds))
        {
            lab->load(labScriptPath(), content, tuning);
            print(lab->status);
            if (labOpen)
                labShow();
        }

        if (not labOpen)
            return;

        if (labPlaying and labQueueIdle())
        {
            labPlaying = false;
            labIdle = 0.0f;
        }

        // Loop: the scene again a moment after it ends
        if (not labPlaying)
        {
            labIdle += frameSeconds;
            if (lab->loop and labIdle > 1.0f)
                labShow();
        }
    }
}
