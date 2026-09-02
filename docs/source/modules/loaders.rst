Asset Loaders
=============

The ``Loaders/`` module turns files on disk into things the renderer can draw: texture atlases, uniform sprite sheets, and Aseprite animations. Plain textures don't need a loader at all — ``masterRenderer->registerTexture(name, path)`` loads any PNG/JPG via the bundled stb_image.

Texture atlases (``LoadedAtlas``)
---------------------------------

``Loaders/atlasloader.h`` parses a text ``.atlas`` file describing named sub-regions of one image. Recognized keys: ``Image Path``, ``Atlas Width``/``Atlas Height``, ``Row``/``Base Y``, ``Charactere``, ``Width``, ``Height``, ``Y-Offset``, with ``###########`` terminating each record. Each entry becomes an ``AtlasTexture`` carrying its UV rectangle.

Register an atlas with the renderer, then address sub-textures as ``"atlasName.subName"``:

.. code-block:: cpp

    masterRenderer->registerAtlasTexture("tiles", "res/tiles.png", "res/tiles.atlas",
                                         std::make_unique<LoadedAtlas>(...));

    makeUiTexture(ecsRef, 32, 32, "tiles.grass");   // Texture2DComponentSystem splits on '.'

Uniform grids (``GridAtlasLoader``)
-----------------------------------

For sprite sheets laid out on a regular grid there is no file format to write — ``GridAtlasLoader`` (``Loaders/gridatlasloader.h``) subclasses ``LoadedAtlas`` and slices the sheet arithmetically:

.. code-block:: cpp

    // (imgPath, atlasW, atlasH, frameW, frameH, columns, frameCount, startX = 0, startY = 0)
    auto grid = std::make_unique<GridAtlasLoader>("res/walk.png", 256, 64, 32, 32, 8, 16);

Frames are named by index (``"walk.0"``, ``"walk.1"``, ...).

Aseprite animations
-------------------

The Aseprite importer (``Loaders/Aseprite/``) reads Aseprite's **JSON export** (sprite sheet + metadata) and understands frame durations, frame tags (one tag = one named animation), tag direction (including reversed), and custom data.

The full pipeline, exactly as ``examples/PixelJam`` uses it:

.. code-block:: cpp

    // 1. Load the JSON — one AsepriteFile per exported sheet
    auto loader = ecs->createSystem<AsepriteLoader>();
    const auto anim = loader->loadAnim("res/sprites/main-char.json", "main-char");

    // 2. Register the sheet as an atlas (AsepriteFileAtlasLoader adapts frames to atlas entries)
    masterRenderer->registerAtlasTexture(anim.filename, anim.metadata.imagePath.c_str(), "",
                                         std::make_unique<AsepriteFileAtlasLoader>(anim));

    // 3. Animate an entity: attach the frames of a tag
    auto idle = loader->getAnimationFrames("main-char", "Idle_Front");
    ecsRef->attach<Texture2DAnimationComponent>(playerEnt.entity, idle,
                                                /*running*/ true, /*loop*/ true);

    // 4. Switch animations at runtime
    player->get<Texture2DAnimationComponent>()->changeAsepriteAnimation("main-char/Run_Front");

``Texture2DAnimatorSystem`` (``2D/animator2d.h``) ticks the animation and rewrites the entity's ``Texture2DComponent`` texture name each frame — animation is data, no per-game code needed.

Tilemaps
--------

The engine does not currently ship a general tilemap loader (``Loaders/tileloader.*`` is legacy and disabled). The working pattern is the one ``examples/PixelJam`` uses: a **Tiled** importer (built on the header-only ``tileson``) that registers each tileset as an atlas via a small ``LoadedAtlas`` adapter:

.. code-block:: cpp

    masterRenderer->registerAtlasTexture(tileset.name, tileset.imagePath.c_str(), "",
                                         std::make_unique<TileMapAtlasLoader>(tileset));

If you need Tiled support, start from ``examples/PixelJam/Tiled_Lib/`` — promoting it into the engine proper is on the roadmap.

Writing a custom loader
-----------------------

Any format that reduces to "an image plus named UV rectangles" only needs a ``LoadedAtlas`` subclass that fills ``AtlasTexture`` entries (this is all ``AsepriteFileAtlasLoader`` and ``GridAtlasLoader`` do). For text formats, ``Files/fileparser.h`` provides ``FileParser`` — a regex-keyed line parser (``addCallback(pattern, fn)`` + ``run()``) that powers the ``.atlas`` reader.
