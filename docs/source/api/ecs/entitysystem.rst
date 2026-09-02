EntitySystem
============

``pg::EntitySystem`` (``ECS/entitysystem.h``) is the world: it owns entities, component storage, systems, the scheduler, events, saves, and the script registry. With ``pg::Engine`` you receive it ready-made in your setup function; standalone use just needs the constructor.

Lifecycle
---------

.. code-block:: cpp

    EntitySystem(const std::string& savePath = "save/savedata.sz");

    void start();        // spawns the run loop thread; taskflow re-executes until stop()
    void stop();
    void executeOnce();  // single taskflow pass — useful in tests (with fakeStart())
    void fakeStart();    // mark running without the loop thread (testing)

Entities
--------

.. code-block:: cpp

    EntityRef createEntity();
    EntityRef createEntity(const std::string& name);   // attaches an EntityName
    std::vector<EntityRef> createEntities(size_t count);

    void removeEntity(Entity* entity);
    void removeEntity(_unique_id id);

    Entity* getEntity(_unique_id id) const;
    Entity* getEntity(const std::string& name) const;

While the world is running, creation/removal is deferred and committed at the next frame boundary (see :doc:`../../modules/ecs`). ``createEntity`` returns a usable ``EntityRef`` immediately — it self-resolves after the commit.

Components
----------

Usually you attach through an ``EntityRef`` (``ref.attach<C>(...)``); the world-level API exists for generic code:

.. code-block:: cpp

    template <typename Type, typename... Args>
    CompRef<Type> attach(EntityRef entity, Args&&... args);

    // dynamic (string-named StandardComponent) variant, used by scripts
    CompRef<StandardComponent> attach(EntityRef entity, const std::string& componentName, ...);

    // attach a plain struct without a registered component system
    template <typename Type, typename... Args>
    CompRef<Type> attachGeneric(EntityRef entity, Args&&... args);

    template <typename Type> void detach(Entity* entity) noexcept;
    template <typename Comp> Comp* getComponent(_unique_id id) const;   // nullptr if absent
    template <typename Comp> auto view() const;                        // packed iteration

``attachGeneric`` is what quick game-side data uses (e.g. the ``BouncingBox`` struct in the template); ``attach`` requires a system that ``Own``'s the type.

Systems
-------

.. code-block:: cpp

    template <class Sys, bool Bypass = false, typename... Args>
    Sys* createSystem(const Args&... args);       // refuses while running unless Bypass

    template <class Sys> Sys* getSystem() const noexcept;
    template <class Sys> void deleteSystem();

    // ordering: A runs AFTER B
    template <typename SysAfter, typename SysBefore> void succeed();
    void succeed(const std::string& sysAfter, const std::string& sysBefore);
    template <typename SysBefore> void precede(const std::string& sysAfter);

    void dumbTaskflow(bool showEventNodes = true,
                      const std::string& outputFile = "") const;  // GraphViz dump

Events
------

.. code-block:: cpp

    template <typename Event>
    void sendEvent(const Event& event, bool isDeferred = false);

Thread-safe from any thread; see :doc:`../../modules/events` for delivery semantics.

Scripting
---------

.. code-block:: cpp

    ScriptRegistry& scripts();          // load/compile .pg files (bytecode VM)
    void setupVm(VM& vm);               // wire native modules into a VM instance
    template <typename ModuleType>
    void registerCustomVmModule(const std::string& name, ModuleType&& module);

Saves
-----

.. code-block:: cpp

    ElementType getSavedData(const std::string& id) const;   // key-value channel
    SaveManager& getSaveManager();
    void forceSaveNow();       // flush key-value store + all SaveSys systems + exit callbacks
    void clearAllSaveData();   // wipe both channels
