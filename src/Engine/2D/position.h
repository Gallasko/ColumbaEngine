#pragma once

#include "ECS/system.h"

namespace pg
{
    enum class AnchorType : uint8_t
    {
        None = 0,
        Top,
        Right,
        Bottom,
        Left,
        X,
        Y,
        Z,
        Width,
        Height,
        TMargin,
        RMargin,
        BMargin,
        LMargin
    };

    struct PositionComponentChangedEvent
    {
        _unique_id id = 0;
    };

    enum class PosOpType
    {
        None,
        Add,
        Sub,
        Mul,
        Div
    };

    struct PosConstrain
    {
        _unique_id id = 0;
        AnchorType type = AnchorType::None;

        PosOpType opType = PosOpType::None;
        float opValue = 0.0f;
    };

    struct PosAnchor
    {
        _unique_id id = 0;
        AnchorType type = AnchorType::None;
        float value = 0.0f;
    };

    struct ParentingEvent
    {
        _unique_id parent = 0;
        _unique_id child = 0;
    };

    struct UiAnchor : public Ctor
    {
        // Current Anchor of this component
        PosAnchor top;
        PosAnchor left;
        PosAnchor right;
        PosAnchor bottom;

        bool hasTopAnchor = false;
        bool hasLeftAnchor = false;
        bool hasRightAnchor = false;
        bool hasBottomAnchor = false;

        PosAnchor topAnchor;
        PosAnchor leftAnchor;
        PosAnchor rightAnchor;
        PosAnchor bottomAnchor;

        float topMargin = 0.0f;
        float leftMargin = 0.0f;
        float rightMargin = 0.0f;
        float bottomMargin = 0.0f;

        PosConstrain widthConstrain;
        PosConstrain heightConstrain;

        void setTopAnchor(const PosAnchor& anchor);
        void clearTopAnchor();

        void setLeftAnchor(const PosAnchor& anchor);
        void clearLeftAnchor();

        void setRightAnchor(const PosAnchor& anchor);
        void clearRightAnchor();

        void setBottomAnchor(const PosAnchor& anchor);
        void clearBottomAnchor();

        void setTopMargin(float value);
        void setLeftMargin(float value);
        void setRightMargin(float value);
        void setBottomMargin(float value);

        void setWidthConstrain(const PosConstrain& constrain);
        void setHeightConstrain(const PosConstrain& constrain);

        // Todo add function to handle center, vertical and horizontal center alignment

        virtual void onCreation(EntityRef entity) override;
 
        void updateAnchor(bool hasAnchor, PosAnchor& anchor);

        void update();

        // Private:

        _unique_id id = 0;

        EntitySystem *ecsRef = nullptr;
    };

    struct ClippedTo : public Ctor
    {
        ClippedTo(_unique_id clipperId) : clipperId(clipperId) {}
        ClippedTo(const ClippedTo& other) : clipperId(other.clipperId), id(other.id), ecsRef(other.ecsRef) {}

        ClippedTo& operator=(const ClippedTo& other)
        {
            clipperId = other.clipperId;

            ecsRef = other.ecsRef;
            id = other.id;

            return *this;
        }

        virtual void onCreation(EntityRef entity) override;

        _unique_id clipperId;

        _unique_id id;

        EntitySystem *ecsRef = nullptr;
    };

    struct PositionComponent : public Ctor
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        float width = 0.0f;
        float height = 0.0f;

        bool visible = true;

        virtual void onCreation(EntityRef entity) override;

        void setX(float x);
        void setY(float y);
        void setZ(float z);

        void setWidth(float width);
        void setHeight(float height);

        void setVisibility(bool visible);

        bool updatefromAnchor(const UiAnchor& anchor);

        // Private:

        _unique_id id = 0;

        EntitySystem *ecsRef = nullptr;
    };

    // Todo add Listener<ResizeEvent>,
    struct PositionComponentSystem : public System<Own<PositionComponent>, Own<UiAnchor>, Own<ClippedTo>, Listener<ParentingEvent>, Listener<PositionComponentChangedEvent>>
    {
        virtual std::string getSystemName() const override { return "Position System"; }

        virtual void onEvent(const ParentingEvent& event) override
        {
            parentalMap[event.parent].insert(event.child);
        }

        virtual void onEvent(const PositionComponentChangedEvent& event) override
        {
            eventQueue.push(event);
        }

        void pushChildrenInChange(_unique_id parentId, Entity *entity);
        
        virtual void execute() override;

        std::unordered_map<_unique_id, std::set<_unique_id>> parentalMap;

        std::set<_unique_id> changedIds;

        std::queue<PositionComponentChangedEvent> eventQueue;

        bool updated = false;
    };
}