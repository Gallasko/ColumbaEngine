#pragma once

#include "ECS/system.h"
#include "Input/inputcomponent.h"

#include <string>
#include <vector>

namespace pg { class MasterRenderer; }

class AtlasPickerSystem : public pg::System<pg::Listener<pg::OnMouseClick>, pg::InitSys>
{
public:
    AtlasPickerSystem(pg::MasterRenderer* renderer,
                      const std::string& atlasName,
                      const std::string& fontPath,
                      unsigned int tileW, unsigned int tileH,
                      unsigned int cols, unsigned int totalCount,
                      float scale,
                      float screenW, float screenH);

    virtual std::string getSystemName() const override { return "Atlas Picker System"; }

    virtual void init() override;

    virtual void onEvent(const pg::OnMouseClick& event) override;

private:
    pg::MasterRenderer* renderer;
    std::string atlasName;
    std::string fontPath;
    unsigned int tileW, tileH;
    unsigned int cols, totalCount;
    float scale;
    float screenW, screenH;

    float displayTileW, displayTileH;

    pg::_unique_id labelEntityId = 0;
    std::vector<pg::_unique_id> tileEntityIds;
};
