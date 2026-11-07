#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <tuple>
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
#include "Core/progress.h"
#include "spellbook.h"
#include "options.h"
#include "shop.h"

#include "gpumesh.h"
#include "hud.h"
#include "lab.h"
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
        ModSources mods;   // the game's mods folder, the player's, and which are turned off
        std::vector<std::string> startupCommands;

        /** Networking: single player, hosting (a listen server) or joining a server. */
        enum class Mode { Single, Host, Client };
        Mode mode = Mode::Single;
        pg::NetworkSystem* network = nullptr;   // the engine's, for Host and Client
        std::string playerName = "player";
        std::string address;                    // a client: where it joined
        uint16_t port = 7777;                   // TCP; UDP is the next one
        bool upnp = true;                       // a host opens its ports on the router
        std::string upnpPorts;                  // the outside ports to try there ("" : the game's, then 50000, then 45000-45200)

        /** The lab (--lab): the panel that shows the content, played by a pgscript ("": tools/lab/lab.pg of the source tree). */
        bool lab = false;
        std::string labScript;
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
                                        pg::Listener<pg::ResizeEvent>, pg::Listener<pg::OnSDLGamepadPressed>,
                                        pg::Listener<pg::OnSDLGamepadReleased>, pg::Listener<pg::OnSDLGamepadAxisChanged>>
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
        void onEvent(const pg::OnSDLGamepadPressed& event) override;
        void onEvent(const pg::OnSDLGamepadReleased& event) override;
        void onEvent(const pg::OnSDLGamepadAxisChanged& event) override;

        /** Runs one console command and returns what it prints. */
        std::string runCommand(const std::string& line);

    private:
        // ---- Content ----
        void loadAll(bool first);
        void loadBlockLooks();
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
        std::shared_ptr<GpuMesh> worldMesh, skyMesh, boxMesh, discMesh, ringMesh;
        std::map<std::string, std::shared_ptr<GpuMesh>> modelMeshes;   // kept for the whole run, see GpuMesh
        uint64_t meshedRevision = ~0ull;
        uint64_t meshedTerrainRevision = ~0ull;

        // Blocks: their textures, and the geometry of the ones drawn as a model (by BlockId)
        std::shared_ptr<GpuTextureArray> blockTextures;
        std::vector<MeshData> blockModels;
        MeshOptions blockMeshOptions() const;

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

        // A controller (the tuning's "gamepad" section): its buttons by action, as the keys are. The triggers count as
        // buttons, pressed past half way. Under inputMutex, like the keys
        static constexpr int PAD_LEFT_TRIGGER = 100, PAD_RIGHT_TRIGGER = 101;
        std::map<std::string, std::vector<int>> padControls;   // action -> SDL_GameControllerButton (or a trigger)
        std::set<int> padDown;                  // held
        std::set<int> padStepPresses;           // first presses not yet handed to a simulation step
        std::vector<int> padFramePresses;       // first presses since the last frame (the screens' keys)
        float padAxis[6] = {};                  // SDL_GameControllerAxis, -1..1 (triggers 0..1)
        int padId = -1;                         // the controller last used (for its rumble)
        bool padActive = false;                 // the controller was used last (not the keyboard or the mouse): its
                                                // buttons on the HUD, a cursor on the screens
        // The screens with a controller: the button picked (its action, where it is), moved along by the d-pad or the stick
        int padFocusAction = 0;
        glm::vec2 padFocusAt = glm::vec2(0.0f);
        glm::vec4 padFocusRect = glm::vec4(0.0f);   // window pixels: x0, y0, x1, y1
        glm::vec2 padNavDir = glm::vec2(0.0f);       // the direction held, and when it moves on again
        float padNavTimer = 0.0f;
        bool padOnScreen = false;               // a screen took the controller last frame (a new one starts on its first button)
        // The controls screen's Controller tab: its rows, and an action waiting for its new button (the next one
        // pressed, under inputMutex; Start cancels)
        bool controlsPad = false;
        std::string padRebinding;
        int padCaptured = -1;
        static const std::vector<std::pair<std::string, std::string>>& padRows();
        void bindPad(const std::string& action, int button);
        static std::string padButtonName(int button);
        bool padHeld(const std::string& action) const;
        bool padPressed(const std::string& action) const;
        /** A stick, its dead zone taken out (0 inside, then rising to 1): x right, y down as SDL has it. */
        glm::vec2 stick(int axisX, int axisY) const;
        void padFrame(float frameSeconds);      // the cursor on the screens, and the presses that are keys there
        void rumble(float strength, int ms);

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
        bool difficultyOpen = false;        // the difficulty choice as a run starts is up
        int difficultyCursor = 0;           // its card under the arrow keys
        /** The levels that may be picked: this game's progress, or the server's (a client). */
        int levelOpenNow() const { return client ? std::max(1, world.run.levelsOpen) : levelOpen + 1; }
        void drawDifficulty();

        // The controls screen (from the options): an action rebound by the next key or button pressed
        bool controlsOpen = false;
        std::string rebinding;              // the action waiting for its new key ("" when none)
        std::atomic<bool> capturing{false}; // the input thread hands the next mouse button to it
        int rebindMouse = -1;               // that button (under inputMutex)
        std::string controlsMessage;
        static const std::vector<std::pair<std::string, std::string>>& controlRows();
        void bindKey(const std::string& action, const std::string& key);
        void drawControls();
        std::string keyLabel(int scancode, int mouseButton) const;
        void drawArmory();
        bool armoryBack = false;             // the first spell's step, back to the weapons
        int armoryHover = -1;                // the weapon card under the mouse, and since when (its tooltip)
        std::chrono::steady_clock::time_point armoryHoverSince;
        bool armoryChoosingSpell() const;
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
        /** Nobody else in the game (alone, or hosting with nobody connected): the menus pause it. */
        bool alone() const;

        // The name the others see (the options' field, the join panel): kept, and told to the others
        void rename(const std::string& name);

        // Quitting: the first click arms it, a second within a few seconds quits
        float quitArmedUntil = -1.0f;
        bool quitArmed() const { return uiTime < quitArmedUntil; }
        void quitClicked();

        // Back to the title from playing (the options' To title): leaving a run asks twice
        float titleArmedUntil = -1.0f;
        bool titleArmed() const { return uiTime < titleArmedUntil; }
        void toTitleClicked();

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
        std::string hostPortEdit;               // the panel's port field; "": the game looks for one
        bool manualPort = false;                // hosting on the port the player chose
        uint16_t autoPort = 7777;               // the game's port when the player chose none
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
        void pollServer();
        void applyServerMods();
        void restoreOwnMods();

        // On a server's mods: where this game's own are. Their scripts stay unloaded: only a server
        // runs scripts, and what a server sends is not run here
        bool serverMods = false;
        ModSources ownMods;
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
        void playEffect(const std::string& kind, const glm::vec3& at, const glm::vec3& color, bool silent = false);
        void playSound(const std::string& sound, const glm::vec3& at, float distance);   // panned from where the camera looks
        void playEvent(const WorldEvent& e);   // playEffect, and what an event starts (flames) or draws along (arcs)

        // Looks that last: statuses on fighters (tint, particles now and then) and blocks on fire
        void stepLooks(float frameSeconds);
        const EffectDef* statusLook(const Fighter& f) const;
        bool heldStill(const Fighter& f) const;   // a status that stops the body (frozen solid)
        std::map<std::pair<int, std::string>, float> statusTimers;

        // Points the frame drew that shed particles (a weapon's blade with its branch's element, a Legendary item):
        // gathered by buildRenderCalls, thrown by stepLooks with the effect named, each with its own timer
        struct LookPoint
        {
            int fighter = 0;
            std::string effect;          // effects.json ("weapon:fire", "rarity:legendary")
            int slot = 0;                // which point of that fighter's (a timer each)
            glm::vec3 at = glm::vec3(0.0f);
            float rate = 1.0f;           // times the effect's own rate (a bow sheds less)
        };
        std::vector<LookPoint> lookPoints;

        // A blast's shockwave: a ring sweeping out over the ground to its radius, a flash at its heart, fading
        struct Shockwave
        {
            glm::vec3 at = glm::vec3(0.0f);   // on the ground
            float radius = 1.0f;
            float age = 0.0f;
        };
        std::vector<Shockwave> shockwaves;
        std::map<std::tuple<int, std::string, int>, float> lookTimers;
        /** An effect's tint (its glow), or none. */
        const glm::vec3* effectTint(const std::string& effect) const;

        // The gems' shapes: a UI icon made a voxel badge (LookDef::icon), a mesh each, kept like the models
        std::map<std::string, std::shared_ptr<GpuMesh>> iconMeshes;
        std::shared_ptr<GpuMesh> iconMesh(const std::string& icon);

        // The dummies' damage meter: what they took since the first hit of the latest burst of hits
        struct DamageMeter
        {
            float total = 0.0f;
            float first = 0.0f, last = -100.0f;   // simTime of its first and latest damage
            float health = -1.0f;                 // the health it had at the step before
        };
        std::map<int, DamageMeter> meters;
        void stepMeters();

        // Reach rings (reach): each attack's furthest reach on the ground round the fighter, while it winds up and strikes
        bool showReach = false;
        struct BurningLook
        {
            glm::ivec3 cell;
            float left = 0.0f;
            float timer = 0.0f;
        };
        std::vector<BurningLook> burningLooks;
        // A beam's bolt (effects.json "beam"), fading. fromHand: the player's own, in first person: it leaves from the
        // weapon hand in front of the eye (the world's start is the chest: seen end on from the eye, it would not show)
        struct StrikeLook
        {
            glm::vec3 from, to;
            BeamLook look;
            float age = 0.0f;
            uint32_t seed = 0;
            bool fromHand = false;
        };
        std::vector<StrikeLook> strikes;
        /** Where a beam of the player's leaves from in first person: low and to the right, in front of the eye. */
        glm::vec3 handPoint() const;
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
        /** A run: its difficulty chosen first (chooseLevel), or straight in at the level picked last. */
        void startArena(bool chooseLevel = true);

        // The difficulty levels: the highest open (each opens once the one before is won), the one picked
        int levelOpen = 0, levelPicked = 0;
        std::string levelJustOpened;       // a win just opened it: the end screen says so
        void pickLevel(int level);
        void saveLevels() const;
        std::vector<std::string> levelLines(int level) const;   // what a level changes, in words, a line each
        std::string arenaLine(int level) const;   // "10 timed waves, a shop between them, a boss at the end"
        void runHud(Hud::View& v);

        // The achievements (achievements.json): this player's progress, what it still locks, their screen
        Progress progress;
        std::set<std::string> lockedNow;
        bool runCounted = true;                   // this run is in the progress already (or there is no run)
        std::vector<std::string> goalsJustReached;   // by the run that just ended: its end screen says so
        bool achievementsOpen = false;
        int achievementsPage = 0;
        void refreshLocks();
        /** The run so far goes into the progress (once): at its end, or when leaving it. */
        void countThisRun(bool won);
        /** A weapon's, a spell's, a technique's or an item's name, or a branch's ("Sword: Riposte"). */
        std::string unlockName(const std::string& id) const;
        std::string unlocksLine(const AchievementDef& a) const;   // "Inferno, Ember ring"
        /** What unlocks a locked id, in words: the first goal not reached ("Deal 5 000 burn damage"). */
        std::string lockedBecause(const std::string& id) const;
        void drawAchievements();
        /** A recap's line for what dealt damage (RunStats::bySource): its name as the player knows it, its colour. */
        EndView::Source damageSource(const std::string& id, int damage) const;
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
        // The damage the player's hits do: a number rising off what it hit (the option damageNumbers)
        struct DamageNumber
        {
            int target = 0;
            glm::vec3 at = glm::vec3(0.0f);
            float value = 0.0f;
            float age = 0.0f;
            bool crit = false, kill = false;
        };
        std::vector<DamageNumber> damageNumbers;
        int critOn = 0;           // the target of the crit event just played: its hit (the next event) was a crit
        int headlineWave = 0;     // the wave whose name was put up, and when (simTime)
        float waveStartedAt = 0.0f;
        void edgeArrows(Hud::View& v, const glm::mat4& proj, const glm::mat4& view);

        // The lab (see Lab): its panel takes the keys and the mouse, its script plays the scenes through the command queue
        std::unique_ptr<Lab> lab;
        bool labOpen = false;
        float labOrbit = 0.0f;              // degrees a second the scene turns the camera round the player (orbit)
        // The Orbit button: the scene decides, until the player turns it off (a drag does too) or on; then it stays so
        enum class LabOrbitChoice { Scene, Off, On } labOrbitChoice = LabOrbitChoice::Scene;
        float labTurn() const
        {
            return labOrbitChoice == LabOrbitChoice::Off ? 0.0f : (labOrbitChoice == LabOrbitChoice::On and labOrbit == 0.0f ? 30.0f : labOrbit);
        }
        bool labOwnCamera = false;          // the player dragged or zoomed: the scenes of this tab leave the camera where it is
        int labShownKind = -1;
        float labFaceYaw = 0.0f;            // where the player's attacks and spells go while the camera turns (face)
        float labAimPitch = 8.0f;           // ... and how far down they aim, so a spell lands in front of the dummies
        std::string labCaption;             // what the scene shows now (caption)
        float labIdle = 0.0f;               // s the scene has been over (Loop plays it again)
        bool labPlaying = false;
        enum class LabDrag { None, OnPanel, Turning } labDrag = LabDrag::None;   // a left drag over the arena turns the camera
        float labDragX = 0.0f, labDragY = 0.0f;
        std::string labScriptPath() const;
        void setLabOpen(bool open);
        void labShow();                     // the pick, played from the start: the stage, then the scene
        void labTick(float frameSeconds);
        // The scene's commands, beside the console's queue (typed, --exec) with waits of their own
        std::deque<std::string> labQueue;
        int labWaitSteps = 0;
        std::chrono::steady_clock::time_point labSleepUntil;
        bool runningLab = false;            // a command of labQueue is running: its wait and sleep are the lab's
        bool labQueueIdle() const { return labQueue.empty() and labWaitSteps <= 0 and std::chrono::steady_clock::now() >= labSleepUntil; }

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
