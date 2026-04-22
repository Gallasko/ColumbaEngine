#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"
#include "ECS/entitysystem_fwd.h"

#include "missionsystem.h"
#include "depotsystem.h"
#include "playerinventory.h"

using namespace pg;

class MissionUISystem : public System<QueuedListener<OnMouseClick>,
                                       QueuedListener<OnSDLScanCode>,
                                       Listener<TickEvent>,
                                       Listener<ResizeEvent>>
{
public:
    static constexpr size_t UI_VP = 2;

    // Layout
    static constexpr float PANEL_W        = 380.0f;
    static constexpr float PADDING        = 12.0f;
    static constexpr float TITLE_H        = 28.0f;
    static constexpr float SECTION_H      = 20.0f;
    static constexpr float ROW_H          = 28.0f;
    static constexpr float ROW_GAP        = 4.0f;
    static constexpr float BTN_W          = 50.0f;
    static constexpr float BTN_H          = 22.0f;
    static constexpr float PROGRESS_H     = 10.0f;
    static constexpr float CLOSE_SIZE     = 24.0f;

    static constexpr size_t MAX_DEF_ROWS    = 5;
    static constexpr size_t MAX_ACTIVE_ROWS = 3;

    static constexpr float TITLE_SCALE   = 0.40f;
    static constexpr float TEXT_SCALE     = 0.30f;
    static constexpr float BTN_TEXT_SCALE = 0.30f;

    static constexpr const char* FONT_PATH = "res/font/Inter/static/Inter_28pt-Light.ttf";

    MissionUISystem(MissionSystem* missionSystem, DepotSystem* depotSystem,
                    PlayerInventorySystem* playerInv,
                    float screenWidth, float screenHeight)
        : missionSystem(missionSystem), depotSystem(depotSystem),
          playerInv(playerInv),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Mission UI System"; }

    bool isOpen() const { return visible; }
    void toggle();
    void open();
    void close();

    virtual void onProcessEvent(const OnMouseClick& event) override;
    virtual void onProcessEvent(const OnSDLScanCode& event) override;
    virtual void onEvent(const TickEvent&) override;
    virtual void onEvent(const ResizeEvent& event) override
    {
        screenWidth = event.width;
        screenHeight = event.height;
    }

    void execute() override;

private:
    float getPanelX() const { return (screenWidth - PANEL_W) * 0.5f; }
    float getPanelY() const { return (screenHeight - panelH) * 0.5f; }

    void ensurePanelCreated();
    void createPanel();
    void setPanelVisibility(bool vis);
    void refresh();

    void setEntityVisibility(uint64_t id, bool vis);
    void setEntityText(uint64_t id, const std::string& text);

    bool isClickInRect(float cx, float cy, float rx, float ry, float rw, float rh) const;

    // --- Depot selection for starting missions ---
    struct PendingStart
    {
        size_t defIndex = 0;
        bool active = false;
    };

    void tryStartWithFirstAvailableDepot(size_t defIndex);

    // --- Members ---
    MissionSystem* missionSystem = nullptr;
    DepotSystem* depotSystem = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    float screenWidth, screenHeight;

    bool visible = false;
    bool panelCreated = false;
    float panelH = 0.0f;

    PendingStart pendingStart;

    // Entity IDs — panel chrome
    uint64_t backdropId = 0;
    uint64_t titleTextId = 0;
    uint64_t closeBtnBgId = 0;
    uint64_t closeBtnTextId = 0;

    // Available section
    uint64_t availHeaderId = 0;
    struct DefRow
    {
        uint64_t bgId = 0;
        uint64_t nameId = 0;
        uint64_t infoId = 0;
        uint64_t btnBgId = 0;
        uint64_t btnTextId = 0;
        float btnX = 0, btnY = 0;
    };
    DefRow defRows[MAX_DEF_ROWS] = {};

    // Active section
    uint64_t activeHeaderId = 0;
    struct ActiveRow
    {
        uint64_t bgId = 0;
        uint64_t nameId = 0;
        uint64_t progressBgId = 0;
        uint64_t progressFillId = 0;
        uint64_t statusId = 0;
        uint64_t btnBgId = 0;
        uint64_t btnTextId = 0;
        float btnX = 0, btnY = 0;
    };
    ActiveRow activeRows[MAX_ACTIVE_ROWS] = {};

    // Shop section
    uint64_t shopHeaderId = 0;
    uint64_t shopLabelId = 0;
    uint64_t shopBtnBgId = 0;
    uint64_t shopBtnTextId = 0;
    float shopBtnX = 0, shopBtnY = 0;
};
