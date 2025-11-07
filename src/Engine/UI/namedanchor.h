#pragma once

#include "ECS/system.h"
#include "2D/position.h"

#include "serialization.h"

namespace pg
{

    struct NamedUiAnchor : public Component
    {
        DEFAULT_COMPONENT_MEMBERS(NamedUiAnchor)

        // Named anchors for basic cardinal directions
        std::string topAnchorName;
        std::string leftAnchorName;
        std::string rightAnchorName;
        std::string bottomAnchorName;

        // Named anchors for center alignment
        std::string verticalCenterAnchorName;
        std::string horizontalCenterAnchorName;

        // Flags indicating which anchors are set
        bool hasTopAnchor = false;
        bool hasLeftAnchor = false;
        bool hasRightAnchor = false;
        bool hasBottomAnchor = false;
        bool hasVerticalCenter = false;
        bool hasHorizontalCenter = false;

        // Anchor types for each direction
        AnchorType topAnchorType = AnchorType::Top;
        AnchorType leftAnchorType = AnchorType::Left;
        AnchorType rightAnchorType = AnchorType::Right;
        AnchorType bottomAnchorType = AnchorType::Bottom;
        AnchorType verticalCenterAnchorType = AnchorType::VerticalCenter;
        AnchorType horizontalCenterAnchorType = AnchorType::HorizontalCenter;

        // Margins
        float topMargin = 0.0f;
        float leftMargin = 0.0f;
        float rightMargin = 0.0f;
        float bottomMargin = 0.0f;

        // Named constraints
        std::string widthConstrainName;
        std::string heightConstrainName;
        std::string zConstrainName;

        bool hasWidthConstrain = false;
        bool hasHeightConstrain = false;
        bool hasZConstrain = false;

        AnchorType widthConstrainType = AnchorType::Width;
        AnchorType heightConstrainType = AnchorType::Height;
        AnchorType zConstrainType = AnchorType::Z;

        PosOpType widthConstrainOpType = PosOpType::None;
        PosOpType heightConstrainOpType = PosOpType::None;
        PosOpType zConstrainOpType = PosOpType::None;

        float widthConstrainOpValue = 0.0f;
        float heightConstrainOpValue = 0.0f;
        float zConstrainOpValue = 0.0f;

        // Methods to set anchors
        void setTopAnchor(const std::string& entityName, AnchorType type = AnchorType::Top);
        void setLeftAnchor(const std::string& entityName, AnchorType type = AnchorType::Left);
        void setRightAnchor(const std::string& entityName, AnchorType type = AnchorType::Right);
        void setBottomAnchor(const std::string& entityName, AnchorType type = AnchorType::Bottom);
        void setVerticalCenter(const std::string& entityName, AnchorType type = AnchorType::VerticalCenter);
        void setHorizontalCenter(const std::string& entityName, AnchorType type = AnchorType::HorizontalCenter);

        // Methods to clear anchors
        void clearTopAnchor();
        void clearLeftAnchor();
        void clearRightAnchor();
        void clearBottomAnchor();
        void clearVerticalCenter();
        void clearHorizontalCenter();
        void clearAnchors();

        // Convenience methods
        void fillIn(const std::string& entityName);
        void centeredIn(const std::string& entityName);

        // Methods to set margins
        void setTopMargin(float value);
        void setLeftMargin(float value);
        void setRightMargin(float value);
        void setBottomMargin(float value);

        // Methods to set constraints
        void setWidthConstrain(const std::string& entityName, AnchorType type = AnchorType::Width, PosOpType opType = PosOpType::None, float opValue = 0.0f);
        void setHeightConstrain(const std::string& entityName, AnchorType type = AnchorType::Height, PosOpType opType = PosOpType::None, float opValue = 0.0f);
        void setZConstrain(const std::string& entityName, AnchorType type = AnchorType::Z, PosOpType opType = PosOpType::None, float opValue = 0.0f);

        virtual void onCreation(EntityRef entity) override;

        inline static std::string getType() { return "NamedUiAnchor"; }
    };

    template <>
    void serialize(Archive& archive, const NamedUiAnchor& value);

    // Serialization functions
    template <>
    NamedUiAnchor deserialize(const UnserializedObject& serializedString);

    // Event to trigger anchor resolution
    struct ResolveNamedAnchorsEvent
    {
        _unique_id entityId = 0;
    };

    // System to manage named anchors
    struct NamedUiAnchorSystem : public System<Own<NamedUiAnchor>, Own<UiAnchor>, Listener<EntityChangedEvent>, Listener<ResolveNamedAnchorsEvent>, InitSys>
    {
        virtual std::string getSystemName() const override { return "Named UI Anchor System"; }

        virtual void init() override;

        virtual void onEvent(const EntityChangedEvent& event) override;
        virtual void onEvent(const ResolveNamedAnchorsEvent& event) override;

        virtual void execute() override;

    private:
        void resolveNamedAnchor(_unique_id entityId);
        void updateUiAnchor(_unique_id entityId);
        _unique_id getEntityIdByName(const std::string& name) const;

        std::set<_unique_id> pendingResolution;
    };
}