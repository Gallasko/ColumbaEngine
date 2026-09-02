System Traits
=============

A system is declared by listing traits: ``class MySys : public System<Trait1, Trait2, ...>``. Each trait contributes required overrides and provided members; the engine registers exactly what you list. (``System<...>`` inherits ``AbstractSystem`` plus every trait; registration dispatches per-trait at compile time — ``ECS/system.h``.)

Every system may override:

.. code-block:: cpp

    virtual std::string getSystemName() const;   // names the Taskflow node; REQUIRED for SaveSys
    void execute();                              // runs once per frame (unless StoragePolicy)

Hook traits
-----------

``InitSys``
    Requires ``void init()``. Called once, at system registration — create entities and register groups here.

``DeltaTime`` (``Systems/coresystems.h``)
    Requires ``void onExecute(float deltaTime)``. Called once per frame with elapsed **seconds**; the trait listens to ``TickEvent``, accumulates, and resets for you. Also provides ``getDeltaTime()``. Do **not** override ``execute()`` on a ``DeltaTime`` system — the trait supplies it.

Event traits
------------

``Listener<E>``
    Requires ``void onEvent(const E&)``. Delivered synchronously during the frame's event drain, *before* systems execute. Keep handlers cheap; record state and act later in ``execute()``.

``QueuedListener<E>``
    Requires ``void onProcessEvent(const E&)``. Events are buffered and delivered at the start of *this system's* scheduled slot. Use when handling must run in your system's frame context.

    A system cannot have both ``Listener<E>`` and ``QueuedListener<E>`` for the same ``E``.

Component traits
----------------

``Own<C>``
    This system owns storage for component type ``C``: attaching a ``C`` routes through this system, and it gets ``view<C>()`` / ``atEntity<C>(id)`` for packed iteration. One owner per component type.

``Ref<C>``
    Read/iterate a ``C`` owned by another system (``view<C>()`` provided). Declares the data dependency explicitly.

Multi-component joins use groups (available on any system):

.. code-block:: cpp

    registerGroup<PositionComponent, Velocity>();          // once, e.g. in init()
    for (auto e : viewGroup<PositionComponent, Velocity>())
    {
        auto pos = e->get<PositionComponent>();
        auto vel = e->get<Velocity>();
    }

Persistence trait
-----------------

``SaveSys``
    Requires ``void save(Archive&)`` and ``void load(const UnserializedObject&)``; optional ``void firstLoad()`` for the no-save-yet case. Load runs at registration, save at teardown and on ``forceSaveNow()``. The blob is keyed by ``getSystemName()``, so a real name is mandatory. Minimal example: ``examples/SimpleBoxBouncer``.

Scheduling policy traits
------------------------

At most one policy per system (two logs an error):

``StoragePolicy``
    No Taskflow node — ``execute()`` never runs. For pure component containers and event-only systems (e.g. ``AsepriteLoader``).

``IndependentPolicy``
    Scheduled with no ordering edge to the frame's root task — runs freely in parallel.

``ManualPolicy``
    Not scheduled; you drive it yourself.

(Default, no policy trait: *Sequential* — runs each frame after the root task, in parallel with other systems unless constrained by ``succeed``/``precede``.)

Putting it together
-------------------

.. code-block:: cpp

    class EnemySystem : public System<InitSys, DeltaTime,
                                      Listener<GamePaused>, SaveSys>
    {
        virtual std::string getSystemName() const override { return "Enemy System"; }

        void init() override                  { registerGroup<PositionComponent, Enemy>(); }
        void onEvent(const GamePaused&) override { paused = true; }
        void onExecute(float dt) override     { if (not paused) moveEnemies(dt); }

        void save(Archive& a) override        { serialize(a, "wave", wave); }
        void load(const UnserializedObject& o) override { defaultDeserialize(o, "wave", wave); }

        int wave = 0;
        bool paused = false;
    };
