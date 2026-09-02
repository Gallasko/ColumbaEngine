Versioning & Migrations
=======================

The versioning module (``src/Engine/Versioning/``) answers one question at startup: *the player's save was written by version X, the game is now version Y — what should happen?* It ships semantic-version comparison, a game manifest with changelog, and a save-migration registry.

The manifest
------------

Your game declares its version in a ``manifest.json`` (path set by ``EngineConfig::manifestPath``, default ``manifest.json``):

.. code-block:: json

    {
      "version": "1.1.0",
      "description": "Factory automation game",
      "changelog": [
        { "version": "1.1.0", "changes": ["Added conveyor tiers", "Balance pass"] },
        { "version": "1.0.0", "changes": ["Initial release"] }
      ]
    }

A missing or malformed manifest falls back to ``1.0.0`` — versioning is opt-in and never blocks startup.

What happens at startup
-----------------------

``pg::Engine`` runs ``VersionManager::initialize`` during ECS setup. It reads the version stamped in the save file (key ``__pg_game_version``), compares against the manifest, and acts according to two ``EngineConfig`` flags (both default ``true``):

===================== ==========================================================
Situation             Action
===================== ==========================================================
New install           stamp current version, nothing else
Same version          nothing
**Major** bump        ``autoWipeSaveOnMajorBump`` → wipe all save data, restamp
Minor / patch bump    ``autoRunMigrations`` → run registered migrations, restamp
Downgrade             nothing (restamp only)
===================== ==========================================================

The check result also carries ``relevantChangelog`` — the entries between the saved and current version — ready to show the player a "what's new" screen.

Migrations
----------

Register a callback per target version; on update, all callbacks with ``oldVersion < target <= currentVersion`` run in ascending version order:

.. code-block:: cpp

    manifest.registerMigration(SemanticVersion{1, 1, 0}, [](SaveManager& save)
    {
        // rename a key introduced in 1.1.0
        auto old = save.getValue("score");
        save.onProcessEvent(SaveElementEvent("highscore", old));
    });

This is the intended path for evolving key-value save data across releases. (Per-system ``SaveSys`` blobs get field-level tolerance for free from ``defaultDeserialize`` — see :doc:`serialization` — so most save-format drift needs no migration at all.)

SemanticVersion
---------------

``SemanticVersion`` (header-only) is the comparison workhorse: parses ``"M.m.p"`` strings (anything unparseable resets to ``1.0.0``), full ordering operators, and the bump predicates ``isMajorBumpFrom`` / ``isMinorBumpFrom`` / ``isPatchBumpFrom``. It's fully covered by ``test/semanticversion.cc`` — extend those tests if you touch it.

Practical guidance
------------------

- Bump **patch/minor** for save-compatible releases; migrations cover renames and derived data.
- Reserve **major** for genuinely incompatible saves — with the default flags, a major bump *deletes player saves*. If that's not what you want, set ``autoWipeSaveOnMajorBump = false`` in your ``EngineConfig`` and handle it yourself from the ``VersionCheckResult``.
- Keep the changelog array current; the engine sorts and slices it for you.
