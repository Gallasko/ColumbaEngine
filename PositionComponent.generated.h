#pragma once

#include <cstdint>
#include "pgconstant.h"

namespace pg {

class PositionComponent {
public:
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float rotation = 0.0f;
    bool visible = true;
    bool observable = true;
    _unique_id id = 0;
    EntitySystem* ecsRef = nullptr;

    float getX() const { return x; }
    float getY() const { return y; }
    float getZ() const { return z; }
    float getWidth() const { return width; }
    float getHeight() const { return height; }
    float getRotation() const { return rotation; }
    bool getVisible() const { return visible; }
    bool getObservable() const { return observable; }
    _unique_id getId() const { return id; }
    EntitySystem* getEcsRef() const { return ecsRef; }

    void setX(const float& value);
    void setY(const float& value);
    void setZ(const float& value);
    void setWidth(const float& value);
    void setHeight(const float& value);
    void setRotation(const float& value);
    void setVisible(const bool& value);
    void setObservable(const bool& value);
};

} // namespace pg
