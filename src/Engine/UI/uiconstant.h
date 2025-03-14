#pragma once

#include <memory>

#include "ECS/uniqueid.h"
#include "logger.h"

#include "ECS/entitysystem.h"

namespace pg
{
    // namespace
    // {
    //     const char * DOM = "Ui constant";
    // }

    enum class AnchorDir : uint8_t
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

    struct UiSizeChangeEvent
    {
        _unique_id id;
    };

    struct UiComponentInternalChangeEvent { _unique_id parent, child; };

    enum class UiOrientation
    {
        VERTICAL,
        HORIZONTAL
    };

    //TODO check if a pointer to the current uisize is not already present in the children of the operation to avoid an infinite lockup
    //TODO make the operation here constant and then change uisystem and ui animation accordingly to make everything constant
    //TODO throw if we try to divide by zero
    class UiSize
    {
        class UiValue
        {
        friend class UiSize;
        public:
            enum class UiSizeOpType
            {
                ADD,
                SUB,
                MUL,
                DIV,

                NONE
            };
            
        public:
            UiValue(const float& pixelSize = 0.0f, const float& scaleValue = 0.0f, std::shared_ptr<UiValue> ref1 = nullptr, std::shared_ptr<UiValue> ref2 = nullptr, const UiSizeOpType& op = UiSizeOpType::NONE) : pixelSize(pixelSize), scaleValue(scaleValue), refSize1(ref1), refSize2(ref2), opType(op) { }

            float returnCurrentSize() const;

            void setDirty() { dirty = true; }

            void setEntity(_unique_id id, EntitySystem *ecs) { entityId = id; ecsRef = ecs; }

            inline _unique_id getEntityId() const { return entityId; }

            friend void serialize<>(Archive& archive, const UiSize::UiValue& value);
        
        public:
            AnchorDir dir; 

        private:
            /** Entity id of the entity using this UiValue, default to 0 (no entity) */
            _unique_id entityId = 0;

            EntitySystem *ecsRef = nullptr;

            mutable float oldValue = 0.0f;
            mutable float currentValue = 0.0f;
            mutable bool dirty = true;

        private:
            float pixelSize = 0.0f;
            float scaleValue = 0.0f;
            std::shared_ptr<UiValue> refSize1;
            std::shared_ptr<UiValue> refSize2;
            UiSizeOpType opType = UiSizeOpType::NONE;
        };

    // Todo on op =  trigger an event to send an update event 
    public:
        UiSize(const AnchorDir& dir = AnchorDir::None, float pixelSize = 0.0f, float scaleValue = 0.0f, std::shared_ptr<UiValue> ref1 = nullptr, std::shared_ptr<UiValue> ref2 = nullptr, const UiValue::UiSizeOpType& op = UiValue::UiSizeOpType::NONE) : value(std::make_shared<UiValue>(pixelSize, scaleValue, ref1, ref2, op)) { value->dir = dir; }
        UiSize(float pixelSize) : UiSize(AnchorDir::None, pixelSize) {}
        UiSize(int pixelSize) : UiSize(AnchorDir::None, pixelSize) {}
        //UiSize(const float& pixelSize = 0.0f, const float& scaleValue = 0.0f, std::shared_ptr<UiValue> ref1 = nullptr, std::shared_ptr<UiValue> ref2 = nullptr, const UiValue::UiSizeOpType& op = UiValue::UiSizeOpType::NONE) : value(std::make_shared<UiValue>(pixelSize, scaleValue, ref1, ref2, op)) {}
        UiSize(const UiSize& size) : value(size.value) {} // TODO create a copy contruct with a bool to delete pointer
        UiSize(const UiSize* size) : UiSize(AnchorDir::None, 0.0f, 1.0f, size->value, nullptr, UiValue::UiSizeOpType::NONE) { value->entityId = size->value->entityId; value->ecsRef = size->value->ecsRef; }

        void operator=(const UiSize& rhs) // use to copy a ui size
        {
            value->pixelSize = rhs.value->pixelSize;
            value->scaleValue = rhs.value->scaleValue;
            value->refSize1 = rhs.value->refSize1;
            value->refSize2 = rhs.value->refSize2;
            value->opType = rhs.value->opType;

            // value->entityId = rhs.value->entityId;
            // value->ecsRef = rhs.value->ecsRef;
            // value->oldValue = rhs.value->oldValue;
            // value->currentValue = rhs.value->currentValue;
            // value->dirty = rhs.value->dirty;

            if (value->ecsRef and value->entityId != 0)
            {
                if (value->refSize1 and value->refSize1->entityId != 0 and value->refSize1->entityId != value->entityId)
                {
                    value->ecsRef->sendEvent(UiComponentInternalChangeEvent{value->refSize1->entityId, value->entityId});
                }
                
                if (value->refSize2 and value->refSize2->entityId != 0 and value->refSize2->entityId != value->entityId)
                {
                    value->ecsRef->sendEvent(UiComponentInternalChangeEvent{value->refSize2->entityId, value->entityId});
                }
            }

        }

        void operator=(const AnchorDir& dir)
        {
            value->dir = dir;
        }

        void operator=(const UiSize *rhs) // use to make this ui size refer to another
        {
            value->pixelSize = 0.0f;
            value->scaleValue = 1.0f;
            value->refSize1 = rhs->value;
            value->refSize2 = nullptr;
            value->opType = UiValue::UiSizeOpType::NONE;

            value->dirty = true;

            if(value->ecsRef and value->entityId != 0 and rhs->value->entityId != value->entityId)
                value->ecsRef->sendEvent(UiComponentInternalChangeEvent{rhs->value->entityId, value->entityId});
        }

        void operator=(const int& rhs)
        {
            value->pixelSize = rhs;
            value->scaleValue = 0.0f;
            value->refSize1 = nullptr;
            value->refSize2 = nullptr;
            value->opType = UiValue::UiSizeOpType::NONE;

            value->dirty = true;
        }

        void operator=(const float& rhs)
        {
            value->pixelSize = rhs;
            value->scaleValue = 0.0f;
            value->refSize1 = nullptr;
            value->refSize2 = nullptr;
            value->opType = UiValue::UiSizeOpType::NONE;

            value->dirty = true;
        }

        UiSize& operator+=(const UiSize& rhs)
        {
            UiSize current;
            current.value->pixelSize = value->pixelSize;
            current.value->scaleValue = value->scaleValue;
            current.value->refSize1 = value->refSize1;
            current.value->refSize2 = value->refSize2;
            current.value->opType = value->opType;

            value->pixelSize = 0.0f;
            value->scaleValue = 1.0f;
            value->refSize1 = current.value;
            value->refSize2 = rhs.value;
            value->opType = UiValue::UiSizeOpType::ADD;

            value->dirty = true;

            return *this;
        }

        UiSize operator*(const float& rhs) const 
        {
            return UiSize(AnchorDir::None, 0.0f, rhs, this->value);
        }

        UiSize operator*(const int& rhs) const
        {
            return UiSize(AnchorDir::None, 0.0f, rhs, this->value);
        }

        UiSize operator*(const UiSize& rhs) const
        {
            return UiSize(AnchorDir::None, 0.0f, 1.0f, this->value, rhs.value, UiValue::UiSizeOpType::MUL);
        }

        //TODO make exception for division by 0
        UiSize operator/(const int& rhs) const 
        {
            return UiSize(AnchorDir::None, 0.0f, 1.0f / rhs, this->value);
        }

        UiSize operator/(const float& rhs) const 
        {
            return UiSize(AnchorDir::None, 0.0f, 1.0f / rhs, this->value);
        }

        UiSize operator/(const UiSize& rhs) const
        {
            return UiSize(AnchorDir::None, 0.0f, 1.0f, this->value, rhs.value, UiValue::UiSizeOpType::DIV);
        }

        UiSize operator+(const int& rhs) const
        {
            return UiSize(AnchorDir::None, rhs, 1.0f, this->value);
        }

        UiSize operator+(const float& rhs) const
        {
            return UiSize(AnchorDir::None, rhs, 1.0f, this->value);
        }

        UiSize operator+(const UiSize& rhs) const
        {
            return UiSize(AnchorDir::None, 0.0f, 1.0f, this->value, rhs.value, UiValue::UiSizeOpType::ADD);
        }

        UiSize operator-(const int& rhs) const 
        {
            return UiSize(AnchorDir::None, -rhs, 1.0f, this->value);
        }

        UiSize operator-(const float& rhs) const
        {
            return UiSize(AnchorDir::None, -rhs, 1.0f, this->value);
        }

        UiSize operator-(const UiSize& rhs) const 
        {
            return UiSize(AnchorDir::None, 0.0f, 1.0f, this->value, rhs.value, UiValue::UiSizeOpType::SUB);
        }

        UiSize operator-() const
        {
            return UiSize(AnchorDir::None, 0.0f, -1.0f, this->value);
        }

        template <typename Type>
        friend UiSize operator+(const Type& lhs, const UiSize& rhs);

        template <typename Type>
        friend UiSize operator-(const Type& lhs, const UiSize& rhs);

        template <typename Type>
        friend UiSize operator*(const Type& lhs, const UiSize& rhs);

        template <typename Type>
        friend UiSize operator/(const Type& lhs, const UiSize& rhs);

        friend void serialize<>(Archive& archive, const UiSize& value);

        friend void serialize<>(Archive& archive, const UiSize::UiValue& value);

        operator float() const
        {
            return value->returnCurrentSize();
        }

        void setEntity(_unique_id id, EntitySystem *ecs) { value->setEntity(id, ecs); }

        _unique_id getEntityId() const { return value->entityId; }

    private:
        std::shared_ptr<UiValue> value;
    };

    template <typename Type>
    UiSize operator+(const Type& lhs, const UiSize& rhs)
    {
        return rhs + lhs;
    }

    template <typename Type>
    UiSize operator-(const Type& lhs, const UiSize& rhs)
    {
        return UiSize(AnchorDir::None, lhs, -1.0f, rhs.value);
    }

    template <typename Type>
    UiSize operator*(const Type& lhs, const UiSize& rhs)
    {
        return rhs * lhs;
    }

    template <typename Type>
    UiSize operator/(const Type& lhs, const UiSize& rhs)
    {
        return UiSize(AnchorDir::None, lhs, 0.0f, nullptr, rhs.value, UiSize::UiValue::UiSizeOpType::DIV);
    }

    template <>
    void serialize(Archive& archive, const UiSize& value);

    template <>
    void serialize(Archive& archive, const UiSize::UiValue& value);

    struct UiPosition 
    {
        // UiPosValue always hold a reference to UiPoint assigned to it !

        UiPosition()
        {
            this->x = AnchorDir::X;
            this->y = AnchorDir::Y;
            this->z = AnchorDir::Z;
        }

        UiPosition(const UiSize& x, const UiSize& y, const UiSize& z)
        {
            this->x = &x;
            this->y = &y;
            this->z = &z;

            this->x = AnchorDir::X;
            this->y = AnchorDir::Y;
            this->z = AnchorDir::Z;
        }

        UiPosition(const UiPosition& pos) : x(pos.x), y(pos.y), z(pos.z)
        {
            this->x = AnchorDir::X;
            this->y = AnchorDir::Y;
            this->z = AnchorDir::Z;
        }

        UiPosition(const UiPosition *pos) : x(&pos->x), y(&pos->y), z(&pos->z)
        {
            this->x = AnchorDir::X;
            this->y = AnchorDir::Y;
            this->z = AnchorDir::Z;
        }

        void setEntity(_unique_id id, EntitySystem *ecs)
        {
            entityId = id;

            x.setEntity(id, ecs);
            y.setEntity(id, ecs);
            z.setEntity(id, ecs);
        }

        _unique_id getEntityId() const { return entityId; }

        void operator=(const UiPosition& rhs)
        {
            x = rhs.x;
            y = rhs.y; 
            z = rhs.z; 
        }

        void operator=(UiPosition *rhs)
        {
            x = &rhs->x;
            y = &rhs->y; 
            z = &rhs->z; 
        }

        UiPosition operator+(const UiPosition& rhs) const {
            UiPosition pos;
            pos.x = x + rhs.x;
            pos.y = y + rhs.y;
            pos.z = z + rhs.z;

            return pos;
        } 

        UiSize x;
        UiSize y;
        UiSize z;

        _unique_id entityId = 0;
    };

    struct UiFrame
    {
        UiFrame()
        {
            this->w = AnchorDir::Width;
            this->h = AnchorDir::Height;
        }

        UiFrame(const UiSize& x, const UiSize& y, const UiSize& z, const UiSize& w, const UiSize& h)
        {
            this->pos.x = &x; this->pos.y = &y; this->pos.z = &z; this->w = &w; this->h = &h;

            this->w = AnchorDir::Width;
            this->h = AnchorDir::Height;
        }

        UiFrame(const UiPosition& pos, const UiSize& w, const UiSize& h) : pos(&pos), w(&w), h(&h)
        {
            this->w = AnchorDir::Width;
            this->h = AnchorDir::Height;
        }

        UiFrame(const UiFrame& frame) : pos(frame.pos), w(frame.w), h(frame.h)
        {
            this->w = AnchorDir::Width;
            this->h = AnchorDir::Height;
        }

        UiFrame(const UiFrame *frame) : pos(&frame->pos), w(&frame->w), h(&frame->h)
        {
            this->w = AnchorDir::Width;
            this->h = AnchorDir::Height;
        }

        void operator=(const UiFrame& rhs)
        {
            pos.x = rhs.pos.x;
            pos.y = rhs.pos.y; 
            pos.z = rhs.pos.z;
            w = rhs.w;
            h = rhs.h; 
        }

        void operator=(const UiFrame *rhs)
        {
            pos.x = &rhs->pos.x;
            pos.y = &rhs->pos.y; 
            pos.z = &rhs->pos.z;
            w = &rhs->w;
            h = &rhs->h; 
        }

        UiPosition pos;
        UiSize w;
        UiSize h;
    };
}