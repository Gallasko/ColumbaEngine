#include "progressbar.h"

#include "Helpers/helpers.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Progress Bar";

        const static std::unordered_map<ProgressBarFillDirection, std::string> directionToString = {
            {ProgressBarFillDirection::Horizontal, "Horizontal"},
            {ProgressBarFillDirection::Vertical, "Vertical"},
            {ProgressBarFillDirection::Custom, "Custom"},
        };

        const static std::unordered_map<std::string, ProgressBarFillDirection> stringToDirection = invertMap(directionToString);
    }

    template <>
    void serialize(Archive& archive, const ProgressBarComponent& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization(ProgressBarComponent::getType());

        serialize(archive, "emptyTextureName", value.emptyTextureName);
        serialize(archive, "fullTextureName", value.fullTextureName);
        serialize(archive, "fillRatio", value.percent);
        serialize(archive, "fillDirection", directionToString.at(value.direction));
    
        archive.endSerialization();
    }

    template <>
    ProgressBarComponent deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an ProgressBarComponent");

            auto emptyTextureName = deserialize<std::string>(serializedString["emptyTextureName"]);
            auto fullTextureName = deserialize<std::string>(serializedString["fullTextureName"]);

            auto fillRatio = deserialize<float>(serializedString["fillRatio"]);

            auto direction = stringToDirection.at(deserialize<std::string>(serializedString["fillDirection"]));

            auto bar = ProgressBarComponent{emptyTextureName, fullTextureName, fillRatio};

            bar.direction = direction;

            return bar;
        }

        return ProgressBarComponent{"", ""};
    }

    void ProgressBarComponentSystem::init()
    {
        baseMaterialPreset.shader = masterRenderer->getShader("progressBar");

        baseMaterialPreset.nbTextures = 2;

        baseMaterialPreset.setSimpleMesh({3, 2, 1, 1, 1});

        baseMaterialPreset.uniformMap.emplace("sWidth", "ScreenWidth");
        baseMaterialPreset.uniformMap.emplace("sHeight", "ScreenHeight");

        auto group = registerGroup<PositionComponent, ProgressBarComponent>();

        group->addOnGroup([this](EntityRef entity) {
            LOG_MILE("Progress Bar System", "Add entity " << entity->id << " to ui - 2d shape group !");

            auto ui = entity->get<PositionComponent>();
            auto shape = entity->get<ProgressBarComponent>();

            ecsRef->attach<ProgressBarRenderCall>(entity, createRenderCall(ui, shape));

            changed = true;
        });

        group->removeOfGroup([this](EntitySystem* ecsRef, _unique_id id) {
            LOG_MILE("Progress Bar System", "Remove entity " << id << " of ui - 2d shape group !");

            auto entity = ecsRef->getEntity(id);

            ecsRef->detach<ProgressBarRenderCall>(entity);

            changed = true;
        });
    }

    void ProgressBarComponentSystem::execute()
    {
        if (not changed)
            return;

        renderCallList.clear();

        const auto& renderCallView = view<ProgressBarRenderCall>();

        renderCallList.reserve(renderCallView.nbComponents());

        for (const auto& renderCall : renderCallView)
        {
            renderCallList.push_back(renderCall->call);
        }

        changed = false;
    }

    RenderCall ProgressBarComponentSystem::createRenderCall(CompRef<PositionComponent> ui, CompRef<ProgressBarComponent> obj)
    {
        LOG_THIS_MEMBER(DOM);

        RenderCall call;

        call.processPositionComponent(ui);

        call.setRenderStage(renderStage);

        auto materialName = obj->emptyTextureName + obj->fullTextureName + "_bar";

        if (masterRenderer->hasMaterial(materialName))
        {
            call.setMaterial(masterRenderer->getMaterialID(materialName));
        }
        else
        {
            Material simpleShapeMaterial = baseMaterialPreset;

            simpleShapeMaterial.textureId[0] = masterRenderer->getTexture(obj->emptyTextureName).id;
            simpleShapeMaterial.textureId[1] = masterRenderer->getTexture(obj->fullTextureName).id;

            call.setMaterial(masterRenderer->registerMaterial(materialName, simpleShapeMaterial));
        }

        call.setOpacity(OpacityType::Additive);

        call.data.resize(8);

        call.data[0] = ui->x;
        call.data[1] = ui->y;
        call.data[2] = ui->z;
        call.data[3] = ui->width;
        call.data[4] = ui->height;
        call.data[5] = ui->rotation;
        call.data[6] = obj->percent;
        call.data[7] = static_cast<float>(obj->direction);

        return call;
    }

    void ProgressBarComponentSystem::onEvent(const EntityChangedEvent& event)
    {
        LOG_THIS_MEMBER(DOM);

        onEventUpdate(event.id);
    }

    void ProgressBarComponentSystem::onEventUpdate(_unique_id entityId)
    {
        LOG_THIS_MEMBER(DOM);

        auto entity = ecsRef->getEntity(entityId);
        
        if (not entity or not entity->has<ProgressBarRenderCall>())
            return; 

        auto ui = entity->get<PositionComponent>();
        auto shape = entity->get<ProgressBarComponent>();

        entity->get<ProgressBarRenderCall>()->call = createRenderCall(ui, shape);

        changed = true;
    }
}