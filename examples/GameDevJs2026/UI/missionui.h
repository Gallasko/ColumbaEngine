#pragma once

#include "Systems/basicsystems.h"
#include "Input/inputcomponent.h"
#include "ECS/entitysystem_fwd.h"

#include "missionsystem.h"
#include "depotsystem.h"
#include "playerinventory.h"
#include "itemregistry.h"
#include "inventoryui.h"

using namespace pg;

class MissionUISystem : public System<QueuedListener<OnMouseClick>,
                                       QueuedListener<OnSDLScanCode>,
                                       Listener<TickEvent>,
                                       Listener<ResizeEvent>>
{
public:
    static constexpr size_t UI_VP = 2;

    // --- Layout constants ---
    static constexpr float PANEL_W         = 600.0f;
    static constexpr float LEFT_RATIO      = 0.35f;
    static constexpr float LEFT_W          = PANEL_W * LEFT_RATIO;
    static constexpr float RIGHT_W         = PANEL_W * (1.0f - LEFT_RATIO);
    static constexpr float PADDING         = 14.0f;
    static constexpr float DIVIDER_H       = 2.0f;

    // Top chrome
    static constexpr float TITLE_H         = 30.0f;
    static constexpr float CLOSE_SIZE      = 24.0f;

    // Left column (mission list)
    static constexpr float LIST_ROW_H      = 32.0f;
    static constexpr float STATUS_SQ_SIZE  = 12.0f;
    static constexpr float STATUS_SQ_RAD   = 2.0f;
    static constexpr float GO_PILL_W       = 36.0f;
    static constexpr float GO_PILL_H       = 18.0f;
    static constexpr float GO_PILL_RAD     = 9.0f;
    static constexpr float SEPARATOR_H     = 1.0f;

    // Right column (detail pane)
    static constexpr float DETAIL_PAD      = 16.0f;
    static constexpr float BLOCK_RADIUS    = 6.0f;
    static constexpr float COST_BORDER     = 1.0f;
    static constexpr float REWARD_BORDER   = 2.0f;
    static constexpr float BLOCK_H         = 44.0f;
    static constexpr float ICON_SIZE       = 20.0f;
    static constexpr float ACTION_BTN_H    = 36.0f;
    static constexpr float ACTION_BTN_RAD  = 6.0f;
    static constexpr float PROGRESS_H      = 10.0f;

    // Limits
    static constexpr size_t MAX_LIST_ROWS    = 12;
    static constexpr size_t MAX_COST_ITEMS   = 3;
    static constexpr size_t MAX_REWARD_ITEMS = 4;
    static constexpr size_t NUM_TABS         = 3;

    // Font paths
    static constexpr const char* FONT_LIGHT  = "res/font/Inter/static/Inter_28pt-Light.ttf";
    static constexpr const char* FONT_MEDIUM = "res/font/Inter/static/Inter_28pt-Medium.ttf";
    static constexpr const char* FONT_BOLD   = "res/font/Inter/static/Inter_28pt-Bold.ttf";

    // Font scales
    static constexpr float SCALE_TITLE   = 0.45f;
    static constexpr float SCALE_DISPLAY = 0.42f;
    static constexpr float SCALE_BODY    = 0.28f;
    static constexpr float SCALE_LABEL   = 0.24f;
    static constexpr float SCALE_LIST    = 0.28f;
    static constexpr float SCALE_TAB     = 0.26f;
    static constexpr float SCALE_BTN     = 0.30f;
    static constexpr float SCALE_NUM     = 0.26f;
    static constexpr float SCALE_PILL    = 0.22f;

    // Dark color palette
    struct C {
        static inline const constant::Vector4D BG         = {31.0f, 34.0f, 40.0f, 240.0f};
        static inline const constant::Vector4D PANEL      = {42.0f, 46.0f, 54.0f, 255.0f};
        static inline const constant::Vector4D TEXT       = {214.0f, 210.0f, 196.0f, 255.0f};
        static inline const constant::Vector4D TEXT_DIM   = {120.0f, 118.0f, 112.0f, 255.0f};
        static inline const constant::Vector4D ACCENT     = {217.0f, 106.0f, 58.0f, 255.0f};
        static inline const constant::Vector4D DIVIDER    = {60.0f, 63.0f, 70.0f, 255.0f};
        static inline const constant::Vector4D SELECTED   = {55.0f, 58.0f, 68.0f, 255.0f};
        static inline const constant::Vector4D LOCKED_BTN = {80.0f, 80.0f, 80.0f, 200.0f};
        static inline const constant::Vector4D DONE_BTN   = {90.0f, 90.0f, 100.0f, 200.0f};
        static inline const constant::Vector4D COST_BRD   = {80.0f, 82.0f, 90.0f, 180.0f};
        static inline const constant::Vector4D REWARD_BRD = {140.0f, 140.0f, 150.0f, 220.0f};
        static inline const constant::Vector4D BLOCK_FILL = {38.0f, 42.0f, 50.0f, 255.0f};
        static inline const constant::Vector4D WHITE      = {255.0f, 255.0f, 255.0f, 255.0f};
        static inline const constant::Vector4D TRANSPARENT= {0.0f, 0.0f, 0.0f, 0.0f};
    };

    MissionUISystem(MissionSystem* missionSystem, DepotSystem* depotSystem,
                    PlayerInventorySystem* playerInv, ItemRegistry* itemRegistry,
                    float screenWidth, float screenHeight)
        : missionSystem(missionSystem), depotSystem(depotSystem),
          playerInv(playerInv), itemRegistry(itemRegistry),
          screenWidth(screenWidth), screenHeight(screenHeight) {}

    virtual std::string getSystemName() const override { return "Mission UI System"; }

    bool isOpen() const { return visible; }
    bool isSelectingDepot() const { return pendingStart.active; }
    void toggle();
    void open();
    void close();
    void selectDepot(int depotX, int depotY);
    void cancelDepotSelection();

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
    void createLeftColumn(float px, float contentY);
    void createRightColumn(float px, float contentY);
    void setPanelVisibility(bool vis);
    void refresh();
    void refreshLeftColumn();
    void refreshRightColumn();
    void switchTab(size_t tab);
    void selectMission(size_t defIndex);

    void setEntityVisibility(uint64_t id, bool vis);
    void setEntityText(uint64_t id, const std::string& text);
    void setEntityTexture(uint64_t id, const std::string& textureName);
    void setEntityRoundedRectColor(uint64_t id, const constant::Vector4D& color);
    void setEntityTextColor(uint64_t id, const constant::Vector4D& color);

    bool isClickInRect(float cx, float cy, float rx, float ry, float rw, float rh) const;

    std::vector<size_t> getFilteredDefs(MissionCategory cat) const;

    // --- Depot selection ---
    struct PendingStart
    {
        size_t defIndex = 0;
        bool active = false;
    };

    void tryStartWithFirstAvailableDepot(size_t defIndex);
    void showDepotSelectionPrompt();
    void hideDepotSelectionPrompt();

    // --- Members ---
    MissionSystem* missionSystem = nullptr;
    DepotSystem* depotSystem = nullptr;
    PlayerInventorySystem* playerInv = nullptr;
    ItemRegistry* itemRegistry = nullptr;
    float screenWidth, screenHeight;

    bool visible = false;
    bool panelCreated = false;
    float panelH = 0.0f;
    size_t currentTab = 0;
    size_t selectedDefIndex = SIZE_MAX;

    PendingStart pendingStart;

    // --- Entity IDs: Panel chrome ---
    uint64_t backdropId = 0;
    uint64_t titleTextId = 0;
    uint64_t closeBtnBgId = 0;
    uint64_t closeBtnTextId = 0;
    uint64_t topDividerId = 0;
    uint64_t columnDividerId = 0;

    // Tab bar
    struct TabButton
    {
        uint64_t textId = 0;
        float x = 0, y = 0, w = 0;
    };
    TabButton tabButtons[NUM_TABS] = {};
    // Tab separator slashes
    uint64_t tabSlash1Id = 0;
    uint64_t tabSlash2Id = 0;

    // --- Left column (mission list) ---
    struct ListRow
    {
        uint64_t bgId = 0;
        uint64_t statusBorderId = 0;
        uint64_t statusFillId = 0;
        uint64_t nameId = 0;
        uint64_t goPillBgId = 0;
        uint64_t goPillTextId = 0;
        uint64_t separatorId = 0;
        float rowX = 0, rowY = 0, rowW = 0;
    };
    ListRow listRows[MAX_LIST_ROWS] = {};

    // --- Right column (detail pane) ---
    uint64_t detailMissionLabelId = 0;
    uint64_t detailNameId = 0;
    uint64_t detailDescId = 0;

    // Cost block
    uint64_t costLabelId = 0;
    uint64_t costBlockBorderId = 0;
    uint64_t costBlockFillId = 0;
    struct CostDisplay
    {
        uint64_t iconId = 0;
        uint64_t countTextId = 0;
    };
    CostDisplay costItems[MAX_COST_ITEMS] = {};

    // Reward block
    uint64_t rewardLabelId = 0;
    uint64_t rewardBlockBorderId = 0;
    uint64_t rewardBlockFillId = 0;
    struct RewardDisplay
    {
        uint64_t iconId = 0;
        uint64_t countTextId = 0;
    };
    RewardDisplay rewardItems[MAX_REWARD_ITEMS] = {};

    // Unlock label
    uint64_t unlockLabelId = 0;

    // Progress bar (for active endgame missions)
    uint64_t detailProgressBgId = 0;
    uint64_t detailProgressFillId = 0;
    uint64_t detailProgressTextId = 0;

    // Action button
    uint64_t actionBtnBgId = 0;
    uint64_t actionBtnTextId = 0;
    float actionBtnX = 0, actionBtnY = 0, actionBtnW = 0;

    // Shop section (tab 2)
    uint64_t shopLabelId = 0;
    uint64_t shopCostTextId = 0;

    // Depot selection prompt
    uint64_t promptBgId = 0;
    uint64_t promptTextId = 0;
    bool promptVisible = false;

    // Cached filtered def indices
    std::vector<size_t> filteredDefs;
};
