Renderer
========

The renderer is an ECS system like everything else: ``MasterRenderer`` (``src/Engine/Renderer/renderer.h``) owns the shader/texture/material tables, the cameras, and a double-buffered list of *render calls*. Game-facing systems (sprites, shapes, text, progress bars) never touch OpenGL — they produce ``RenderCall`` records, and the master renderer batches, sorts, and draws them.

The frame, in two phases
------------------------

Rendering is split across two threads with a double buffer:

1. **Collect (ECS thread)** — ``MasterRenderer::execute()`` walks every registered renderer system, gathers their ``RenderCall`` lists, merges batchable calls with identical state, and sorts everything into the *back* buffer.
2. **Draw (GL thread)** — ``Window::render()`` calls ``MasterRenderer::renderAll()``, which walks the *front* buffer and issues instanced GL draws (``glDrawElementsInstanced``); ``endRender()`` then flips the buffers.

Because collection and drawing never share a buffer, renderer systems are free to rebuild calls while the previous frame is still being drawn.

The sort key
------------

Each ``RenderCall`` carries a packed 64-bit key (``Renderer/rendercall.h``); sorting by key alone yields the full draw order::

    bit 63        visibility (0 = visible; invisible calls sort last and are skipped)
    bits 62..59   render stage      (Render / PreRender / PostProcess)
    bits 58..56   viewport index    (0-7)
    bits 55..54   opacity           (Opaque / Normal / Additive / Subtractive)
    bits 53..30   depth             (24 bits, from PositionComponent::z)
    bits 29..0    material id

Two calls with equal keys, equal ``OpenGLState``, and ``batchable == true`` have their per-instance float data concatenated and are drawn in a single instanced call.

GL state is data
----------------

``OpenGLState`` (``rendercall.h``) captures scissor, depth test/function, and blend factors without including any GL header. The master renderer applies only the *diff* between consecutive calls. Clipping is driven by components: give an entity a ``ClippedTo`` component pointing at a clipper entity and its render call gets a scissor rect automatically.

Cameras and viewports
---------------------

``ViewportComponent`` selects which camera a call uses: viewport 0 is the built-in default camera; viewports 1-7 map to registered ``BaseCamera2D`` cameras (orthographic, with zoom and ``screenToWorld``). Register cameras with ``masterRenderer->queueRegisterCamera(entityId)``.

Registering shaders, textures, atlases
--------------------------------------

.. code-block:: cpp

    masterRenderer->registerShader("myShader", "shader/my.vs", "shader/my.fs");
    masterRenderer->registerTexture("hero", "res/sprites/hero.png");     // stb_image, RGBA, GL_NEAREST
    masterRenderer->registerAtlasTexture("tiles", "res/tiles.png", "res/tiles.atlas",
                                         std::make_unique<LoadedAtlas>(...));

Texture registration is thread-safe: while the ECS is running, ``queueRegisterTexture`` defers the GL upload to the start of the next ``renderAll()``. Atlas sub-textures are addressed as ``"atlasName.subName"`` in ``Texture2DComponent``.

The default shaders are loaded by a PgScript file, ``res/setupRenderer.pg``, via the ``RendererModule`` script bindings (``loadShader``, ``loadTexture``, ``loadAtlasTexture``) — you can add your own resources there without recompiling.

Writing a new renderer system
-----------------------------

Route A — ``GenericRenderSystem`` (recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``Renderer/genericrendersys.h`` handles the group management, change tracking, and call caching. You override exactly two things: ``setup()`` (declare your material) and ``createRenderCall(...)`` (pack per-instance floats). The smallest real example is ``RoundedRect2DObjectSystem`` (``2D/simple2dobject.h/.cpp``):

.. code-block:: cpp

    struct RoundedRect2DObjectSystem : public GenericRenderSystem<
        RoundedRect2DObject, RoundedRect2DObjectChangedEvent,
        PositionComponent,   PositionSettledEvent,
        ViewportComponent,   ViewportComponentChangedEvent>
    {
        RoundedRect2DObjectSystem(MasterRenderer* mr) : GenericRenderSystem(mr) {}

        void setup() override
        {
            Material mat;
            mat.shader = masterRenderer->getShader("RoundedRect");
            mat.nbTextures = 0;
            mat.uniformMap.emplace("sWidth", "ScreenWidth");
            mat.uniformMap.emplace("sHeight", "ScreenHeight");
            mat.setSimpleMesh({3, 2, 1, 4, 1});   // pos(3) size(2) rot(1) color(4) radius(1)

            materialId = masterRenderer->registerMaterial(mat);
        }

        RenderCall createRenderCall(CompRef<RoundedRect2DObject> obj,
                                    CompRef<PositionComponent> pos,
                                    CompRef<ViewportComponent> vp) override;
        uint64_t materialId = 0;
    };

In ``createRenderCall`` you set stage/opacity/material/viewport on the call, let ``processPositionComponent(pos)`` fill in visibility, depth, and scissor, then push the floats matching your ``setSimpleMesh`` layout. ``Simple2DObjectSystem`` and ``Texture2DComponentSystem`` (``2D/texture.cpp``, which also shows atlas lookup and per-texture opacity) follow the same pattern.

Route B — raw ``AbstractRenderer``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For full control (custom grouping, computed geometry), subclass ``AbstractRenderer`` plus ``System<...>`` directly and fill the inherited ``renderCallList`` yourself, calling ``setDirty(true)`` / ``finishChanges()``. ``ProgressBarComponentSystem`` (``UI/progressbar.cpp``) is the minimal example; ``TTFTextSystem`` (``UI/ttftext.cpp``) is the advanced one — it rasterizes FreeType glyphs into a per-font atlas texture and draws whole strings as one instanced call.

Registration is automatic: the ``BaseAbstractRenderer`` constructor calls ``masterRenderer->addRenderer(this)``, so creating your system is enough:

.. code-block:: cpp

    ecs->createSystem<MyRenderSystem>(window.masterRenderer);

The built-in systems are wired in ``Init/rendersystems.cpp`` — add yours there if it should ship with the engine.

Porting to a new target
-----------------------

There is no formal render-backend interface today; SDL2 + OpenGL are used directly, with the web/desktop split handled by ``#ifdef __EMSCRIPTEN__``. The engine currently targets **GL core 3.3** on desktop and **GLES 3.0** in the browser. A new target touches a small, known set of places:

- ``Helpers/openglobject.h`` — the single place GL headers are chosen, and home of the only GL abstractions (``OpenGLShaderProgram``, ``OpenGLBuffer``, ``OpenGLVertexArrayObject``). A new backend starts by reimplementing these wrappers.
- ``window.cpp`` (desktop) / ``engine.cpp`` (Emscripten) — context creation, GL attributes, swap.
- Two GL-enum divergence points: texture wrap mode (``renderer.cpp``, ``GL_CLAMP`` vs ``GL_CLAMP_TO_EDGE``) and the font-atlas format (``ttftext.cpp``, ``GL_LUMINANCE`` vs ``GL_RED``).
- ``MasterRenderer::processRenderCall`` / ``setState`` (``renderer.cpp``) — the only functions that issue draw and state calls.

Everything above ``RenderCall`` (materials, sort keys, batching, all the game-facing systems) is already backend-agnostic data.
