Serialization
=============

The serialization module (``src/Engine/serialization.h/.cpp``) is the persistence backbone: save files, per-system state, and the editor's inspector all run through it. This page covers both the everyday API and the internals an engine programmer needs.

The format
----------

Archives are **human-readable indented text**, not binary. A class opens as ``ClassName {`` with increased indentation and closes with ``}``; leaf values are emitted as attributes marked by the ``__PGSA`` sentinel::

    PositionComponent {
        x: __PGSA float {42.5},
        y: __PGSA float {100},
        visible: __PGSA bool {true}
    }

Consequences of the text format:

- **No endianness concerns** — scalars go through ``std::to_string`` and are re-parsed with stream extraction. The portability caveat is float formatting/locale, not byte order.
- Save files are diffable and hand-editable — helpful during development, worth remembering for anti-cheat expectations.
- Every file starts with an archive version line (currently ``1.0.0``, ``ARCHIVEVERSION``). A file with an unknown version parses to *empty* — there is no cross-version migration inside the serializer itself (game-level migration lives in :doc:`versioning`).

Everyday API: serialize a custom type
-------------------------------------

Specialize two function templates — declare in your header, define in a ``.cpp``. This is exactly what the component generator emits, so hand-written types stay consistent with generated ones:

.. code-block:: cpp

    // header
    template <> void serialize(Archive& archive, const MyThing& value);
    template <> MyThing deserialize(const UnserializedObject& serializedString);

    // cpp
    template <>
    void serialize(Archive& archive, const MyThing& value)
    {
        archive.startSerialization("MyThing");
        serialize(archive, "hp", value.hp);
        serialize(archive, "name", value.name);
        archive.endSerialization();
    }

    template <>
    MyThing deserialize(const UnserializedObject& serializedString)
    {
        MyThing data;
        defaultDeserialize(serializedString, "hp", data.hp);
        defaultDeserialize(serializedString, "name", data.name);
        return data;
    }

Prefer ``defaultDeserialize`` over indexing: it only assigns when the field exists, so **adding or removing fields stays save-compatible in both directions** — old saves load into new code (missing fields keep their defaults) and vice versa.

Built-in support: ``bool``, integer types, ``float``/``double``, ``std::string``, ``std::vector<T>``, ``std::unordered_map<K,V>``, and the engine math types (``Vector2D/3D/4D``, ``ModelInfo``).

Reading: ``UnserializedObject``
-------------------------------

The read side is a lazily-parsed tree. ``obj["fieldName"]`` navigates children (missing keys log an error and return a null sentinel — check with ``find`` first), ``getAsAttribute()`` extracts a leaf's ``{name, value}``, and ``isClass()`` distinguishes nested objects from attributes. The parser is an indentation-tracking state machine — which is why class names must not contain ``:`` and attribute values should not contain the ``__PGSA`` marker (currently unenforced; see the TODOs at the top of ``serialization.h``).

Files: ``Serializer``
---------------------

``Serializer`` maps object names to serialized strings and handles the file round-trip:

.. code-block:: cpp

    Serializer s("save/mydata.sz");            // autoSave: flushes on destruction
    s.serializeObject("settings", mySettings);
    auto loaded = s.deserializeObject<Settings>("settings");

A process-wide default is available via ``Serializer::getSerializer("global.sz")``. The engine's save channels (below) each own their own ``Serializer``.

How the engine uses it
----------------------

Two channels, one flush point (``ecs->forceSaveNow()``):

1. **SaveManager (key-value)** — ``SaveElementEvent{key, value}`` events land in an ``unordered_map<string, ElementType>`` written at most once per frame (writes are coalesced; ``forceSave()`` bypasses the coalescing). ``ElementType`` is the engine's tagged scalar union (float/int/size_t/string/bool).
2. **SaveSys blobs (per system)** — a system inheriting ``SaveSys`` gets its ``save(Archive&)`` output stored under its ``getSystemName()`` in ``saveFolder/saveSystemFile`` (default ``save/system.sz``). Load happens at system registration (or ``firstLoad()`` if no blob exists); save happens at teardown and on every ``forceSaveNow()``.

On Emscripten the save folder lives on an OPFS/WasmFS mount (see :doc:`files`); the engine force-saves on tab hide/close.

Generated component serialization
---------------------------------

Components declared in ``.pgcomp`` schema files get their serialization generated at build time by ``tools/component_generator.pg`` (self-hosted in PgScript). Per component the generator emits:

- ``<Name>.generated.h`` — the struct plus ``serialize``/``deserialize`` declarations and a force-link shim so static registration always runs
- ``<Name>.generated.cpp`` — field-by-field ``serialize``/``defaultDeserialize`` bodies (the same pattern shown above)
- ``<Name>.serialization.cpp`` — VM glue: script setters, ``REGISTER_COMPONENT_SERIALIZER``, attach-from-script handlers, and ``ComponentProxyRegistry`` metadata for the inspector

If you add fields to a ``.pgcomp``, regeneration keeps C++, saves, scripts, and the inspector in sync — this is why hand-editing generated files is never the right move. See ``docs/COMPONENT_SCHEMA_REFERENCE.md`` for the schema format.

Internals & known caveats (engine programmers)
----------------------------------------------

- ``Archive`` buffers into a ``std::stringstream``; commas/newlines are deferred via flags so trailing separators come out right. ``InspectorArchive`` is a subclass that builds a live introspection tree instead of text — that's the editor inspector's data source.
- Multi-line string values are re-escaped through the archive's own ``endl`` so the indentation parser doesn't mistake them for class bodies.
- Type names are validated on read (e.g. ``int`` accepts ``int``/``unsigned int``/``size_t``; ``bool`` must be literally ``true``/``false``) and mismatches log rather than throw.
- Open TODOs worth knowing before touching this code: no *per-object* version header yet (only per-file), empty-class parsing has a pending fix, and the forbidden-character rules for names are unenforced. ``serialization.cpp`` is one of the most TODO-dense files in the engine — tread carefully and extend the tests in ``test/serialize.cc`` alongside changes.
