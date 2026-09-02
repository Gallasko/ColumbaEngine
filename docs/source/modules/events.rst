Events
======

Events are the engine's inter-system communication mechanism — input, audio, saves, and game logic all flow through them. There are two flavors: **typed C++ events** (plain structs, zero setup) and **StandardEvents** (dynamic, string-named, script-friendly).

Typed events
------------

Any struct works as an event — no base class, no registration:

.. code-block:: cpp

    // define
    struct AlienDestroyedEvent { float x, y; int points; };

    // send — from any system, any thread
    ecsRef->sendEvent(AlienDestroyedEvent{x, y, alien->points});

    // receive — declare the trait, override onEvent
    class ScoreSystem : public System<Listener<AlienDestroyedEvent>>
    {
        void onEvent(const AlienDestroyedEvent& e) override { score += e.points; }
    };

Delivery semantics
------------------

``sendEvent`` is **thread-safe from anywhere**. If the caller is already on the safe dispatch path it delivers synchronously; otherwise the event lands on a lock-free queue drained at the start of the next frame (during BasicTask, before systems run). Pass ``isDeferred = true`` to explicitly delay delivery one drain cycle.

The two listener traits differ in *when your code runs*:

- ``Listener<E>`` — ``onEvent`` fires during the event drain, before systems execute. Keep it cheap and thread-conscious: record state, don't do heavy work.
- ``QueuedListener<E>`` — events are buffered and handed to ``onProcessEvent`` at the start of *your system's own scheduled slot*. Use this when handling must happen inside your system's frame context (the audio system processes ``PlaySoundEffect`` this way).

One caveat: a single system cannot currently have both ``Listener<E>`` and ``QueuedListener<E>`` for the *same* event type.

The pattern used throughout the engine and examples: **record in ``onEvent``, act in ``execute()``/``onExecute()``** — see the input handling in the Breakout tutorial.

StandardEvents (dynamic events)
-------------------------------

``StandardEvent`` (``ECS/standardevent.h``) carries a string name and a key-value map — the bridge to PgScript and runtime-built systems:

.. code-block:: cpp

    ecsRef->sendEvent(StandardEvent{"enemyHit", "id", enemyId, "damage", 12});

A StandardSystem or script subscribes by name (``.onEvent("enemyHit", "onHit.pg")``); handlers read fields with ``event.get<int>("damage")``.

Bridging typed → standard
-------------------------

With the ``AUTO_CONVERT_EVENT`` CMake option (ON by default, defining ``PG_AUTO_CONVERT_EVENTS_TO_STANDARD``), a typed event can *also* be dispatched as a StandardEvent so scripts can hear it. Opt a type in with the macros:

.. code-block:: cpp

    struct AlienDestroyedEvent { float x, y; int points;
        STANDARD_EVENT_CONVERTIBLE(AlienDestroyedEvent)
    };
    // in a .cpp:
    STANDARD_EVENT_CONVERSION_IMPL(AlienDestroyedEvent)

Events without the macro pay zero conversion overhead.

Engine events you'll meet early
-------------------------------

- ``TickEvent`` — frame tick in milliseconds (prefer the ``DeltaTime`` trait over listening directly)
- ``OnSDLScanCode`` / ``OnSDLScanCodeReleased`` / ``OnSDLTextInput`` — keyboard
- ``OnMouseClick`` / ``OnMouseRelease`` / ``OnMouseMove`` — mouse (see also per-entity ``MouseLeftClickComponent``)
- ``StartAudio`` / ``PlaySoundEffect`` / ``SetMasterVolume`` ... — audio (see :doc:`audio`)
- ``SaveElementEvent`` — key-value persistence
- ``ResizeEvent`` — window size changed
