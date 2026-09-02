EntityRef & CompRef
===================

Handles are how you keep hold of entities and components across frames. Both are small value types that are safe to copy and store as system members.

EntityRef
---------

``pg::EntityRef`` (``ECS/entityref.h``) wraps ``{Entity*, id, ecsRef}``:

.. code-block:: cpp

    template <typename Comp> bool has() const;
    bool has(const std::string& compName) const noexcept;

    template <typename Comp> CompRef<Comp> get() const;
    template <typename Comp, typename... Args> CompRef<Comp> attach(Args&&...);
    template <typename Comp, typename... Args> CompRef<Comp> attachGeneric(Args&&...);

    Entity* operator->();       // forward to the entity
    operator Entity*();         // implicit conversion (e.g. for removeEntity)
    bool empty() const;

Typical flow — factories return component lists whose ``.entity`` you keep:

.. code-block:: cpp

    auto shape = makeSimple2DShape(ecsRef, Shape2D::Square, x, y, color);
    ball = shape.entity;                          // EntityRef member
    ...
    auto pos = ball->get<PositionComponent>();    // later, any frame
    ecsRef->removeEntity(ball.entity);            // deletion

CompRef
-------

``pg::CompRef<Comp>`` wraps ``{Comp*, entityId, ecsRef}`` with pointer-like access:

.. code-block:: cpp

    Comp* operator->();
    operator Comp*();
    bool empty() const;
    Entity* getEntity() const;

Lifetime semantics (read this once)
-----------------------------------

- **Refs are lazy and self-healing.** A ref created in the same tick as an ``attach`` (structural changes are deferred — see :doc:`../../modules/ecs`) resolves itself on first use, after the commit. This is why factory results are immediately storable.
- **Deletion makes refs resolve to null, not dangle** — component lookup goes through the world, which returns ``nullptr`` for missing components. Hence the idiom in every bundled system:

  .. code-block:: cpp

      auto pos = ent->get<PositionComponent>();
      if (not pos) return;    // entity was deleted — always null-check re-fetched refs

- **One sharp edge:** a ``CompRef`` that already resolved caches its raw pointer; after entity deletions, prefer re-``get()`` over trusting a long-held resolved ref.
- ``entity->has<C>()`` also checks *pending* attachments, so attach-then-check within one tick behaves as expected.
