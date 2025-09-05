#pragma once

#include "Scene/scenemanager.h"

#include "Systems/coresystems.h"

#include "achievement.h"
#include "gamefacts.h"

#include "theme.h"

namespace pg
{
    struct RessourceGenerator;

    struct NexusButtonCost
    {
        std::string resourceId = "";
        float value = 0.0f;
        std::string valueId = "";
        bool consumed = true;
    };

    // AutoClicker system structures
    struct AutoClicker
    {
        std::string id;                         // Unique identifier
        std::string targetButtonId;            // Button to auto-click
        float baseInterval = 5000.0f;          // Base time between clicks (ms)
        bool active = false;                   // Is the auto-clicker running
        float timer = 0.0f;                    // Current timer countdown
        size_t clickCount = 0;                 // Statistics tracking
        
        // Purchase/unlock system
        std::vector<NexusButtonCost> costs = {};        // Cost to buy this auto-clicker
        std::vector<FactChecker> unlockConditions = {}; // Conditions to unlock
        bool owned = false;                             // Player owns this auto-clicker
    };

    // AutoClicker events
    struct PurchaseAutoClicker
    {
        std::string autoClickerId;
    };

    struct ToggleAutoClicker
    {
        std::string autoClickerId;
        bool enable = true;  // true = enable, false = disable
    };

    struct AutoClickerAction
    {
        std::string autoClickerId;
        std::string targetButtonId;  // For statistics tracking
    };

    // A data‐driven UI button definition.
    struct DynamicNexusButton
    {
        std::string id;                         // Unique identifier.
        std::string label;                      // Button text.
        std::vector<FactChecker> conditions;    // Conditions that must be met for the button to trigger the outcome.
        std::vector<AchievementReward> outcome; // Callback executed on click.
        std::string category = "Main";          // Category (e.g., "Main", "Mana Upgrade", etc.)
        std::string description = "";           // Description of the button, to be shown in the tooltip

        size_t nbClickBeforeArchive = 1;        // Number of times the button should be clicked before being archived (0 means infinite)
        std::vector<NexusButtonCost> costs = {};// Cost of clicking the button
        std::vector<float> costIncrease = {};   // Increment of the cost of the button after each click

        std::vector<size_t> neededConditionsForVisibility = {}; // Conditions that must be met for the button to appear. (If empty, every condition must be met)

        bool activable = false;                 // Indicates if the button can be activated.
        float activationTime = 1000.0f;         // Time in milliseconds needed for the button to be active for outputing outcome (Todo this should be an union between a string (value comes from a worldfact) and a float(value is set)).

        // Internals
        size_t nbClick = 0;                     // Number of times the button was clicked
        bool archived = false;                  // Flag indicating whether the button is archived or not.

        bool clickable = true;                  // Flag to track if the button is visible but not all conditions are met

        bool active = false;                    // Indicates if the button is currently active.
        float activeTime = 0.0f;                // Time in milliseconds the button has been active.

        _unique_id entityId = 0;                // Entity identifier
        _unique_id backgroundId = 0;            // Background identifier

        // Prestige system fields
        std::vector<std::string> prestigeTags = {}; // Tags for prestige system (e.g., ["basic", "reset", "tutorial"])
    };

    struct ResourceDisplayEntry
    {
        std::string resourceName;
        EntityRef uiEntity;
    };

    template <>
    void serialize(Archive& archive, const NexusButtonCost& value);

    template <>
    void serialize(Archive& archive, const DynamicNexusButton& value);

    template <>
    void serialize(Archive& archive, const AutoClicker& value);

    struct NexusButtonStateChange
    {
        DynamicNexusButton button;
    };

    struct CurrentActiveButton
    {
        std::vector<std::string> ids;
    };

    struct NexusSystem : public System<Listener<NexusButtonStateChange>, Listener<TickEvent>, Listener<AutoClickerAction>, Listener<StandardEvent>, SaveSys, InitSys>
    {
        virtual std::string getSystemName() const override { return "NexusSystem"; }

        virtual void init() override;

        virtual void onEvent(const NexusButtonStateChange& event) override
        {
            auto it = std::find_if(savedButtons.begin(), savedButtons.end(), [event](const DynamicNexusButton& button) { return button.id == event.button.id; });

            if (it != savedButtons.end())
            {
                LOG_INFO("NexusButtonStateChange", "Event Button: " << event.button.id << " Button state changed: " << it->id << " archived: " << it->archived << " nb click: " << it->nbClick);

                it->archived = event.button.archived;
                it->nbClick = event.button.nbClick;
                it->active = event.button.active;

                if (it->active)
                {
                    if (not activeButton)
                        deltaTime = 0;

                    activeButton = true;
                    currentActiveButton = it.base();

                    ecsRef->sendEvent(CurrentActiveButton{std::vector<std::string>{it->id}});
                }

                LOG_INFO("NexusButtonStateChange", "Button state changed !: " << it->id << " archived: " << it->archived << " nb click: " << it->nbClick);
            }
            else
            {
                LOG_ERROR("NexusSystem", "Button: " << event.button.id << " was not found during init!");
            }
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
            
            // Track playtime statistics
            ecsRef->sendEvent(IncreaseFact{"stat_playtime_seconds", event.tick / 1000.0f});
        }

        virtual void onEvent(const AutoClickerAction& event) override;
        
        virtual void onEvent(const StandardEvent& event) override;

        virtual void execute() override;

        virtual void onRegisterFinished() override
        {
            for (auto& button : savedButtons)
            {
                auto id = button.id;
                auto it = std::find_if(initButtons.begin(), initButtons.end(), [id](const DynamicNexusButton& button) { return button.id == id; });

                if (it != initButtons.end())
                {
                    button.archived = it->archived;
                    button.nbClick = it->nbClick;
                    button.active = it->active;
                    button.activeTime = it->activeTime;
                }

                categories.insert(button.category);
            }
        }

        virtual void save(Archive& archive) override
        {
            serialize(archive, "nexusbuttons", savedButtons);
            serialize(archive, "resourceToBeDisplayed", resourceToBeDisplayed);
            serialize(archive, "tagToTierMap", tagToTierMap);
        }

        virtual void load(const UnserializedObject& serializedString) override
        {
            defaultDeserialize(serializedString, "nexusbuttons", initButtons);
            defaultDeserialize(serializedString, "resourceToBeDisplayed", resourceToBeDisplayed);
            defaultDeserialize(serializedString, "tagToTierMap", tagToTierMap);
        }

        virtual void addResourceDisplay(const std::string& res) { resourceToBeDisplayed.push_back(res); }
        
        // Prestige system methods
        void resetByPrestigeLevel(int prestigeLevel);
        void resetFactsByTags(const std::vector<std::string>& tagsToReset);
        void resetResourcesByTags(const std::vector<std::string>& tagsToReset);
        void resetGeneratorsByTags(const std::vector<std::string>& tagsToReset);
        void resetConvertersByTags(const std::vector<std::string>& tagsToReset);
        void applyStartingValues(const std::vector<std::string>& tagsToReset);
        std::vector<std::string> getFactsCreatedByTags(const std::vector<std::string>& tags) const;
        std::vector<std::string> getResourcesAffectedByTags(const std::vector<std::string>& tags) const;
        void setTagToTierMapping(const std::string& tag, int tier) { tagToTierMap[tag] = tier; }
        int getTagTier(const std::string& tag) const { 
            auto it = tagToTierMap.find(tag); 
            return (it != tagToTierMap.end()) ? it->second : -1; 
        }

        std::vector<DynamicNexusButton> initButtons;
        std::vector<DynamicNexusButton> savedButtons;
        std::vector<std::string> resourceToBeDisplayed;

        std::set<std::string> categories;

        // Prestige tag system state
        std::vector<std::string> defaultPrestigeTags;
        std::stack<std::vector<std::string>> prestigeTagStack;
        
        // Prestige tier mapping
        std::unordered_map<std::string, int> tagToTierMap = {
            {"tier0", 0},
            {"tier1", 1},
            {"tier2", 2}
        };

        size_t deltaTime = 0;

        bool activeButton = false;
        DynamicNexusButton* currentActiveButton;
        
        // Temporary storage for prestige system
        std::vector<std::string> resourcesToRemoveFromDisplay;
        std::vector<std::string> resourcesToAddToDisplay;
    };

    struct AutoClickerSystem : public System<
        Listener<TickEvent>, 
        Listener<StandardEvent>,
        SaveSys, 
        InitSys
    >
    {
        virtual std::string getSystemName() const override { return "AutoClickerSystem"; }

        virtual void init() override;
        virtual void execute() override;

        virtual void onEvent(const StandardEvent& event) override;
        virtual void onEvent(const TickEvent& event) override;

        virtual void save(Archive& archive) override;
        virtual void load(const UnserializedObject& serializedString) override;

        std::vector<AutoClicker> availableAutoClickers;  // Defined auto-clickers
        std::vector<AutoClicker> ownedAutoClickers;      // Player's auto-clickers

        void createAutoClickerFacts(const AutoClicker& clicker); // Auto-generates WorldFacts

    private:
        float getClickInterval(const AutoClicker& clicker); // Gets speed from WorldFact
        float deltaTime = 0.0f;  // Accumulated delta time from TickEvent
    };

    struct NexusScene : public Scene
    {
        virtual void init() override;

        virtual void startUp() override;

        virtual void execute() override;

        EntityRef createCategoryUI(const std::string &categoryName);

        void updateButtonsClickability(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in);
        void updateButtonsVisibility(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in, std::vector<DynamicNexusButton>& out, bool visiblility);
        void updateDynamicButtons(const std::unordered_map<std::string, ElementType>& factMap);

        std::vector<DynamicNexusButton> maskedButtons;
        std::vector<DynamicNexusButton> visibleButtons;
        std::vector<DynamicNexusButton> archivedButtons;

        EntityRef categoryList;

        std::unordered_map<std::string, EntityRef> categoryMap;

        std::unordered_map<std::string, EntityRef> activeButtonsUi;

        // Adds a resource entry to the list view.
        void addResourceDisplay(const std::string& resourceName);
        void _addResourceDisplay(const std::string& resourceName);
        void removeResourceDisplay(const std::string& resourceName);
        bool hasResourceDisplay(const std::string& resourceName) const;

        void updateRessourceView();

        std::unordered_map<std::string, EntityRef> generatorViews;

        EntityRef createGeneratorView(const RessourceGenerator& gen);
        void updateGeneratorViews();

        EntityRef resLayout;

        std::queue<std::string> resourceToBeDisplayed;

        std::unordered_map<std::string, EntityRef> tooltipsEntities;

        bool newRes = false;

        bool updateUi = false;

        // A vector storing all resource display entries.
        std::vector<ResourceDisplayEntry> resourceList;

        // Helper functions for active button time display
        std::string formatTimeRemaining(float remainingMs);
        std::string getEnhancedDescription(const DynamicNexusButton& button);
        
        // Tick counter for periodic tooltip updates
        size_t tooltipUpdateTicks = 0;
        std::string currentHoveredButtonId = "";

        ThemeInfo theme;
    };

    // Forward declarations for shared functions
    std::string floatToString(float value, int decimalPlaces = 2);
}