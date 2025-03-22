#pragma once

#include "Scene/scenemanager.h"

#include "achievement.h"
#include "gamefacts.h"

namespace pg
{
    // A data‐driven UI button definition.
    struct DynamicNexusButton
    {
        std::string id;                         // Unique identifier.
        std::string label;                      // Button text.
        std::vector<FactChecker> conditions;    // Conditions that must be met for the button to trigger the outcome.
        std::vector<AchievementReward> outcome; // Callback executed on click.
        std::string category;                   // Category (e.g., "Main", "Mana Upgrade", etc.)
        std::vector<size_t> neededConditionsForVisibility; // Conditions that must be met for the button to appear. (If empty, every condition must be met)

        size_t nbClickBeforeArchive = 1;        // Number of times the button should be clicked before being archived (0 means infinite)
        std::string description = "";           // Description of the button, to be shown in the tooltip

        // Internals
        size_t nbClick = 0;                     // Number of times the button was clicked
        bool archived = false;                  // Flag indicating whether the button is archived or not.

        bool clickable = true;                  // Flag to track if the button is visible but not all conditions are met

        _unique_id entityId = 0;                // Entity identifier
        _unique_id backgroundId = 0;            // Background identifier
    };

    struct ResourceDisplayEntry
    {
        std::string resourceName;
        EntityRef uiEntity;
    };

    template <>
    void serialize(Archive& archive, const DynamicNexusButton& value);

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

        EntityRef resLayout;

        std::queue<std::string> resourceToBeDisplayed;

        std::unordered_map<std::string, EntityRef> tooltipsEntities;

        bool newRes = false;

        // A vector storing all resource display entries.
        std::vector<ResourceDisplayEntry> resourceList;
    };
}