#include "scenemanager.h"

#include <sstream>

#include "Files/fileparser.h"

#include "Interpreter/pginterpreter.h"

#include "Renderer/renderer.h"

#include "Systems/coresystems.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Scene Element System";

        static constexpr char const * NONESCRIPT = "#__PGNone";
        // static constexpr char const * ONENTERSCRIPT = "#__PGEnterScene";
        // static constexpr char const * ONLEAVESCRIPT = "#__PGLeaveScene";
        // static constexpr char const * NEWSCENEENTITY = "#__PGEntityBegin";
        // static constexpr char const * ENDSCENEENTITY = "#__PGEntityEnd";
        // static constexpr char const * BEGINSUBSCENE = "#__PGSubSceneBegin";
        // static constexpr char const * ORIGINSUBSCENE = "#__PGSubSceneOrigin";
        // static constexpr char const * ENDSUBSCENE = "#__PGSubSceneEnd";
    }

    std::vector<float> splitFloats(const std::string &s, char delim)
    {
        std::vector<float> result;
        std::stringstream ss (s);
        std::string item;

        while (getline (ss, item, delim))
        {
            try
            {
                result.push_back(std::stof(item));
            }
            catch (const std::exception& e)
            {
                LOG_ERROR(DOM, "Invalid base string doesn't contain only floats: " << e.what());
            }
        }

        return result;
    }

    /**
     * @brief A function that trim all whitespace characters from the beginning of a string
     *
     * @param str The string to trim
     * @param whitespace Characters used as whitespace characters
     *
     * @return std::string The resulting string trimmed of the whitespace characters
     */
    std::string trim(const std::string& str, const std::string& whitespace = " \t")
    {
        LOG_THIS(DOM);

        const auto strBegin = str.find_first_not_of(whitespace);
        if (strBegin == std::string::npos)
            return ""; // no content

        const auto strEnd = str.find_last_not_of(whitespace);
        const auto strRange = strEnd - strBegin + 1;

        return str.substr(strBegin, strRange);
    }

    template <>
    void serialize(Archive& archive, const SceneFile& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization(SceneFile::getType());

        serialize(archive, "onEnterScript", value.onEnterScript);
        serialize(archive, "onLeaveScript", value.onLeaveScript);

        serialize(archive, "nbEntities", value.instancedEntities.size());

        size_t i = 0;

        for (const auto& entity : value.instancedEntities)
        {
            auto str = std::to_string(i);

            serialize(archive, "entity" + str, *entity.entity);

            ++i;
        }

        serialize(archive, "nbSubscenes", value.subScenes.size());

        i = 0;

        for (const auto& subscene : value.subScenes)
        {
            auto str = std::to_string(i);

            serialize(archive, "subscene" + str, subscene.filename);
            serialize(archive, "subsceneOrigin" + str, subscene.originCoord);

            ++i;
        }

        archive.endSerialization();
    }

    template <>
    SceneFile deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing Scene File");

            SceneFile data;

            data.onEnterScript = deserialize<std::string>(serializedString["onEnterScript"]);
            data.onLeaveScript = deserialize<std::string>(serializedString["onLeaveScript"]);

            auto nbEntities = deserialize<size_t>(serializedString["nbEntities"]);

            for (size_t i = 0; i < nbEntities; ++i)
            {
                auto str = std::to_string(i);

                auto dataStr = serializedString["entity" + str].getString();

                // Todo doc this, we need to remove the last character if it is a ',' as the serializer expect '}' to end the class and not '},'
                if (dataStr.back() == '\n')
                {
                    dataStr.pop_back();
                    if (dataStr.back() == ',')
                    {
                        dataStr.pop_back();
                    }
                }

                LOG_INFO(DOM, "Entity serialized: " << dataStr);

                data.entityList.push_back(dataStr);
            }

            auto nbSubscenes = deserialize<size_t>(serializedString["nbSubscenes"]);

            for (size_t i = 0; i < nbSubscenes; ++i)
            {
                auto str = std::to_string(i);

                SceneFile subscene;

                subscene.filename = deserialize<std::string>(serializedString["subscene" + str]);
                subscene.originCoord = deserialize<std::string>(serializedString["subsceneOrigin" + str]);

                data.subScenes.push_back(subscene);
            }

            return data;
        }

        return SceneFile{};
    }

    class GetCurrentScene : public Function
    {
        using Function::Function;
    public:
        void setUp(const SceneFile& sceneFile)
        {
            LOG_THIS_MEMBER(DOM);

            setArity(0, 0);

            this->sceneFile = sceneFile;
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            LOG_THIS_MEMBER(DOM);

            auto entityList = makeList(this, {});

            auto subsceneList = makeList(this, {});

            auto list = makeList(this, {{"name", sceneFile.filename}});

            for (const auto& ent : sceneFile.instancedEntities)
            {
                if (ent.has<EntityName>())
                {
                    addToList(entityList, this->token, {std::to_string(ent.id), ent.get<EntityName>()->name});
                }
                else
                {
                    addToList(entityList, this->token, {std::to_string(ent.id), "UnNamed"});
                }
            }

            addToList(list, this->token, {"entities", entityList});

            unsigned int i = 0;
            for (const auto& subscene : sceneFile.subScenes)
            {
                addToList(subsceneList, this->token, {std::to_string(i), subscene.filename});

                ++i;
            }

            addToList(list, this->token, {"subscenes", subsceneList});

            return list;
        }

        SceneFile sceneFile;
    };

    EntityRef Scene::createEntity()
    {
        auto entity = ecsRef->createEntity();

        ecsRef->attach<SceneElement>(entity);

        return entity;
    }

    SceneFile parseSceneFile(const std::string& filepath)
    {
        LOG_MILE(DOM, "Parse scene to load: " << filepath);

        Serializer sceneLoader(false);

        sceneLoader.setFile(filepath);

        SceneFile scene = sceneLoader.deserializeObject<SceneFile>("scenedata");

        for (auto& subscene : scene.subScenes)
        {
            auto subSceneFile = parseSceneFile(subscene.filename);

            subSceneFile.originCoord = subscene.originCoord;

            subscene = subSceneFile;
        }

        return scene;
    }

    void SceneElementSystem::init()
    {
        // auto group = registerGroup<UiComponent, SceneElement>();

        // group->addOnGroup([](EntityRef entity) {
        //     LOG_MILE("Scene Element System", "Add entity " << entity->id << " to ui - scene group !");

        //     auto entityUiC = entity->get<UiComponent>();

        //     auto overlappingEntity = entity->world()->createEntity();

        //     auto uiComp = entity->world()->attach<UiComponent>(overlappingEntity);

        //     uiComp->fill(entityUiC);

        //     uiComp->setZ(entityUiC->pos.z + 1);

        //     entity->world()->attach<MouseLeftClickComponent>(overlappingEntity, makeCallable<SceneElementClicked>(entity));
        // });
    }

    void SceneElementSystem::onEvent(const SaveScene& event)
    {
        // If this is true then we made a ctrl shift s (save as)
        if (event.otherNameForScene != "")
        {
            currentScene.filename = event.otherNameForScene;
        }

        LOG_INFO("Scene Element", "Save current scene named: " << currentScene.filename);

        serializer.setFile(currentScene.filename);

        serializer.clear();

        currentScene.instancedEntities.clear();

        for (auto entity : view<SceneElement>())
        {
            currentScene.instancedEntities.push_back(entity->entity);
        }

        serializer.serializeObject("scenedata", currentScene);
    }

    void SceneElementSystem::onEvent(const LoadScene& event)
    {
        LOG_THIS_MEMBER(DOM);

        if (not ecsRef)
            return;

        sceneToLoad = event.filename;
        loadingNewScene = true;
        sceneToLoadFlag = SceneToLoadFlag::FileScene;
    }

    void SceneElementSystem::onEvent(const NameScene& event)
    {
        LOG_INFO("Scene Element", "Name the current scene: " << event.filename);

        if (not ecsRef)
            return;

        newSceneName = event.filename;
        namingSceneFlag = true;
    }

    void SceneElementSystem::execute()
    {
        if (currentState != LoadingState::Idle)
        {
            LOG_INFO(DOM, "Current loading state: " << static_cast<uint8_t>(currentState));
        }

        // Todo do this for file scene too...
        if (currentState == LoadingState::Idle and currentLoadedScene == SceneToLoadFlag::SystemScene)
        {
            systemScene->execute();
        }

        if (sceneToLoadFlag == SceneToLoadFlag::FileScene and currentState == LoadingState::OnEnter)
        {
            runEnterScript(nextScene);

            currentScene = nextScene;

            currentLoadedScene = SceneToLoadFlag::FileScene;

            loadingNewScene = false;

            currentState = LoadingState::Idle;

            ecsRef->sendEvent(NewSceneLoaded{});

            LOG_INFO(DOM, "Scene loaded");
        }
        else if (sceneToLoadFlag == SceneToLoadFlag::SystemScene and currentState == LoadingState::OnEnter)
        {
            nextSystemScene->startUp();

            if (systemScene)
            {
                delete systemScene;
                systemScene = nullptr;
            }

            systemScene = nextSystemScene;

            nextSystemScene = nullptr;

            currentLoadedScene = SceneToLoadFlag::SystemScene;

            loadingNewScene = false;

            currentState = LoadingState::Idle;

            ecsRef->sendEvent(NewSceneLoaded{});

            LOG_INFO(DOM, "Scene loaded");
        }
        else if (sceneToLoadFlag == SceneToLoadFlag::FileScene and currentState == LoadingState::SubSceneLoading)
        {
            // Todo upgrade this !
            // auto mainWindowEnt = ecsRef->getEntity(ecsRef->getSystem<EntityNameSystem>()->getEntityId("__MainWindow"));

            // auto ui = mainWindowEnt->get<UiComponent>();

            // Todo
            // for (const auto& subScene : nextScene.subScenes)
            // {
            //     translateEntitiesInScene(subScene, ui->frame);
            // }

            currentState = LoadingState::OnEnter;
        }
        else if (sceneToLoadFlag == SceneToLoadFlag::FileScene and currentState == LoadingState::EntityLoading)
        {
            if (systemScene)
            {
                delete systemScene;
                systemScene = nullptr;
            }

            sceneDeserialization(nextScene);

            // If some subscene are present we skip another render pass to avoid jittering when the subscenes are being relocated in the frame
            if (not nextScene.subScenes.empty())
            {
                ecsRef->sendEvent(SkipRenderPass{});
            }

            currentState = LoadingState::SubSceneLoading;
        }
        else if (sceneToLoadFlag == SceneToLoadFlag::SystemScene and currentState == LoadingState::EntityLoading)
        {
            // If some subscene are present we skip another render pass to avoid jittering when the subscenes are being relocated in the frame
            nextSystemScene->init();

            currentState = LoadingState::OnEnter;
        }
        else if (currentState == LoadingState::EntityUnloading)
        {
            const auto& sceneView = view<SceneElement>();

            LOG_INFO(DOM, "Unloading " << sceneView.nbComponents() << " entities");

            for (const auto& comp : sceneView)
            {
                ecsRef->removeEntity(comp->entity);
            }

            ecsRef->sendEvent(SkipRenderPass{});

            currentState = LoadingState::EntityLoading;
        }
        else if (currentLoadedScene == SceneToLoadFlag::FileScene and currentState == LoadingState::OnLeave)
        {
            runLeaveScript(currentScene);

            currentState = LoadingState::EntityUnloading;
        }
        else if (currentLoadedScene == SceneToLoadFlag::SystemScene and currentState == LoadingState::OnLeave)
        {
            if (systemScene)
            {
                systemScene->onLeave();
            }

            currentState = LoadingState::EntityUnloading;
        }
        else if (currentLoadedScene == SceneToLoadFlag::None and currentState == LoadingState::OnLeave)
        {
            LOG_INFO(DOM, "Starting up of the scene system, no scene to unload. Skiping this state !");

            currentState = LoadingState::EntityUnloading;
        }
        else if (sceneToLoadFlag == SceneToLoadFlag::FileScene and currentState == LoadingState::Idle and loadingNewScene)
        {
            LOG_INFO(DOM, "Loading a new scene: " << sceneToLoad);

            currentState = LoadingState::Parsing;

            nextScene = parseSceneFile(sceneToLoad);

            currentState = LoadingState::OnLeave;

            loadingNewScene = false;
        }
        else if (sceneToLoadFlag == SceneToLoadFlag::SystemScene and currentState == LoadingState::Idle and loadingNewScene)
        {
            LOG_INFO(DOM, "Loading a new system scene");

            currentState = LoadingState::Parsing;

            if (nextSystemScene)
            {
                nextSystemScene->ecsRef = ecsRef;
            }

            currentState = LoadingState::OnLeave;
        }
        else if (currentState == LoadingState::Idle and namingSceneFlag)
        {
            namingSceneFlag = false;

            if (not (currentLoadedScene == SceneToLoadFlag::None) or not (currentLoadedScene == SceneToLoadFlag::FileScene))
            {
                LOG_ERROR(DOM, "Rename are only possible on file scenes !");
                return;
            }

            // Todo delete old file if it exists, after the rename is done !

            currentScene.filename = newSceneName;
        }
    }

    void SceneElementSystem::sceneDeserialization(SceneFile& sceneFile)
    {
        LOG_THIS_MEMBER(DOM);

        for (const auto& data : sceneFile.entityList)
        {
            LOG_INFO(DOM, "Entity data: " << data);

            // Todo remove the 1, it means that the indent is of 1 because of how we deserialize the scene, it will become obsolete when serializer don't need indent
            auto serializedData = Serializer::readData(sceneFile.version, data, 1);

            deserializeData(sceneFile, serializedData);
        }

        for (auto& subScene : sceneFile.subScenes)
        {
            sceneDeserialization(subScene);
        }
    }

    void SceneElementSystem::deserializeData(SceneFile& sceneFile, const std::unordered_map<std::string, std::string>& serializedData)
    {
        LOG_THIS_MEMBER(DOM);

        std::unordered_map<_unique_id, _unique_id> idCorrelationMap;

        LOG_INFO(DOM, "Serialized size: " << serializedData.size());

        for (const auto& elem : serializedData)
        {
            LOG_INFO(DOM, elem.second << " --- " << elem.first);
            UnserializedObject serializedString(elem.second, elem.first);

            auto newEntity = ecsRef->createEntity();

            sceneFile.instancedEntities.push_back(newEntity);

            ecsRef->attach<SceneElement>(newEntity);

            _unique_id oldId;
            std::istringstream iss(elem.first);
            iss >> oldId;

            idCorrelationMap[oldId] = newEntity.id;

            if (serializedString.isNull())
            {
                LOG_ERROR(DOM, "Element is null");
            }
            else
            {
                // Todo Loop over those ref id and correlate them to the correlation map and push them in the entity
                // auto nbRefId = deserialize<size_t>(serializedString["nbRefId"]);

                for (const auto& childStr : serializedString.children)
                {
                    const auto& objType = childStr.getObjectType();

                    if (objType.find("idRef") != std::string::npos)
                    {
                        // Todo
                    }
                    else
                    {
                        // Filter any attributes
                        if (childStr.isClassObject())
                            ecsRef->deserializeComponent(newEntity, childStr);
                    }
                }
            }
        }
    }

    // void SceneElementSystem::translateEntitiesInScene(const SceneFile& subScene, const UiFrame& oriFrame)
    // {
    //     LOG_THIS_MEMBER(DOM);
    //     // UiFrame originFrame = oriFrame;

    //     UiFrame originFrame;
    //     originFrame.pos.x = static_cast<float>(oriFrame.pos.x);
    //     originFrame.pos.y = static_cast<float>(oriFrame.pos.y);

    //     std::string coord = subScene.originCoord;

    //     // Todo
    //     if (coord != "")
    //     {
    //         if (coord.find("Entity") != coord.npos)
    //         {
    //             LOG_ERROR(DOM, "Not supported yet !");
    //         }
    //         else
    //         {
    //             auto coordSplit = splitFloats(coord, ',');

    //             if (coordSplit.size() != 5)
    //             {
    //                 LOG_ERROR(DOM, "Invalid coordinates given, current size: " << coordSplit.size());
    //             }
    //             else
    //             {
    //                 originFrame.pos.x += coordSplit[0];
    //                 originFrame.pos.y += coordSplit[1];
    //                 originFrame.pos.z += coordSplit[2];
    //                 originFrame.w += coordSplit[3];
    //                 originFrame.h += coordSplit[4];
    //             }
    //         }
    //     }

    //     for (auto entity : subScene.instancedEntities)
    //     {
    //         if (entity.has<UiComponent>())
    //         {
    //             auto ui = entity.get<UiComponent>();

    //             // Todo fix this for full anchored component and width/height anchor...
    //             // Todo this can be easily fix if all UiComponent have a parent and the base parent for everyone is the window
    //             // And so this can be just view as changing the window parent to our origin point !

    //             ui->pos.x += originFrame.pos.x;
    //             ui->pos.y += originFrame.pos.y;
    //             // ui->pos.z += originFrame.pos.z;

    //             ui->hasTopAnchor = false;
    //             ui->hasRightAnchor = false;
    //             ui->hasBottomAnchor = false;
    //             ui->hasLeftAnchor = false;

    //             ui->update();
    //         }
    //     }

    //     // Recursively translate all sub subscenes with the new origin point !
    //     for (const auto& subscene : subScene.subScenes)
    //     {
    //         translateEntitiesInScene(subscene, originFrame);
    //     }
    // }

    void SceneElementSystem::runEnterScript(const SceneFile& scene)
    {
        LOG_THIS_MEMBER(DOM);

        if (scene.onEnterScript != "" and scene.onEnterScript != NONESCRIPT)
        {
            CustomSysFunctions function;
            function.addSystemFunction<GetCurrentScene>("getCurrentScene", scene);

            ecsRef->sendEvent(ExecuteFileScriptEvent{scene.onEnterScript, function});
        }

        for (const auto& subscene : scene.subScenes)
        {
            runEnterScript(subscene);
        }
    }

    void SceneElementSystem::runLeaveScript(const SceneFile& scene)
    {
        LOG_THIS_MEMBER(DOM);

        for (const auto& subscene : scene.subScenes)
        {
            runLeaveScript(subscene);
        }

        if (scene.onLeaveScript != "" and scene.onLeaveScript != NONESCRIPT)
        {
            CustomSysFunctions function;
            function.addSystemFunction<GetCurrentScene>("getCurrentScene", scene);

            ecsRef->sendEvent(ExecuteFileScriptEvent{scene.onLeaveScript, function});
        }
    }

}