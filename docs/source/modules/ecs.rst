ECS Architecture
================

Everything in ColumbaEngine is a system running inside one ``EntitySystem`` (the "world", ``ECS/entitysystem.h``) — rendering, audio, scripting, and your game logic all use the same machinery. This page explains the model; see :doc:`../api/ecs/index` for the full API reference.

The execution model
-------------------

The world runs on a `Taskflow <https://taskflow.github.io/>`_ graph that re-executes continuously after ``ecs->start()``. Each iteration:

1. **BasicTask** (the root node) runs alone. It drains the event queues, commits all deferred structural changes (entity/component creation and deletion), and applies pending script hot-reloads. Nothing else runs during this window, which is what makes those operations safe.
2. **Your systems** run — after BasicTask, and *in parallel with each other* unless you order them.

Ordering is explicit and per-pair:

.. code-block:: cpp

    ecs.createSystem<CollisionSystem>();
    ecs.createSystem<CollisionHandlerSystem>();
    ecs.succeed<CollisionHandlerSystem, CollisionSystem>();   // handler runs AFTER collision

Execution policies (set via traits) change how a system participates: the default (*Sequential*) runs after BasicTask each frame; ``IndependentPolicy`` runs with no ordering edge; ``StoragePolicy`` opts out of execution entirely — the system is a pure component container / event receiver, and its ``execute()`` never runs.

``dumbTaskflow()`` dumps the whole graph (GraphViz) — invaluable when debugging ordering.

Deferred mutation
-----------------

While the world is running, structural changes never happen immediately:

- ``createEntity``, ``removeEntity``, ``attach``, ``detach`` are routed through a command dispatcher and **committed at the next BasicTask**, when no system is running.
- This is why ``EntityRef``/``CompRef`` are *lazy*: a ref created in the same tick as the attach resolves itself on first use, after the commit. ``entity->has<C>()`` also checks pending components, so attach-then-check within one tick works.
- The corollary: after a ``removeEntity``, refs resolve to ``nullptr`` — **always null-check** components you re-fetch (``if (not pos) return;``), as every bundled example does.

Thread-safety rules of thumb
----------------------------

- ``sendEvent`` is safe from any thread (it enqueues unless it is already on the safe path).
- Structural changes are safe from any thread (deferred, see above).
- *Reading and writing component data* is where you think: two systems that touch the same components and run in the same frame must be ordered with ``succeed``/``precede``, or they will race.

Iterating components
--------------------

Components live in sparse sets — O(1) lookup by entity, packed arrays for iteration. A system that ``Own<C>``'s or ``Ref<C>``'s a component iterates it directly:

.. code-block:: cpp

    for (const auto& timer : view<Timer>())      // single component, packed iteration
        if (timer->running) { ... }

For multi-component joins, register a **group** once (usually in ``init()``) and iterate it; membership is maintained incrementally as components attach/detach, not recomputed per frame:

.. code-block:: cpp

    registerGroup<PositionComponent, Velocity, Ball>();

    for (auto ball : viewGroup<PositionComponent, Velocity, Ball>())
    {
        auto pos = ball->get<PositionComponent>();
        auto vel = ball->get<Velocity>();
        ...
    }

(Real example: the ball/alien collision loop in ``examples/InvadersBreaker/enemy.h``.)

Declaring systems: the trait list
---------------------------------

A system is a class inheriting ``System<Traits...>``; each trait opts into one capability, and the engine wires exactly what you list:

============================ ==================================================================
Trait                        What it gives you
============================ ==================================================================
``InitSys``                  ``init()``, called once at registration
``Own<C>``                   this system owns storage for component ``C`` (+ ``view<C>()``)
``Ref<C>``                   iterate/read a ``C`` owned by another system
``Listener<E>``              ``onEvent(const E&)``, delivered during the event drain
``QueuedListener<E>``        ``onProcessEvent(const E&)``, delivered in *your* execution slot
``DeltaTime``                ``onExecute(float seconds)``, tick accumulation handled for you
``SaveSys``                  ``save(Archive&)`` / ``load(...)`` persistence (needs a system name)
``StoragePolicy``            no ``execute()`` — pure storage / event-only system
``IndependentPolicy``        scheduled with no ordering edge to BasicTask
============================ ==================================================================

Override ``getSystemName()`` in every real system — it names the Taskflow node for profiling and is **required** for ``SaveSys``.

Two ways to build a system
--------------------------

Beyond ``System<Traits...>`` classes, ``createStandardSystem(...)`` builds a system at runtime from a fluent description — including handlers written in PgScript:

.. code-block:: cpp

    createStandardSystem(ecs, "spawner")
        .onInit([](...) { ... })
        .onEvent("OnMouseClick", "res/scripts/spawn.pg")   // hot-reloadable script handler
        .onExecute("res/scripts/tick.pg");

See ``examples/StandardSys`` and the StandardSystem guides in ``docs/STANDARD_SYSTEM_INDEX.md``. Use C++ ``System`` classes for engine-shaped, performance-sensitive systems; use StandardSystems for game logic you want to iterate on with hot reload.

Saving state
------------

Two channels, both flushed by ``ecs->forceSaveNow()`` (which the web build calls automatically on tab close):

- **Key-value**: ``ecsRef->sendEvent(SaveElementEvent{"highscore", score})``, read back with ``ecs->getSavedData("highscore")``. Coalesced to at most one disk write per frame.
- **Per-system blobs**: the ``SaveSys`` trait — your ``save(Archive&)``/``load(...)`` run automatically at registration/teardown, keyed by system name. ``firstLoad()`` covers the no-save-yet case. See ``examples/SimpleBoxBouncer`` for a complete 10-line example, and :doc:`serialization` for the format underneath.

``AutoSaveSystem`` (``Systems/autosavesystem.h``) adds periodic flushes (default: every 3 minutes).
