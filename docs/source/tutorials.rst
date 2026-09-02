Tutorial: Build Breakout
========================

In this tutorial you build a complete Breakout game — paddle, ball, bricks, score — in about 150 lines of C++. Along the way you meet the core engine concepts: the ``pg::Engine`` entry point, ECS systems, tick updates, keyboard input, entity creation and deletion, text, and sound.

Everything here uses APIs exactly as the bundled examples do. If you get stuck, ``examples/SimpleBoxBouncer`` is the closest working reference.

Setup
-----

Start from the quickstart template (engine installed via the install script — see :doc:`getting_started`):

.. code-block:: bash

    cp -r templates/quickstart ~/breakout
    cd ~/breakout
    ./build.sh          # verify the bouncing-box template runs first
    cd build && ./ColumbaEngineTestApp

Once the bouncing box works, replace ``src/boxbouncersystem.h`` with a new file ``src/breakoutsystem.h`` and update the include and ``createSystem`` call in ``src/application.cpp``:

.. code-block:: cpp

    #include "breakoutsystem.h"

    GameApp::GameApp(const std::string &appName) : engine(appName)
    {
        engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
        {
            auto config = engine.getConfig();
            ecs.createSystem<BreakoutSystem>(config.width, config.height);
        });
    }

``pg::Engine`` hides all SDL and platform boilerplate (on the web build it even manages the Emscripten main loop) — ``main.cpp`` stays a dozen lines forever.

Step 1 — The paddle
-------------------

A system is a class inheriting ``System<...traits...>``. ``InitSys`` gives you an ``init()`` hook; ``Listener<TickEvent>`` delivers frame ticks. Entities are created through factory helpers — ``makeSimple2DShape`` returns a component list whose ``.entity`` you can keep as a handle.

.. code-block:: cpp

    // src/breakoutsystem.h
    #pragma once

    #include <vector>

    #include "Systems/basicsystems.h"
    #include "2D/simple2dobject.h"
    #include "Input/sdlevents.h"

    using namespace pg;

    class BreakoutSystem : public System<InitSys, Listener<TickEvent>,
                                         Listener<OnSDLScanCode>, Listener<OnSDLScanCodeReleased>>
    {
    public:
        BreakoutSystem(float width, float height) : screenWidth(width), screenHeight(height) {}

        virtual std::string getSystemName() const override { return "Breakout System"; }

        void init() override
        {
            auto paddleShape = makeSimple2DShape(ecsRef, Shape2D::Square,
                screenWidth / 2 - 60, screenHeight - 40,
                {220.0f, 220.0f, 220.0f, 255.0f});

            auto pos = paddleShape.get<PositionComponent>();
            pos->width = 120.0f;
            pos->height = 16.0f;

            paddle = paddleShape.entity;
        }

    private:
        float screenWidth, screenHeight;
        EntityRef paddle;
    };

Build and run: a white paddle sits at the bottom of the window.

Step 2 — Moving the paddle
--------------------------

Key events arrive as ``OnSDLScanCode`` (press) and ``OnSDLScanCodeReleased`` (release), carrying an ``SDL_Scancode``. For held-key movement, track pressed state and apply it every tick:

.. code-block:: cpp

    // add to the class:
    virtual void onEvent(const OnSDLScanCode& event) override
    {
        if (event.key == SDL_SCANCODE_LEFT)  movingLeft = true;
        if (event.key == SDL_SCANCODE_RIGHT) movingRight = true;
    }

    virtual void onEvent(const OnSDLScanCodeReleased& event) override
    {
        if (event.key == SDL_SCANCODE_LEFT)  movingLeft = false;
        if (event.key == SDL_SCANCODE_RIGHT) movingRight = false;
    }

    virtual void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick / 1000.0f;
    }

    void execute() override
    {
        if (deltaTime == 0.0f)
            return;

        auto pos = paddle->get<PositionComponent>();

        if (movingLeft)  pos->setX(std::max(0.0f, pos->x - paddleSpeed * deltaTime));
        if (movingRight) pos->setX(std::min(screenWidth - pos->width, pos->x + paddleSpeed * deltaTime));

        deltaTime = 0.0f;
    }

    // members:
    bool movingLeft = false, movingRight = false;
    float paddleSpeed = 420.0f;
    float deltaTime = 0.0f;

The tick/execute split matters: events can arrive on other threads, so you accumulate in ``onEvent`` and mutate the world in ``execute()``, exactly like ``SimpleBoxBouncer`` does.

Step 3 — The ball
-----------------

Create the ball in ``init()`` and bounce it off walls and the paddle in ``execute()``. An AABB overlap test is four comparisons — collision *response* deliberately lives in your game logic in this engine (the built-in ``CollisionSystem`` gives you broad-phase, layers, and raycasts when a game outgrows this; see ``examples/Asteroid``).

.. code-block:: cpp

    // in init():
    auto ballShape = makeSimple2DShape(ecsRef, Shape2D::Square,
        screenWidth / 2, screenHeight / 2, {255.0f, 200.0f, 80.0f, 255.0f});
    ballShape.get<PositionComponent>()->width = 14.0f;
    ballShape.get<PositionComponent>()->height = 14.0f;
    ball = ballShape.entity;

    // helper:
    static bool overlaps(PositionComponent* a, PositionComponent* b)
    {
        return a->x < b->x + b->width  and a->x + a->width  > b->x
           and a->y < b->y + b->height and a->y + a->height > b->y;
    }

    // in execute(), after the paddle movement:
    auto bpos = ball->get<PositionComponent>();

    float x = bpos->x + ballVelX * deltaTime;
    float y = bpos->y + ballVelY * deltaTime;

    if (x <= 0 or x + bpos->width >= screenWidth)  ballVelX = -ballVelX;
    if (y <= 0)                                    ballVelY = -ballVelY;

    bpos->setX(x);
    bpos->setY(y);

    if (overlaps(bpos, pos) and ballVelY > 0)
        ballVelY = -ballVelY;

    if (y > screenHeight)   // missed: reset
    {
        bpos->setX(screenWidth / 2);
        bpos->setY(screenHeight / 2);
    }

    // members:
    EntityRef ball;
    float ballVelX = 240.0f, ballVelY = -240.0f;

Step 4 — Bricks
---------------

A grid of colored shapes, each kept as an ``EntityRef``. On hit, delete the entity with ``ecsRef->removeEntity(...)`` — the same call the bundled games use (see ``examples/InvadersBreaker/powerups.h``).

.. code-block:: cpp

    // in init():
    for (int row = 0; row < 5; ++row)
    {
        for (int col = 0; col < 8; ++col)
        {
            auto brick = makeSimple2DShape(ecsRef, Shape2D::Square,
                20.0f + col * 98.0f, 60.0f + row * 34.0f,
                {60.0f + row * 40.0f, 120.0f, 255.0f - row * 40.0f, 255.0f});

            auto bp = brick.get<PositionComponent>();
            bp->width = 90.0f;
            bp->height = 26.0f;

            bricks.push_back(brick.entity);
        }
    }

    // in execute(), after the paddle bounce:
    for (auto it = bricks.begin(); it != bricks.end(); ++it)
    {
        auto brickPos = (*it)->get<PositionComponent>();

        if (overlaps(bpos, brickPos))
        {
            ballVelY = -ballVelY;
            ecsRef->removeEntity(it->entity);
            bricks.erase(it);
            score += 10;
            break;
        }
    }

    // members:
    std::vector<EntityRef> bricks;
    int score = 0;

Build and run — you have a playable Breakout.

Step 5 — Score text
-------------------

Text needs the ``TTFTextSystem`` with a registered font. Create it in the setup function in ``application.cpp`` (fonts are registered under a short alias, which is what ``makeTTFText`` takes — not the file path):

.. code-block:: cpp

    #include "UI/ttftext.h"

    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);
        ttfSys->registerFont("res/font/Inter/static/Inter_28pt-Regular.ttf", "regular");

        auto config = engine.getConfig();
        ecs.createSystem<BreakoutSystem>(config.width, config.height);
    });

Copy the font from the engine repo next to your executable (``cp -r <engine>/res/font/Inter build/res/font/Inter`` — asset paths are resolved relative to the working directory). Then in the system:

.. code-block:: cpp

    #include "UI/ttftext.h"

    // in init():
    auto text = makeTTFText(ecsRef, 10.0f, 8.0f, 1.0f, "regular", "Score: 0", 0.4f);
    scoreText = text.entity;

    // when a brick breaks:
    scoreText->get<TTFText>()->setText("Score: " + std::to_string(score));

Step 6 — Sound
--------------

An ``AudioSystem`` is registered by the engine automatically, so playing audio is just sending an event with a path to an ``.ogg``/``.mp3`` file (again relative to the working directory):

.. code-block:: cpp

    #include "Audio/audiosystem.h"

    ecsRef->sendEvent(PlaySoundEffect{"res/audio/hit.ogg"});   // on brick hit
    ecsRef->sendEvent(StartAudio{"res/audio/music.ogg", -1});  // looping music, e.g. in init()

Where to go from here
---------------------

- **Hot-reloadable scripting** — move brick-hit logic into a ``.pg`` PgScript file and iterate without recompiling. Start with the `PgScript quick start <https://github.com/Gallasko/ColumbaEngine/blob/main/docs/compiler/QUICK_START.md>`_.
- **Real collision layers** — replace the manual AABB checks with ``CollisionSystem`` + ``CollisionComponent`` (``examples/Asteroid`` and ``examples/TugOfWar`` show the pattern, including script-driven collision handlers).
- **Sprites instead of shapes** — register textures and use ``make2DTexture`` / ``makeUiTexture`` (``examples/RenderingTest``).
- **Configurable keys** — map scancodes to your own action enum with ``ConfiguredKeySystem`` (``examples/TetrisClone/keyconfig.h``).
- **Ship it to the web** — build the engine with Emscripten (``scripts/install/install-emscripten.sh``) and your game compiles to an ``.html`` + ``.wasm`` bundle; ``pg::Engine`` already handles the browser main loop.
