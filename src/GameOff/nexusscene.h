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

    // A data‐driven UI button definition.
    struct DynamicNexusButton
    {
        std::string id;                         // Unique identifier.
        std::string label;                      // Button text.
        std::vector<FactChecker> conditions;    // Conditions that must be met for the button to trigger the outcome.
        std::vector<AchievementReward> outcome; // Callback executed on click.
        std::string category;                   // Category (e.g., "Main", "Mana Upgrade", etc.)
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


    struct NexusButtonStateChange
    {
        DynamicNexusButton button;
    };

    struct NexusSystem : public System<Listener<NexusButtonStateChange>, Listener<TickEvent>, SaveSys, InitSys>
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
                // it->activeTime = event.button.activeTime;

                if (it->active)
                {
                    if (not activeButton)
                        deltaTime = 0;

                    activeButton = true;
                    currentActiveButton = it.base();
                }

                LOG_INFO("NexusButtonStateChange", "Button state changed !: " << it->id << " archived: " << it->archived << " nb click: " << it->nbClick);
            }
            else
            {
                LOG_ERROR("NexusSystem", "Button: " << event.button.id << " was not found during init!");
                // savedButtons.push_back(event.button);
            }
        }

        virtual void onEvent(const TickEvent& event) override
        {
            deltaTime += event.tick;
        }

        virtual void execute() override;

        virtual void onRegisterFinished() override
        {
            for (const auto& button : initButtons)
            {
                auto id = button.id;
                auto it = std::find_if(savedButtons.begin(), savedButtons.end(), [id](const DynamicNexusButton& button) { return button.id == id; });

                if (it != savedButtons.end())
                {
                    it->archived = button.archived;
                    it->nbClick = button.nbClick;
                    it->active = button.active;
                    it->activeTime = button.activeTime;
                }
            }
        }

        virtual void save(Archive& archive) override
        {
            serialize(archive, "nexusbuttons", savedButtons);
            serialize(archive, "resourceToBeDisplayed", resourceToBeDisplayed);
        }

        virtual void load(const UnserializedObject& serializedString) override
        {
            defaultDeserialize(serializedString, "nexusbuttons", initButtons);
            defaultDeserialize(serializedString, "resourceToBeDisplayed", resourceToBeDisplayed);
        }

        virtual void addResourceDisplay(const std::string& res) { resourceToBeDisplayed.push_back(res); }

        std::vector<DynamicNexusButton> initButtons;
        std::vector<DynamicNexusButton> savedButtons;
        std::vector<std::string> resourceToBeDisplayed;

        size_t deltaTime = 0;

        bool activeButton = false;
        DynamicNexusButton* currentActiveButton;
    };

    struct NexusScene : public Scene
    {
        virtual void init() override;

        virtual void startUp() override;

        virtual void execute() override;

        void updateButtonsClickability(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in);
        void updateButtonsVisibility(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in, std::vector<DynamicNexusButton>& out, bool visiblility);
        void updateDynamicButtons(const std::unordered_map<std::string, ElementType>& factMap);

        std::vector<DynamicNexusButton> maskedButtons;
        std::vector<DynamicNexusButton> visibleButtons;
        std::vector<DynamicNexusButton> archivedButtons;

        EntityRef nexusLayout;

        // Adds a resource entry to the list view.
        void addResourceDisplay(const std::string& resourceName) { resourceToBeDisplayed.push(resourceName); }
        void _addResourceDisplay(const std::string& resourceName);

        void updateRessourceView();

        std::unordered_map<std::string, EntityRef> generatorViews;

        EntityRef createGeneratorView(const RessourceGenerator& gen);
        void updateGeneratorViews();

        EntityRef resLayout;

        std::queue<std::string> resourceToBeDisplayed;

        std::unordered_map<std::string, EntityRef> tooltipsEntities;

        bool newRes = false;

        // A vector storing all resource display entries.
        std::vector<ResourceDisplayEntry> resourceList;

        ThemeInfo theme;
    };
}