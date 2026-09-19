#pragma once

/**
 * @file entity_loop_lowering.h
 * @brief AST pass: lower eager getEntities() loops to lazy entity iteration.
 *
 * Rewrites
 *
 *     for (var e : getEntities("Position")) { var p = e.PositionComponent ... }
 *
 * into
 *
 *     for (var @eid0 : __ecsEntityIds("Position")) {
 *         var e = __ecsEntityView(@eid0, "PositionComponent")
 *         ...original body...
 *     }
 *
 * getEntities eagerly materializes, for EVERY entity, a full table with ALL
 * components serialized plus two per-entity native closures - before the
 * loop runs at all. The lowered form snapshots only the entity ids and
 * builds, per iteration, a minimal table containing just the components the
 * body actually accesses (statically known from the e.<Member> usage).
 *
 * Lowering conditions (ALL must hold, otherwise the loop is left untouched):
 * - The iterable is a call to the bare global `getEntities` with one arg.
 * - The program nowhere declares or assigns `getEntities`, `__ecsEntityIds`
 *   or `__ecsEntityView` (shadow guard - if it does, NOTHING is lowered).
 * - Every use of the loop variable in the body is a member read or write:
 *   dot form (e.X / e.X = v) or bracket form with a string literal
 *   (e["X"]). The loop bails if the variable escapes (passed bare, indexed
 *   with a dynamic key, captured by a nested function/class), is reassigned
 *   or shadowed, or if `has`/`attachComp` members are used (the lazy view
 *   does not attach those closures).
 *
 * Documented semantic divergences vs the eager path (pinned by tests):
 * - The body mutating the ECS mid-loop: components attached to a
 *   NOT-yet-visited entity become visible to its iteration (the lazy view
 *   is built per step); the eager path snapshots everything up front.
 * - An entity deleted mid-loop yields a table with only __entityId
 *   (deterministic) where the eager path kept a stale pre-built table.
 */

#include "../ast_pass.h"

namespace pg
{
    class EntityLoopLoweringPass : public AstPass
    {
    public:
        std::string getName() const override { return "EntityLoopLowering"; }

        bool runPass(VM* vm, std::queue<StatementPtr>& statements) override;
    };
}
