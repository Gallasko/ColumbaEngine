#include "stdafx.h"

#include "namedanchor.h"

#include "ECS/entitysystem.h"
#include "Systems/coresystems.h"

namespace pg
{
    namespace
    {
        static constexpr const char* const DOM = "NamedUiAnchor";
    }
    
    // NamedUiAnchor implementation
    void NamedUiAnchor::onCreation(EntityRef entity)
    {
        ecsRef = entity->world();
        entityId = entity->id;
        
        // Todo
        // Ensure the entity has a UiAnchor component
        // if (!entity->has<UiAnchor>())
        // {
        //     ecsRef->attach<UiAnchor>(entity);
        // }
        
        // Trigger resolution of named anchors
        ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setTopAnchor(const std::string& entityName, AnchorType type)
    {
        topAnchorName = entityName;
        topAnchorType = type;
        hasTopAnchor = true;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setLeftAnchor(const std::string& entityName, AnchorType type)
    {
        leftAnchorName = entityName;
        leftAnchorType = type;
        hasLeftAnchor = true;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setRightAnchor(const std::string& entityName, AnchorType type)
    {
        rightAnchorName = entityName;
        rightAnchorType = type;
        hasRightAnchor = true;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setBottomAnchor(const std::string& entityName, AnchorType type)
    {
        bottomAnchorName = entityName;
        bottomAnchorType = type;
        hasBottomAnchor = true;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setVerticalCenter(const std::string& entityName, AnchorType type)
    {
        verticalCenterAnchorName = entityName;
        verticalCenterAnchorType = type;
        hasVerticalCenter = true;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setHorizontalCenter(const std::string& entityName, AnchorType type)
    {
        horizontalCenterAnchorName = entityName;
        horizontalCenterAnchorType = type;
        hasHorizontalCenter = true;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::clearTopAnchor()
    {
        hasTopAnchor = false;
        topAnchorName.clear();
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::clearLeftAnchor()
    {
        hasLeftAnchor = false;
        leftAnchorName.clear();
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::clearRightAnchor()
    {
        hasRightAnchor = false;
        rightAnchorName.clear();
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::clearBottomAnchor()
    {
        hasBottomAnchor = false;
        bottomAnchorName.clear();
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::clearVerticalCenter()
    {
        hasVerticalCenter = false;
        verticalCenterAnchorName.clear();
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::clearHorizontalCenter()
    {
        hasHorizontalCenter = false;
        horizontalCenterAnchorName.clear();
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::clearAnchors()
    {
        clearTopAnchor();
        clearLeftAnchor();
        clearRightAnchor();
        clearBottomAnchor();
        clearVerticalCenter();
        clearHorizontalCenter();
    }
    
    void NamedUiAnchor::fillIn(const std::string& entityName)
    {
        setTopAnchor(entityName, AnchorType::Top);
        setLeftAnchor(entityName, AnchorType::Left);
        setRightAnchor(entityName, AnchorType::Right);
        setBottomAnchor(entityName, AnchorType::Bottom);
    }
    
    void NamedUiAnchor::centeredIn(const std::string& entityName)
    {
        setVerticalCenter(entityName, AnchorType::VerticalCenter);
        setHorizontalCenter(entityName, AnchorType::HorizontalCenter);
    }
    
    void NamedUiAnchor::setTopMargin(float value)
    {
        topMargin = value;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setLeftMargin(float value)
    {
        leftMargin = value;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setRightMargin(float value)
    {
        rightMargin = value;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setBottomMargin(float value)
    {
        bottomMargin = value;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setWidthConstrain(const std::string& entityName, AnchorType type, PosOpType opType, float opValue)
    {
        widthConstrainName = entityName;
        widthConstrainType = type;
        widthConstrainOpType = opType;
        widthConstrainOpValue = opValue;
        hasWidthConstrain = true;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setHeightConstrain(const std::string& entityName, AnchorType type, PosOpType opType, float opValue)
    {
        heightConstrainName = entityName;
        heightConstrainType = type;
        heightConstrainOpType = opType;
        heightConstrainOpValue = opValue;
        hasHeightConstrain = true;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    void NamedUiAnchor::setZConstrain(const std::string& entityName, AnchorType type, PosOpType opType, float opValue)
    {
        zConstrainName = entityName;
        zConstrainType = type;
        zConstrainOpType = opType;
        zConstrainOpValue = opValue;
        hasZConstrain = true;
        
        if (ecsRef)
            ecsRef->sendEvent(ResolveNamedAnchorsEvent{entityId});
    }
    
    // Serialization functions
    template <>
    void serialize<>(Archive& archive, const NamedUiAnchor& value)
    {
        archive.startSerialization("NamedUiAnchor");
        
        // Serialize anchor names and flags
        serialize(archive, "topAnchorName", value.topAnchorName);
        serialize(archive, "leftAnchorName", value.leftAnchorName);
        serialize(archive, "rightAnchorName", value.rightAnchorName);
        serialize(archive, "bottomAnchorName", value.bottomAnchorName);
        serialize(archive, "verticalCenterAnchorName", value.verticalCenterAnchorName);
        serialize(archive, "horizontalCenterAnchorName", value.horizontalCenterAnchorName);
        
        serialize(archive, "hasTopAnchor", value.hasTopAnchor);
        serialize(archive, "hasLeftAnchor", value.hasLeftAnchor);
        serialize(archive, "hasRightAnchor", value.hasRightAnchor);
        serialize(archive, "hasBottomAnchor", value.hasBottomAnchor);
        serialize(archive, "hasVerticalCenter", value.hasVerticalCenter);
        serialize(archive, "hasHorizontalCenter", value.hasHorizontalCenter);
        
        // Serialize anchor types
        serialize(archive, "topAnchorType", AnchorTypeToStringMap.at(value.topAnchorType));
        serialize(archive, "leftAnchorType", AnchorTypeToStringMap.at(value.leftAnchorType));
        serialize(archive, "rightAnchorType", AnchorTypeToStringMap.at(value.rightAnchorType));
        serialize(archive, "bottomAnchorType", AnchorTypeToStringMap.at(value.bottomAnchorType));
        serialize(archive, "verticalCenterAnchorType", AnchorTypeToStringMap.at(value.verticalCenterAnchorType));
        serialize(archive, "horizontalCenterAnchorType", AnchorTypeToStringMap.at(value.horizontalCenterAnchorType));
        
        // Serialize margins
        serialize(archive, "topMargin", value.topMargin);
        serialize(archive, "leftMargin", value.leftMargin);
        serialize(archive, "rightMargin", value.rightMargin);
        serialize(archive, "bottomMargin", value.bottomMargin);
        
        // Serialize constraints
        serialize(archive, "widthConstrainName", value.widthConstrainName);
        serialize(archive, "heightConstrainName", value.heightConstrainName);
        serialize(archive, "zConstrainName", value.zConstrainName);
        
        serialize(archive, "hasWidthConstrain", value.hasWidthConstrain);
        serialize(archive, "hasHeightConstrain", value.hasHeightConstrain);
        serialize(archive, "hasZConstrain", value.hasZConstrain);
        
        serialize(archive, "widthConstrainType", AnchorTypeToStringMap.at(value.widthConstrainType));
        serialize(archive, "heightConstrainType", AnchorTypeToStringMap.at(value.heightConstrainType));
        serialize(archive, "zConstrainType", AnchorTypeToStringMap.at(value.zConstrainType));
        
        serialize(archive, "widthConstrainOpType", PosOpTypeToStringMap.at(value.widthConstrainOpType));
        serialize(archive, "heightConstrainOpType", PosOpTypeToStringMap.at(value.heightConstrainOpType));
        serialize(archive, "zConstrainOpType", PosOpTypeToStringMap.at(value.zConstrainOpType));
        
        serialize(archive, "widthConstrainOpValue", value.widthConstrainOpValue);
        serialize(archive, "heightConstrainOpValue", value.heightConstrainOpValue);
        serialize(archive, "zConstrainOpValue", value.zConstrainOpValue);
        
        archive.endSerialization();
    }
    
    template <>
    NamedUiAnchor deserialize(const UnserializedObject& serializedString)
    {
        NamedUiAnchor data;
        
        // Deserialize anchor names and flags
        defaultDeserialize(serializedString, "topAnchorName", data.topAnchorName);
        defaultDeserialize(serializedString, "leftAnchorName", data.leftAnchorName);
        defaultDeserialize(serializedString, "rightAnchorName", data.rightAnchorName);
        defaultDeserialize(serializedString, "bottomAnchorName", data.bottomAnchorName);
        defaultDeserialize(serializedString, "verticalCenterAnchorName", data.verticalCenterAnchorName);
        defaultDeserialize(serializedString, "horizontalCenterAnchorName", data.horizontalCenterAnchorName);
        
        defaultDeserialize(serializedString, "hasTopAnchor", data.hasTopAnchor);
        defaultDeserialize(serializedString, "hasLeftAnchor", data.hasLeftAnchor);
        defaultDeserialize(serializedString, "hasRightAnchor", data.hasRightAnchor);
        defaultDeserialize(serializedString, "hasBottomAnchor", data.hasBottomAnchor);
        defaultDeserialize(serializedString, "hasVerticalCenter", data.hasVerticalCenter);
        defaultDeserialize(serializedString, "hasHorizontalCenter", data.hasHorizontalCenter);
        
        // Deserialize anchor types
        std::string typeStr;
        defaultDeserialize(serializedString, "topAnchorType", typeStr);
        data.topAnchorType = StringToAnchorTypeMap.at(typeStr);
        
        defaultDeserialize(serializedString, "leftAnchorType", typeStr);
        data.leftAnchorType = StringToAnchorTypeMap.at(typeStr);
        
        defaultDeserialize(serializedString, "rightAnchorType", typeStr);
        data.rightAnchorType = StringToAnchorTypeMap.at(typeStr);
        
        defaultDeserialize(serializedString, "bottomAnchorType", typeStr);
        data.bottomAnchorType = StringToAnchorTypeMap.at(typeStr);
        
        defaultDeserialize(serializedString, "verticalCenterAnchorType", typeStr);
        data.verticalCenterAnchorType = StringToAnchorTypeMap.at(typeStr);
        
        defaultDeserialize(serializedString, "horizontalCenterAnchorType", typeStr);
        data.horizontalCenterAnchorType = StringToAnchorTypeMap.at(typeStr);
        
        // Deserialize margins
        defaultDeserialize(serializedString, "topMargin", data.topMargin);
        defaultDeserialize(serializedString, "leftMargin", data.leftMargin);
        defaultDeserialize(serializedString, "rightMargin", data.rightMargin);
        defaultDeserialize(serializedString, "bottomMargin", data.bottomMargin);
        
        // Deserialize constraints
        defaultDeserialize(serializedString, "widthConstrainName", data.widthConstrainName);
        defaultDeserialize(serializedString, "heightConstrainName", data.heightConstrainName);
        defaultDeserialize(serializedString, "zConstrainName", data.zConstrainName);
        
        defaultDeserialize(serializedString, "hasWidthConstrain", data.hasWidthConstrain);
        defaultDeserialize(serializedString, "hasHeightConstrain", data.hasHeightConstrain);
        defaultDeserialize(serializedString, "hasZConstrain", data.hasZConstrain);
        
        defaultDeserialize(serializedString, "widthConstrainType", typeStr);
        data.widthConstrainType = StringToAnchorTypeMap.at(typeStr);
        
        defaultDeserialize(serializedString, "heightConstrainType", typeStr);
        data.heightConstrainType = StringToAnchorTypeMap.at(typeStr);
        
        defaultDeserialize(serializedString, "zConstrainType", typeStr);
        data.zConstrainType = StringToAnchorTypeMap.at(typeStr);
        
        std::string opTypeStr;
        defaultDeserialize(serializedString, "widthConstrainOpType", opTypeStr);
        data.widthConstrainOpType = StringToPosOpTypeMap.at(opTypeStr);
        
        defaultDeserialize(serializedString, "heightConstrainOpType", opTypeStr);
        data.heightConstrainOpType = StringToPosOpTypeMap.at(opTypeStr);
        
        defaultDeserialize(serializedString, "zConstrainOpType", opTypeStr);
        data.zConstrainOpType = StringToPosOpTypeMap.at(opTypeStr);
        
        defaultDeserialize(serializedString, "widthConstrainOpValue", data.widthConstrainOpValue);
        defaultDeserialize(serializedString, "heightConstrainOpValue", data.heightConstrainOpValue);
        defaultDeserialize(serializedString, "zConstrainOpValue", data.zConstrainOpValue);
        
        return data;
    }
    
    // NamedUiAnchorSystem implementation
    void NamedUiAnchorSystem::init()
    {
        LOG_THIS_MEMBER(DOM);
    }
    
    void NamedUiAnchorSystem::onEvent(const EntityChangedEvent& event)
    {
        // Check if this entity has a NamedUiAnchor component and needs resolution
        // auto entity = ecsRef->getEntity(event.id);
        // if (entity && entity->has<NamedUiAnchor>())
        // {
        //     pendingResolution.insert(event.id);
        // }
    }
    
    void NamedUiAnchorSystem::onEvent(const ResolveNamedAnchorsEvent& event)
    {
        pendingResolution.insert(event.entityId);
    }
    
    void NamedUiAnchorSystem::execute()
    {
        // Process all pending resolutions
        for (auto entityId : pendingResolution)
        {
            resolveNamedAnchor(entityId);
        }
        pendingResolution.clear();
    }
    
    void NamedUiAnchorSystem::resolveNamedAnchor(_unique_id entityId)
    {
        auto entity = ecsRef->getEntity(entityId);
        if (not entity or not entity->has<NamedUiAnchor>() or not entity->has<UiAnchor>())
            return;
            
        auto namedAnchor = entity->get<NamedUiAnchor>();
        
        auto uiAnchor = entity->get<UiAnchor>();
        
        // Clear existing anchors first
        // uiAnchor->clearAnchors();

        // Resolve and set anchors
        // Resolve and set anchors
        struct AnchorInfo 
        {
            bool hasAnchor;
            std::string name;
            AnchorType type;
            void (UiAnchor::*setFunc)(const PosAnchor&);
            void (UiAnchor::*clearFunc)();
        };

        AnchorInfo anchors[] = {
            {namedAnchor->hasTopAnchor, namedAnchor->topAnchorName, namedAnchor->topAnchorType, &UiAnchor::setTopAnchor, &UiAnchor::clearTopAnchor},
            {namedAnchor->hasLeftAnchor, namedAnchor->leftAnchorName, namedAnchor->leftAnchorType, &UiAnchor::setLeftAnchor, &UiAnchor::clearLeftAnchor},
            {namedAnchor->hasRightAnchor, namedAnchor->rightAnchorName, namedAnchor->rightAnchorType, &UiAnchor::setRightAnchor, &UiAnchor::clearRightAnchor},
            {namedAnchor->hasBottomAnchor, namedAnchor->bottomAnchorName, namedAnchor->bottomAnchorType, &UiAnchor::setBottomAnchor, &UiAnchor::clearBottomAnchor},
            {namedAnchor->hasVerticalCenter, namedAnchor->verticalCenterAnchorName, namedAnchor->verticalCenterAnchorType, &UiAnchor::setVerticalCenter, &UiAnchor::clearVerticalCenter},
            {namedAnchor->hasHorizontalCenter, namedAnchor->horizontalCenterAnchorName, namedAnchor->horizontalCenterAnchorType, &UiAnchor::setHorizontalCenter, &UiAnchor::clearHorizontalCenter}
        };

        for (const auto& anchor : anchors)
        {
            _unique_id targetId = getEntityIdByName(anchor.name);
            if (targetId != 0)
                (uiAnchor->*(anchor.setFunc))(PosAnchor{targetId, anchor.type});
            // else
            //     (uiAnchor->*(anchor.clearFunc))();
        }
        
        // Set margins
        uiAnchor->setTopMargin(namedAnchor->topMargin);
        uiAnchor->setLeftMargin(namedAnchor->leftMargin);
        uiAnchor->setRightMargin(namedAnchor->rightMargin);
        uiAnchor->setBottomMargin(namedAnchor->bottomMargin);
        
        // Set constraints
        if (namedAnchor->hasWidthConstrain)
        {
            auto targetId = getEntityIdByName(namedAnchor->widthConstrainName);
            if (targetId != 0)
            {
                uiAnchor->setWidthConstrain(PosConstrain{
                    targetId, 
                    namedAnchor->widthConstrainType, 
                    namedAnchor->widthConstrainOpType, 
                    namedAnchor->widthConstrainOpValue
                });
            }
        }
        
        if (namedAnchor->hasHeightConstrain)
        {
            auto targetId = getEntityIdByName(namedAnchor->heightConstrainName);
            if (targetId != 0)
            {
                uiAnchor->setHeightConstrain(PosConstrain{
                    targetId, 
                    namedAnchor->heightConstrainType, 
                    namedAnchor->heightConstrainOpType, 
                    namedAnchor->heightConstrainOpValue
                });
            }
        }
        
        if (namedAnchor->hasZConstrain)
        {
            auto targetId = getEntityIdByName(namedAnchor->zConstrainName);
            if (targetId != 0)
            {
                uiAnchor->setZConstrain(PosConstrain{
                    targetId, 
                    namedAnchor->zConstrainType, 
                    namedAnchor->zConstrainOpType, 
                    namedAnchor->zConstrainOpValue
                });
            }
        }
    }
    
    _unique_id NamedUiAnchorSystem::getEntityIdByName(const std::string& name) const
    {
        if (name.empty())
            return 0;
            
        // Get the EntityNameSystem to resolve the name
        auto nameSystem = ecsRef->getSystem<EntityNameSystem>();
        if (nameSystem)
        {
            return nameSystem->getEntityId(name);
        }
        
        LOG_ERROR(DOM, "EntityNameSystem not found, cannot resolve entity name: " << name);
        return 0;
    }
}