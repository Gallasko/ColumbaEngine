#include "uisystem.h"

#include "constant.h"
#include "logger.h"

#include "Renderer/renderer.h"

namespace pg
{
    namespace
	{
		static constexpr char const * const DOM = "Ui System";
	}

    /**
     * @brief Specialization of the serialize function for AnchorDir 
     * 
     * @param archive A references to the archive
     * @param value The anchor dir value
     */
    template <>
    void serialize(Archive& archive, const AnchorDir& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization("AnchorDir");

        std::string anchorDirString;

        switch(value)
        {
            case AnchorDir::None:
                anchorDirString = "None"; break;
            case AnchorDir::Top:
                anchorDirString = "Top"; break;
            case AnchorDir::Right:
                anchorDirString = "Right"; break;
            case AnchorDir::Bottom:
                anchorDirString = "Bottom"; break;
            case AnchorDir::Left:
                anchorDirString = "Left"; break;
            case AnchorDir::X:
                anchorDirString = "X"; break;
            case AnchorDir::Y:
                anchorDirString = "Y"; break;
            case AnchorDir::Z:
                anchorDirString = "Z"; break;
            case AnchorDir::Width:
                anchorDirString = "Width"; break;
            case AnchorDir::Height:
                anchorDirString = "Height"; break;
            case AnchorDir::TMargin:
                anchorDirString = "TMargin"; break;
            case AnchorDir::RMargin:
                anchorDirString = "RMargin"; break;
            case AnchorDir::BMargin:
                anchorDirString = "BMargin"; break;
            case AnchorDir::LMargin:
                anchorDirString = "LMargin"; break;
            default:
                LOG_ERROR(DOM, "Invalid anchor dir value");
                anchorDirString = "None";
                break;
        }

        serialize(archive, "dir", anchorDirString);

        archive.endSerialization();
    }

    /**
     * @brief Specialization of the serialize function for UiPosition 
     * 
     * @param archive A references to the archive
     * @param value The ui position value
     */
    template <>
    void serialize(Archive& archive, const UiPosition& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization("UiPosition");

        serialize(archive, "x", static_cast<float>(value.x));
        serialize(archive, "y", static_cast<float>(value.y));
        serialize(archive, "z", static_cast<float>(value.z));

        // Todo fix this (Use Uisize only in ui parenting not in pos component Too heavy !)
        // serialize(archive, "x", value.x);
        // serialize(archive, "y", value.y);
        // serialize(archive, "z", value.z);

        archive.endSerialization();
    }

    /**
     * @brief Specialization of the serialize function for UiFrame 
     * 
     * @param archive A references to the archive
     * @param value The ui frame value
     */
    template <>
    void serialize(Archive& archive, const UiFrame& value)
    {
        LOG_THIS(DOM);

        archive.startSerialization("UiFrame");

        serialize(archive, "pos", value.pos);
        serialize(archive, "w", value.w);
        serialize(archive, "h", value.h);

        archive.endSerialization();
    }

    /**
     * @brief Specialization of the serialize function for UiComponent 
     * 
     * @param archive A references to the archive
     * @param value The ui component value
     */
    template <>
    void serialize(Archive& archive, const UiComponent& value)
    {
        LOG_THIS(DOM);

        // Can't actually serialize anchors here cause they are just pointers to other components
        // So they don't hold the same value each time

        archive.startSerialization(UiComponent::getType());

        serialize(archive, "visibility",    value.isVisible());
        serialize(archive, "pos",           value.pos);
        serialize(archive, "width",         static_cast<float>(value.width));
        serialize(archive, "height",        static_cast<float>(value.height));
        serialize(archive, "rotation",      value.rotation);

        // Todo bring back this in the different Comp ( position, uiparenting, ...)

        // serialize(archive, "width",         value.width);
        // serialize(archive, "height",        value.height);

        // serialize(archive, "topMargin",     value.topMargin);
        // serialize(archive, "rightMargin",   value.rightMargin);
        // serialize(archive, "bottomMargin",  value.bottomMargin);
        // serialize(archive, "leftMargin",    value.leftMargin);

        // serialize(archive, "hasTopAnchor", value.hasTopAnchor);
        // serialize(archive, "hasRightAnchor", value.hasRightAnchor);
        // serialize(archive, "hasBottomAnchor", value.hasBottomAnchor);
        // serialize(archive, "hasLeftAnchor", value.hasLeftAnchor);

        // if (value.hasTopAnchor)
        //     serialize(archive, "topAnchor", value.topAnchor);

        // if (value.hasRightAnchor)
        //     serialize(archive, "rightAnchor", value.rightAnchor);
        
        // if (value.hasBottomAnchor)
        //     serialize(archive, "bottomAnchor", value.bottomAnchor);

        // if (value.hasLeftAnchor)
        //     serialize(archive, "leftAnchor", value.leftAnchor);

        archive.endSerialization();
    }

    /**
     * @brief Specialization of the deserialize function for AnchorDir
     * 
     * @param serializedString A serialized string
     * @return AnchorDir An AnchorDir object contructed via the serialization string
     */
    template <>
    AnchorDir deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        AnchorDir dir = AnchorDir::None;

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            type = deserialize<std::string>(serializedString["dir"]);

            if (type == "None")
            {
                dir = AnchorDir::None;
            }
            else if (type =="Top")
            {
                dir = AnchorDir::Top;
            }
            else if (type =="Right")
            {
                dir = AnchorDir::Right;
            }
            else if (type =="Bottom")
            {
                dir = AnchorDir::Bottom;
            }
            else if (type =="Left")
            {
                dir = AnchorDir::Left;
            }
            else if (type =="X")
            {
                dir = AnchorDir::X;
            }
            else if (type =="Y")
            {
                dir = AnchorDir::Y;
            }
            else if (type =="Z")
            {
                dir = AnchorDir::Z;
            }
            else if (type =="Width")
            {
                dir = AnchorDir::Width;
            }
            else if (type =="Height")
            {
                dir = AnchorDir::Height;
            }
            else if (type =="TMargin")
            {
                dir = AnchorDir::TMargin;
            }
            else if (type =="RMargin")
            {
                dir = AnchorDir::RMargin;
            }
            else if (type =="BMargin")
            {
                dir = AnchorDir::BMargin;
            }
            else if (type =="LMargin")
            {
                dir = AnchorDir::LMargin;
            }
            else
            {
                LOG_ERROR(DOM, "Invalid anchor dir value");
                dir = AnchorDir::None;
            }
        }

        return dir;
    }

    /**
     * @brief Specialization of the deserialize function for UiSize
     * 
     * @param serializedString A serialized string
     * @return UiSize An UiSize object contructed via the serialization string
     */
    template <>
    UiSize deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        UiSize size;

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            size = deserialize<float>(serializedString["floatValue"]);

            // Todo add deserialization for UiValue !
        }

        return size;
    }

    /**
     * @brief Specialization of the deserialize function for UiPosition
     * 
     * @param serializedString A serialized string
     * @return UiPosition An UiPosition object contructed via the serialization string
     */
    template <>
    UiPosition deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        UiPosition pos;

        if(serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            pos.x = deserialize<float>(serializedString["x"]);
            pos.y = deserialize<float>(serializedString["y"]);
            pos.z = deserialize<float>(serializedString["z"]);

            // Todo fix this at the same time as serialize
            // pos.x = deserialize<UiSize>(serializedString["x"]);
            // pos.y = deserialize<UiSize>(serializedString["y"]);
            // pos.z = deserialize<UiSize>(serializedString["z"]);
        }

        return pos;
    }

    /**
     * @brief Specialization of the deserialize function for UiFrame
     * 
     * @param serializedString A serialized string
     * @return UiFrame An UiFrame object contructed via the serialization string
     */
    template <>
    UiFrame deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        UiFrame frame;

        if(serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an UiFrame");

            frame.pos = deserialize<UiPosition>(serializedString["pos"]);
            frame.w   = deserialize<UiSize>(serializedString["w"]);
            frame.h   = deserialize<UiSize>(serializedString["h"]);
        }

        return frame;
    }

    /**
     * @brief Specialization of the deserialize function for UiComponent
     * 
     * @param serializedString A serialized string
     * @return UiComponent An UiComponent object contructed via the serialization string
     */
    template <>
    UiComponent deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS(DOM);

        UiComponent component;

        if (serializedString.isNull())
        {
            LOG_ERROR(DOM, "Element is null");
        }
        else
        {
            LOG_INFO(DOM, "Deserializing an UiComponent");

            deserialize<bool>(serializedString["visibility"]) ? component.show() : component.hide();

            component.pos           = deserialize<UiPosition>(serializedString["pos"]);

            component.width         = deserialize<float>(serializedString["width"]);
            component.height        = deserialize<float>(serializedString["height"]);
            component.rotation      = deserialize<float>(serializedString["rotation"]);

            // Todo Fix this !
            // component.width         = deserialize<UiSize>(serializedString["width"]);
            // component.height        = deserialize<UiSize>(serializedString["height"]);
            // component.topMargin     = deserialize<UiSize>(serializedString["topMargin"]);
            // component.rightMargin   = deserialize<UiSize>(serializedString["rightMargin"]);
            // component.bottomMargin  = deserialize<UiSize>(serializedString["bottomMargin"]);
            // component.leftMargin    = deserialize<UiSize>(serializedString["leftMargin"]);

            component.update();
        }

        return component;
    }

    UiComponent::UiComponent(const UiComponent& rhs)
    {
        LOG_THIS_MEMBER(DOM);

        // Copy ui component stuff

        this->visible       = rhs.visible;
        this->pos           = rhs.pos;
        this->width         = rhs.width;
        this->height        = rhs.height;

        this->rotation      = rhs.rotation;

        this->topAnchor     = rhs.topAnchor;
        this->rightAnchor   = rhs.rightAnchor;
        this->bottomAnchor  = rhs.bottomAnchor;
        this->leftAnchor    = rhs.leftAnchor;

        this->topMargin     = rhs.topMargin;
        this->rightMargin   = rhs.rightMargin;
        this->bottomMargin  = rhs.bottomMargin;
        this->leftMargin    = rhs.leftMargin;

        this->hasTopAnchor = rhs.hasTopAnchor;
        this->hasRightAnchor = rhs.hasRightAnchor;
        this->hasBottomAnchor = rhs.hasBottomAnchor;
        this->hasLeftAnchor = rhs.hasLeftAnchor;

        top = AnchorDir::Top;
        right = AnchorDir::Right;
        bottom = AnchorDir::Bottom;
        left = AnchorDir::Left;

        // Copy entity stuff

        this->ecsRef   = rhs.ecsRef;
        this->entityId = rhs.entityId;

        top.setEntity(entityId, ecsRef);
        right.setEntity(entityId, ecsRef);
        bottom.setEntity(entityId, ecsRef);
        left.setEntity(entityId, ecsRef);

        topAnchor.setEntity(entityId, ecsRef);
        rightAnchor.setEntity(entityId, ecsRef);
        bottomAnchor.setEntity(entityId, ecsRef);
        leftAnchor.setEntity(entityId, ecsRef);

        pos.setEntity(entityId, ecsRef);

        width.setEntity(entityId, ecsRef);
        height.setEntity(entityId, ecsRef);

        topMargin.setEntity(entityId, ecsRef);
        leftMargin.setEntity(entityId, ecsRef);
        rightMargin.setEntity(entityId, ecsRef);
        bottomMargin.setEntity(entityId, ecsRef);

        isClippedToWindow = rhs.isClippedToWindow;

        clipTopLeft = rhs.clipTopLeft;
        clipBottomRight = rhs.clipBottomRight;
    }

    bool UiComponent::inBound(float x, float y) const
    {
        LOG_THIS_MEMBER(DOM);

        if (not visible)
            return false;

        // Lockup x and y only once
        const float xValue = this->pos.x;
        const float yValue = this->pos.y;

        return x >= xValue and x <= (xValue + this->width) and y <= (yValue + this->height) and y >= yValue;
    }

    bool UiComponent::inBound(const constant::Vector2D& vec2) const
    {
        LOG_THIS_MEMBER(DOM);

        return inBound(vec2.x, vec2.y);
    }

    bool UiComponent::inClipBound(float x, float y) const
    {
        LOG_THIS_MEMBER(DOM);

        if (not visible)
            return false;
        
        if (isClippedToWindow)
            return inBound(x, y);

        // Lockup x and y only once
        const float xValue = this->pos.x;
        const float yValue = this->pos.y;

        // Todo add clip resolution
        return x >= clipTopLeft.horizontalAnchor and x <= clipBottomRight.horizontalAnchor and
               y >= clipTopLeft.verticalAnchor and y <= clipBottomRight.verticalAnchor and
               x >= xValue and x <= (xValue + this->width) and y <= (yValue + this->height) and y >= yValue;
    }

    bool UiComponent::inClipBound(const constant::Vector2D& vec2) const
    {
        LOG_THIS_MEMBER(DOM);

        return inClipBound(vec2.x, vec2.y);
    }

    void UiComponent::update()
    {
        LOG_THIS_MEMBER(DOM);
        
        if (hasTopAnchor and hasBottomAnchor)
        {
            this->height = (bottomAnchor - bottomMargin) - (topAnchor - topMargin);
            this->pos.y = topAnchor + topMargin;
        }
        else if (hasTopAnchor and not hasBottomAnchor)
        {
            this->pos.y = topAnchor + topMargin;
        }
        else if (not hasTopAnchor and hasBottomAnchor)
        {
            this->pos.y = (bottomAnchor - bottomMargin) - this->height;
        }

        if (hasRightAnchor and hasLeftAnchor)
        {
            this->width = (rightAnchor - rightMargin) - (leftAnchor - leftMargin);
            this->pos.x = leftAnchor + leftMargin;
        }
        else if (hasRightAnchor and not hasLeftAnchor)
        {
            this->pos.x = (rightAnchor - rightMargin) - this->width;
        }
        else if (not hasRightAnchor and hasLeftAnchor)
        {
            this->pos.x = leftAnchor + leftMargin;
        }

        if (ecsRef)
            ecsRef->sendEvent(EntityChangedEvent{entityId});
    }
}