#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
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

#include "gpumesh.h"
#include "hud.h"

namespace pg
{
    class Input;
    class Window;
}

namespace arena
{
    struct ArenaOptions
    {
        std::string modsDir;
        std::vector<std::string> startupCommands;
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
        bool firstPerson = false;          // V switches; see setFirstPerson
        void setFirstPerson(bool on);
        float fov() const { return firstPerson ? tuning.firstPerson.fov : tuning.camera.fov; }

        // Rendering
        size_t litMaterial = 0, skyMaterial = 0, debugMaterial = 0;
        std::shared_ptr<GpuMesh> worldMesh, skyMesh, boxMesh;
        std::map<std::string, std::shared_ptr<GpuMesh>> modelMeshes;   // kept for the whole run, see GpuMesh
        uint64_t meshedRevision = ~0ull;
        bool meshDirty = true;
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
        bool onWSL = false;
        int warpPendingX = -1, warpPendingY = -1;

        // Debug
        bool showOverlay = true;
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

        Hud hud;
    };
}
