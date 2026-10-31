#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "ECS/system.h"
#include "ECS/entitysystem_fwd.h"
#include "Input/inputcomponent.h"
#include "Input/sdlevents.h"
#include "Renderer/renderer.h"
#include "Systems/coresystems.h"

#include "Core/content.h"
#include "Core/orbitcamera.h"
#include "Core/scriptbrain.h"
#include "Core/tuning.h"
#include "Core/world.h"

#include "Core/effects.h"
#include "spellbook.h"
#include "options.h"
#include "shop.h"

#include "gpumesh.h"
#include "hud.h"
#include "screens.h"
#include "ui.h"
#include "particles.h"
#include "Net/portmapper.h"
#include "soundbank.h"

namespace pg
{
    class Input;
    class Window;
    struct NetworkSystem;
    class SdlNetworkBackend;
}

namespace arena::net
{
    class EngineTransport;
    class NetServer;
    class NetClient;
}

namespace arena
{
    struct ArenaOptions
    {
        std::string modsDir;
        std::vector<std::string> startupCommands;

        /** Networking: single player, hosting (a listen server) or joining a server. */
        enum class Mode { Single, Host, Client };
        Mode mode = Mode::Single;
        pg::NetworkSystem* network = nullptr;   // the engine's, for Host and Client
        std::string playerName = "player";
        std::string address;                    // a client: where it joined
        uint16_t port = 7777;                   // TCP; UDP is the next one
        bool upnp = true;                       // a host opens its ports on the router
    };

    /** A key or a mouse button an action is bound to (from the "controls" section of the tuning). */
    struct Binding
    {
        int scancode = -1;
        int mouseButton = -1;
    };

    /**
     * The game: owns the content, the simulation and the camera, turns input into
     * simulation steps (60 per second) and draws the result.
     *
     * One system on purpose: the simulation, the camera and the render calls must see the
     * same state, and the ECS may run separate systems at the same time.
     */
    class ArenaGame : public pg::AbstractRenderer,
                      public pg::System<pg::InitSys,
                                        pg::Listener<pg::OnSDLScanCode>, pg::Listener<pg::OnSDLScanCodeReleased>,
                                        pg::Listener<pg::OnSDLTextInput>, pg::Listener<pg::OnSDLMouseMotion>,
                                        pg::Listener<pg::OnSDLMouseWheel>, pg::Listener<pg::OnMouseClick>,
                                        pg::Listener<pg::ResizeEvent>>
    {
    public:
        ArenaGame(pg::MasterRenderer* masterRenderer, pg::Window* window, const ArenaOptions& options);
        ~ArenaGame() override;

        std::string getSystemName() const override { return "Arena Game"; }

        void init() override;
        void execute() override;

        void onEvent(const pg::OnSDLScanCode& event) override;
        void onEvent(const pg::OnSDLScanCodeReleased& event) override;
        void onEvent(const pg::OnSDLTextInput& event) override;
        void onEvent(const pg::OnSDLMouseMotion& event) override;
        void onEvent(const pg::OnSDLMouseWheel& event) override;
        void onEvent(const pg::OnMouseClick& event) override;
        void onEvent(const pg::ResizeEvent& event) override;

        /** Runs one console command and returns what it prints. */
        std::string runCommand(const std::string& line);

    private:
        // ---- Content ----
        void loadAll(bool first);
        void readControls();
        std::string contentSummary() const;

        // ---- Input ----
        bool held(const std::string& action) const;
        bool pressed(const std::string& action) const;   // since the last step
        bool keyIs(int scancode, const std::string& action) const;
        void updateCursorLock();

        // ---- Frame ----
        void simulate(float frameSeconds);
        void feedback();
        void updateCamera(float frameSeconds);
        void buildRenderCalls();
        void remeshIfNeeded();
        std::shared_ptr<GpuMesh> modelMesh(const std::string& modelId);

        void consoleKey(int scancode);
        void print(const std::string& line);

        // Engine
        pg::Window* window = nullptr;
        pg::Input* input = nullptr;
        ArenaOptions options;

        // Game
        Content content;
        Tuning tuning;
        ArenaWorld world;
        ScriptBrain brain;
        OrbitCamera camera;
        bool firstPerson = true;           // the default view; F (or V) switches, see setFirstPerson
        void setFirstPerson(bool on);
        float fov() const { return (firstPerson ? tuning.firstPerson.fov : tuning.camera.fov) + prefs.fovOffset; }

        // Rendering
        size_t litMaterial = 0, skyMaterial = 0, debugMaterial = 0;
        std::shared_ptr<GpuMesh> worldMesh, skyMesh, boxMesh;
        std::map<std::string, std::shared_ptr<GpuMesh>> modelMeshes;   // kept for the whole run, see GpuMesh
        uint64_t meshedRevision = ~0ull;
        uint64_t meshedTerrainRevision = ~0ull;

        // Falling pieces of the arena: a mesh each while they fall, taken from and given back to a pool
        std::map<int, std::shared_ptr<GpuMesh>> pieceMeshes;
        std::vector<std::shared_ptr<GpuMesh>> freePieceMeshes;
        void updatePieceMeshes();

        /** The block the camera points at, within reach of the player's abilities. */
        bool aimed(RayHit& hit) const;
        std::string keyName(const std::string& action) const;   // the first key bound to an action, for the HUD
        bool meshDirty = true;
        int meshedArenaBuilds = -1;   // the world's arena builds already meshed (a new grid starts its revisions over)
        float aspect = 16.0f / 9.0f;
        float screenW = 1280.0f, screenH = 720.0f;

        // Positions before the last step, to draw between steps
        struct Pose
        {
            glm::vec3 position;
            float yaw;
            float stepOffset;
        };
        std::map<int, Pose> previous;
        float alpha = 1.0f;

        // Time
        std::chrono::steady_clock::time_point lastFrame;
        bool clockStarted = false;
        int64_t lastFrameStartUs = 0;   // the frame clock of the ECS pass pacer, when it runs (see execute)
        double accumulator = 0.0;
        float timeScale = 1.0f;
        bool paused = false;
        int stepRequests = 0;
        float frameSecondsSmoothed = 1.0f / 60.0f;
        float simTime = 0.0f;

        // Input, written by the event listeners and read by execute()
        mutable std::mutex inputMutex;
        std::set<int> keysDown;
        struct KeyPress { int scancode; bool repeat; uint16_t mod; };
        std::vector<KeyPress> keyPresses;   // since the last frame, in order
        std::set<int> stepPresses;          // first presses not yet handed to a simulation step
        std::set<int> mousePresses;         // not yet handed to a simulation step
        float lookDx = 0.0f, lookDy = 0.0f;
        float wheel = 0.0f;
        std::string typed;

        std::map<std::string, std::vector<Binding>> controls;

        bool cursorLocked = false;
        bool cursorFree = false;        // Escape toggles it
        bool recapture = false;
        bool mouseLook = true;          // "mouse off": scripted runs ignore the real mouse         // a click while freed takes the mouse back
        int warpPendingX = -1, warpPendingY = -1;

        // Debug
        bool showOverlay = false;   // F1
        std::string overlayText;    // its text this frame
        bool showBoxes = false;
        bool consoleOpen = false;
        std::string consoleLine;
        std::deque<std::string> consoleLog;
        std::vector<std::string> history;
        int historyIndex = -1;

        // Scripted input, for tests and for frame data checks: "hold forward 30; wait 30; press light"
        std::deque<std::string> commandQueue;
        int waitSteps = 0;
        std::chrono::steady_clock::time_point sleepUntil;
        std::set<std::string> injectedPresses;
        std::map<std::string, int> injectedHolds;   // action -> steps left
        void runQueue();

        // Script cost over the last second, for the overlay
        struct
        {
            uint64_t calls = 0;
            double seconds = 0.0;
            float timer = 0.0f;
            std::string line;
        } scriptStats;

        int playerDeaths = 0;
        int enemyKills = 0;
        std::chrono::steady_clock::time_point lastReload;

        // The 2D layer: drawn anew each frame (see drawUi), clicked through its last frame's regions
        Ui ui;
        Hud hud;
        std::vector<glm::vec2> uiClicks;     // clicks on a screen (input thread -> game)
        float mouseX = -1.0f, mouseY = -1.0f;
        float uiTime = 0.0f;
        bool endOpen = false;               // the end of a run is up
        bool armoryOpen = false;            // the weapon choice before the first wave is up
        void drawArmory();
        bool uiTakesClicks() const;         // a screen is up that the mouse clicks on
        void handleUiClicks();
        void drawUi(float frameSeconds);
        void fillHud(Hud::View& v);
        void playerHud(Hud::View& v, const Fighter& p);
        void enemyHud(Hud::View& v);
        std::string statusName(const std::string& id) const;
        /** A player's name: the lobby's in multiplayer, "" when not known. */
        std::string playerNameOf(int id) const;
        std::vector<Hud::Status> statusesOf(const Fighter& f) const;

        // Networking (see ArenaOptions::mode)
        std::unique_ptr<net::EngineTransport> transport;
        std::unique_ptr<net::NetServer> server;
        std::unique_ptr<net::NetClient> client;
        std::chrono::steady_clock::time_point netEpoch = std::chrono::steady_clock::now();
        double netNowMs() const { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - netEpoch).count(); }
        bool networked() const { return options.mode != ArenaOptions::Mode::Single; }

        // The lobby: what a host shows its friends to join with, and what happened lately
        net::PortMapper portMapper;
        std::string lanAddress;
        std::deque<std::string> netLog;
        RunPhase clientPhase = RunPhase::Off;
        RunPhase duelSeen = RunPhase::Off;      // the run's phase last frame, for the duel's start   // a client: the run's phase last frame (its end brings the lobby back)
        void drawLobby();

        // Inviting: the + of the title and the lobby. Alone, it hosts this game first (hostNow)
        bool inviteOpen = false;
        std::string inviteMessage;
        std::unique_ptr<pg::NetworkSystem> ownNetwork;   // hosting started from the title: stepped here, not by the engine
        pg::SdlNetworkBackend* ownBackend = nullptr;
        bool hostNow();

        // Joining a friend's game from the title (J): this game becomes a client of theirs, and Leave
        // (or Stop hosting) makes it a game alone again
        bool joinOpen = false;
        std::string joinAddress, joinName, joinMessage;
        int joinFocus = 0;
        bool joinNow();
        void leaveGame();

        /** Reads the network as a client, and takes on the server's mods if they came. */
        void pollClient();
        void applyServerMods();
        void restoreOwnMods();

        // On a server's mods: where this game's own are. Their scripts stay unloaded: only a server
        // runs scripts, and what a server sends is not run here
        bool serverMods = false;
        std::string ownModsDir;
        void openJoin();
        void joinAction(int action);
        void resetLocalState();
        void drawInvite();
        void inviteAction(int action);
        int pendingEquip = 0;   // weapon key pressed, handed to the next step's input
        std::string netStatus() const;

        // Effects: what world events sound and look like
        std::map<std::string, EffectDef> effects;
        SoundBank sounds;
        Particles particles;
        void playEffect(const std::string& kind, const glm::vec3& at, const glm::vec3& color);
        void playSound(const std::string& sound, const glm::vec3& at, float distance);   // panned from where the camera looks
        void playEvent(const WorldEvent& e);   // playEffect, and what an event starts (flames) or draws along (arcs)

        // Looks that last: statuses on fighters (tint, particles now and then) and blocks on fire
        void stepLooks(float frameSeconds);
        const EffectDef* statusLook(const Fighter& f) const;
        bool heldStill(const Fighter& f) const;   // a status that stops the body (frozen solid)
        std::map<std::pair<int, std::string>, float> statusTimers;
        struct BurningLook
        {
            glm::ivec3 cell;
            float left = 0.0f;
            float timer = 0.0f;
        };
        std::vector<BurningLook> burningLooks;
        std::mt19937 lookRng{7};

        float hurtFlash = 0.0f;   // the screen's red wash when the player is hit, fading

        // The spells: the book (what is on the keys, what is saved) and its screen (B)
        SpellbookData book;
        Spellbook spellbook;
        bool bookOpen = false;
        void setBookOpen(bool open);
        void applySpells();                  // the book's spells onto the player (or to the server)
        void bookChanged(bool spellsChanged);

        // The options (O): kept between runs
        GameOptions prefs;
        OptionsScreen optionsScreen;
        bool optionsOpen = false;
        void setOptionsOpen(bool open);
        void applyOptions();

        // The shop between a run's waves: open while the run is in it
        ShopScreen shopScreen;
        bool shopOpen = false;
        void updateShop();

        // The arena run: the title screen, a pick waiting for the next step, the best score
        bool titleOpen = false;
        int pendingPick = 0;
        RunPhase lastPhase = RunPhase::Off;
        int bestScore = 0;
        void startArena();
        void runHud(Hud::View& v);
        struct SpawnMark
        {
            glm::vec3 position;
            float left = 0.0f;
            bool boss = false;
            bool elite = false;
        };
        std::vector<SpawnMark> spawnMarks;
        // Enemies just come in: pointed at from the screen's edge for a moment while out of view
        struct SpawnPing
        {
            int fighter = 0;
            float left = 0.0f;
        };
        std::vector<SpawnPing> spawnPings;
        int headlineWave = 0;     // the wave whose name was put up, and when (simTime)
        float waveStartedAt = 0.0f;
        void edgeArrows(Hud::View& v, const glm::mat4& proj, const glm::mat4& view);

        // Limbs: a walk cycle per fighter
        struct Gait
        {
            float phase = 0.0f;
            float amount = 0.0f;   // 0 standing .. 1 walking
        };
        std::map<int, Gait> gaits;
        void stepGaits(float frameSeconds);
        glm::mat4 partPose(const std::string& part, const Fighter& f, const Gait& g, float frame) const;
    };
}
