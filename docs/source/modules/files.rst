File Access
===========

All engine file IO goes through ``Files/filemanager.h``. The API is deliberately small and ``noexcept``: a missing file yields a ``TextFile`` with empty ``data``, never an exception.

.. code-block:: cpp

    struct TextFile { std::string filepath; std::string data; };

Three accessors
---------------

- ``FileAccessor`` — plain filesystem access relative to the working directory: ``openTextFile``, ``openTextFolder(recursive)``, ``writeToFile``, ``exists``.
- ``ResourceAccessor`` — bundled resources, addressed with a ``":/"`` prefix.
- ``UniversalFileAccessor`` — **use this one.** It tries the system path first and falls back to the ``":/"`` resource path only if nothing was found. That order means users can override any bundled ``res/`` file by dropping a same-named file next to the executable — mod-friendly by default. It also adds path helpers (``getFileName``, ``getFoldername``, ``getRelativePath``).

.. code-block:: cpp

    auto file = UniversalFileAccessor::openTextFile("res/config.json");
    if (file.data.empty()) { /* not found */ }

    FileAccessor::writeToFile(file, newContent);

Path conventions
----------------

Paths are resolved relative to the **working directory**; the convention is to keep assets under ``res/`` next to the executable (all examples follow it). On Emscripten, absolute-style paths get a leading ``/`` (e.g. ``/res/audio/...``).

Web builds (Emscripten)
-----------------------

The browser has no real filesystem; ``pg::Engine`` mounts one before anything else runs:

- The save directory (``EngineConfig::saveFolder``, default ``save``) is mounted on an **OPFS backend via WasmFS** — persistent across page reloads. The mount happens on a pthread in ``Engine::exec()`` because OPFS cannot be mounted from the main thread.
- Game assets are shipped in the ``.data`` preload bundle Emscripten generates at link time.
- On tab hide/close the engine calls ``ecs->forceSaveNow()`` automatically (``visibilitychange`` / ``beforeunload`` hooks), so saves survive the user closing the tab.

PgScript access
---------------

Scripts get file IO through the VM's ``FileModule`` (``Files/filemodule.h``): ``readFile(path)``, ``writeFile(path, content)``, ``fileExists(path)``. ``readFile`` resolves through ``UniversalFileAccessor``, so the same override semantics apply.

Native file dialogs
-------------------

``tinyfiledialogs`` is vendored (``Helpers/tinyfiledialogs.h``) for open/save dialogs; the scene editor uses it. It is desktop-only — don't reach for it in game code you intend to ship to the web.
