#pragma once

#include <cstdint>

#include "Systems/oneventcomponent.h"

#include "serialization.h"

#include "ECS/entitysystem.h"

namespace pg
{
    struct SceneElementClicked { SceneElementClicked(EntityRef entity) : entity(entity) {} EntityRef entity; };

    struct SaveScene { std::string otherNameForScene; };

    struct LoadScene { std::string filename; };

    struct NameScene { std::string filename; };

    struct NewSceneLoaded {};

    struct SceneElement : public Ctor
    {
        SceneElement() {}
        SceneElement(const SceneElement& other) : entity(other.entity) {}

        SceneElement& operator=(const SceneElement& other)
        {
            entity = other.entity;

            return *this;
        }

        virtual void onCreation(EntityRef ent) override { entity = ent; }

        // std::string sceneName;

        EntityRef entity;
    };

    struct Scene
    {
        virtual ~Scene() {}

        /**
         * @brief Pure abstract function called when the scene is loaded to load the first entities, components and link events
         */
        virtual void init() = 0;

        /**
         * @brief Virtual function called when all the init is done and all the entities/components are loaded
         */
        virtual void startUp() { };

        virtual void onLeave() { };

        virtual void execute() { };

        template <typename Event, typename Callback>
        EntityRef listenToEvent(const Callback& callback)
        {
            std::function<void(const Event&)> f = callback;

            return listenToEvent(f);
        }

        template <typename Event>
        EntityRef listenToEvent(const std::function<void(const Event&)>& callback)
        {
            auto ent = createEntity();

            ecsRef->attach<OnEventComponent>(ent, callback);

            return ent;
        }

        template <typename Callback>
        EntityRef listenToStandardEvent(const std::string& eventName, const Callback& callback)
        {
            std::function<void(const StandardEvent&)> f = callback;

            return listenToStandardEvent(eventName, f);
        }

        EntityRef listenToStandardEvent(const std::string& eventName, const std::function<void(const StandardEvent&)>& callback)
        {
            auto ent = createEntity();

            ecsRef->attach<OnStandardEventComponent>(ent, eventName, callback);

            return ent;
        }

        EntityRef createEntity();

        inline Entity* getEntity(_unique_id id) const { return ecsRef->getEntity(id); }

        template <typename Type, typename... Args>
        CompRef<Type> attach(EntityRef entity, Args&&... args) noexcept
        {
            return ecsRef->attach<Type>(entity, args...);
        }

        template <typename Type>
        Type* getSystem() const { return ecsRef->getSystem<Type>(); }

        EntitySystem *ecsRef;
    };

    struct SceneFile
    {
        SceneFile() {}
        SceneFile(const SceneFile& other) : filename(other.filename), version(other.version), onEnterScript(other.onEnterScript), onLeaveScript(other.onLeaveScript), entityList(other.entityList), subScenes(other.subScenes), originCoord(other.originCoord), instancedEntities(other.instancedEntities) {}

        SceneFile& operator=(const SceneFile& other)
        {
            filename = other.filename;
            version = other.version;
            onEnterScript = other.onEnterScript;
            onLeaveScript = other.onLeaveScript;
            entityList = other.entityList;
            subScenes = other.subScenes;
            originCoord = other.originCoord;
            instancedEntities = other.instancedEntities;

            return *this;
        }

        inline static std::string getType() { return "SaveData"; }

        std::string filename;
        std::string version = ARCHIVEVERSION;
        std::string onEnterScript;
        std::string onLeaveScript;
        std::vector<std::string> entityList;

        /** A subscene behave like a regular scene but it is instanciated in another main scene and is dependant of this scene
         If the main scene is unloaded it should unload all the subscene, but unloading a subscene only unload the entities and the subscene of the subscene */
        std::vector<SceneFile> subScenes;

        std::string originCoord; // Todo rename this as originViewpoint

        std::vector<EntityRef> instancedEntities;
    };

    template<>
    void serialize(Archive& archive, const SceneFile& config);

    template <>
    SceneFile deserialize(const UnserializedObject& serializedString);

    struct SceneElementSystem : public System<
        Listener<SceneElementClicked>, Listener<SaveScene>, Listener<LoadScene>, Listener<NameScene>,
        Own<SceneElement>, InitSys>
    {
        virtual std::string getSystemName() const override { return "Scene System"; }

        enum class LoadingState : uint8_t
        {
            Idle = 0,
            Parsing,
            OnLeave,
            EntityUnloading,
            EntityLoading,
            SubSceneLoading,
            OnEnter,
        };

        enum class SceneToLoadFlag : uint8_t
        {
            SystemScene = 0,
            FileScene,
            None
        };

        SceneElementSystem()
        {
            currentScene.filename = "newScene.sz";
        }

        virtual void init() override;

        virtual void onEvent(const SceneElementClicked& event) override
        {
            LOG_INFO("Scene Element", "Entity clicked: " << event.entity.id);
        }

        virtual void onEvent(const SaveScene& event) override;

        virtual void onEvent(const LoadScene& event) override;

        virtual void onEvent(const NameScene& event) override;

        template <typename Type, typename... Args>
        void loadSystemScene(const Args&... args)
        {
            if (not ecsRef)
                return;

            if (loadingNewScene)
                return;

            if (nextSystemScene)
                delete nextSystemScene;

            nextSystemScene = new Type(args...);

            sceneToLoadFlag = SceneToLoadFlag::SystemScene;
            loadingNewScene = true;
        }

        virtual void execute() override;

        void sceneDeserialization(SceneFile& sceneFile);

        void deserializeData(SceneFile& sceneFile, const std::unordered_map<std::string, std::string>& serializedData);

        // Todo
        // void translateEntitiesInScene(const SceneFile& subScene, const UiFrame& originFrame);

        void runEnterScript(const SceneFile& scene);

        void runLeaveScript(const SceneFile& scene);

        bool loadingNewScene = false;
        SceneToLoadFlag sceneToLoadFlag = SceneToLoadFlag::None;
        SceneToLoadFlag currentLoadedScene = SceneToLoadFlag::None;
        std::string sceneToLoad;

        LoadingState currentState = LoadingState::Idle;

        SceneFile currentScene;
        SceneFile nextScene;

        Scene* systemScene = nullptr;
        Scene* nextSystemScene = nullptr;

        bool namingSceneFlag = false;
        std::string newSceneName;

        Serializer serializer;
    };
}