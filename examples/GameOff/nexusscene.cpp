#include "nexusscene.h"

#include "managenerator.h"
#include "gamefacts.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"
#include "UI/prefab.h"
#include "UI/sizer.h"

#include "Systems/logmodule.h"
#include "gamemodule.h"

#include <iomanip>
#include <sstream>

#include "Systems/tween.h"

// Forward declarations for functions we'll use
std::string floatToString(float value, int decimalPlaces);

namespace pg
{
    namespace
    {
        struct OnBackgroundButtonHover
        {
            std::string buttonId;

            _unique_id id;

            bool state;
        };

        struct HideButton
        {
            std::string buttonId;
        };

        struct UpdateGenView {};

        // Helper function to check if button costs can be afforded
        bool checkButtonCosts(WorldFacts* wf, const DynamicNexusButton& button)
        {
            for (size_t i = 0; i < button.costs.size(); ++i)
            {
                const auto& cost = button.costs[i];

                float requiredAmount = cost.value;
                if (!cost.valueId.empty())
                {
                    requiredAmount = wf->getFact<float>(cost.valueId, cost.value);
                }

                if (button.costIncrease.size() > i)
                {
                    requiredAmount *= std::pow(button.costIncrease[i], button.nbClick);
                }

                if (!wf->canAfford(cost.resourceId, requiredAmount))
                {
                    return false;
                }
            }
            return true;
        }

        // Helper function to deduct button costs
        void deductButtonCosts(WorldFacts* wf, const DynamicNexusButton& button)
        {
            for (size_t i = 0; i < button.costs.size(); ++i)
            {
                const auto& cost = button.costs[i];

                if (cost.consumed)
                {
                    float requiredAmount = cost.value;
                    if (!cost.valueId.empty())
                    {
                        requiredAmount = wf->getFact<float>(cost.valueId, cost.value);
                    }

                    if (button.costIncrease.size() > i)
                    {
                        requiredAmount *= std::pow(button.costIncrease[i], button.nbClick);
                    }

                    wf->spendResource(cost.resourceId, requiredAmount);
                }
            }
        }

        // Helper function to get resource with optional max value
        std::pair<float, float> getResourceWithMax(WorldFacts* wf, const std::string& resourceName)
        {
            float value = wf->getResource(resourceName);
            float maxValue = wf->getFact<float>(resourceName + "_max_value", 0.0f);
            return {value, maxValue};
        }

        constant::Vector4D getButtonColors(ThemeInfo& info, bool clickable, bool activable, bool highlight = false)
        {
            if (clickable)
            {
                if (activable)
                {
                    return {
                        info.values["activeClickableNexusButton.r"].get<float>(),
                        info.values["activeClickableNexusButton.g"].get<float>(),
                        info.values["activeClickableNexusButton.b"].get<float>(),
                        info.values["activeClickableNexusButton.a"].get<float>()};
                }

                if (highlight)
                {
                    return {
                        info.values["hoverClickableNexusButton.r"].get<float>(),
                        info.values["hoverClickableNexusButton.g"].get<float>(),
                        info.values["hoverClickableNexusButton.b"].get<float>(),
                        info.values["hoverClickableNexusButton.a"].get<float>()};
                }
                else
                {
                    return {
                        info.values["clickableNexusButton.r"].get<float>(),
                        info.values["clickableNexusButton.g"].get<float>(),
                        info.values["clickableNexusButton.b"].get<float>(),
                        info.values["clickableNexusButton.a"].get<float>()};
                }
            }
            else
            {
                return {
                    info.values["unclickableNexusButton.r"].get<float>(),
                    info.values["unclickableNexusButton.g"].get<float>(),
                    info.values["unclickableNexusButton.b"].get<float>(),
                    info.values["unclickableNexusButton.a"].get<float>()};
            }
        }

        bool isButtonClickable(const std::unordered_map<std::string, ElementType>& factMap, const DynamicNexusButton& button)
        {
            for (const auto& condition : button.conditions)
            {
                if (!condition.check(factMap))
                {
                    return false;
                }
            }

            for (size_t i = 0; i < button.costs.size(); ++i)
            {
                const auto& cost = button.costs[i];

                auto fc = FactChecker();
                fc.name = cost.resourceId;
                fc.equality = FactCheckEquality::GreaterEqual;

                if (!cost.valueId.empty() && factMap.find(cost.valueId) != factMap.end())
                {
                    fc.value = factMap.at(cost.valueId);
                }
                else
                {
                    fc.value = cost.value;
                }

                if (button.costIncrease.size() > i && fc.value.isNumber())
                {
                    fc.value = ElementType{fc.value.get<float>() * std::pow(button.costIncrease[i], button.nbClick)};
                }

                if (!fc.check(factMap))
                {
                    return false;
                }
            }

            return true;
        }
    }

    void NexusSystem::execute()
    {
        if (activeButton and deltaTime > 0)
        {
            currentActiveButton->activeTime += deltaTime;

            WorldFacts* wf = ecsRef->getSystem<WorldFacts>();

            if (not isButtonClickable(wf->factMap, *currentActiveButton))
            {
                currentActiveButton->activeTime -= deltaTime;
                currentActiveButton->active = false;
                activeButton = false;

                ecsRef->sendEvent(CurrentActiveButton{std::vector<std::string>{}});

                return;
            }

            if (currentActiveButton->activeTime >= currentActiveButton->activationTime)
            {
                // Track task completion statistics
                wf->incrementStat("stat_tasks_completed");
                wf->incrementStat("stat_task_completions_" + currentActiveButton->id);

                // Execute outcomes
                for (const auto& outcome : currentActiveButton->outcome)
                {
                    outcome.call(ecsRef);
                }

                // Deduct costs using helper function
                if (!currentActiveButton->costs.empty())
                {
                    deductButtonCosts(wf, *currentActiveButton);
                }

                currentActiveButton->nbClick++;
                currentActiveButton->activeTime -= currentActiveButton->activationTime;

                if (currentActiveButton->nbClickBeforeArchive != 0 and currentActiveButton->nbClick >= currentActiveButton->nbClickBeforeArchive)
                {
                    currentActiveButton->archived = true;
                    currentActiveButton->active = false;
                    activeButton = false;

                    ecsRef->sendEvent(CurrentActiveButton{std::vector<std::string>{}});

                    ecsRef->sendEvent(HideButton{currentActiveButton->id});
                }
            }

            deltaTime = 0;
        }

    }

    void NexusSystem::onEvent(const AutoClickerAction& event)
    {
        LOG_INFO("AutoClickerAction", "Auto-clicker " << event.autoClickerId << " trying to click button: " << event.targetButtonId);

        // Find the button that the auto-clicker is targeting
        auto it = std::find_if(savedButtons.begin(), savedButtons.end(),
                              [&event](const DynamicNexusButton& button) { return button.id == event.targetButtonId; });

        if (it != savedButtons.end())
        {
            LOG_INFO("AutoClickerAction", "Found target button: " << it->id << " (activable: " << it->activable << ", archived: " << it->archived << ")");
            WorldFacts* wf = ecsRef->getSystem<WorldFacts>();

            // Check if the button is clickable (visibility and conditions)
            if (isButtonClickable(wf->factMap, *it) && !it->archived)
            {
                // Simulate clicking the button
                auto buttonCopy = *it;

                // Check if it's an activable button and not already active
                if (buttonCopy.activable && !buttonCopy.active && !activeButton)
                {
                    // Activate the button
                    buttonCopy.active = true;
                    buttonCopy.activeTime = 0.0f;

                    ecsRef->sendEvent(NexusButtonStateChange{buttonCopy});

                    LOG_INFO("AutoClickerSystem", "Auto-clicker " << event.autoClickerId << " activated button: " << event.targetButtonId);
                }
                else if (!buttonCopy.activable)
                {
                    // Regular button - trigger immediately
                    LOG_INFO("AutoClickerAction", "Button " << buttonCopy.id << " has " << buttonCopy.costs.size() << " costs to check");

                    // Check costs using helper function
                    bool canAfford = checkButtonCosts(wf, buttonCopy);
                    LOG_INFO("AutoClickerAction", "Button " << buttonCopy.id << " canAfford: " << canAfford);

                    if (canAfford)
                    {
                        // Deduct costs using helper function
                        deductButtonCosts(wf, buttonCopy);

                        // Execute outcomes
                        LOG_INFO("AutoClickerAction", "Executing " << buttonCopy.outcome.size() << " outcomes for button: " << buttonCopy.id);
                        for (const auto& outcome : buttonCopy.outcome)
                        {
                            outcome.call(ecsRef);
                        }

                        // Update button state
                        buttonCopy.nbClick++;

                        // Check if button should be archived
                        if (buttonCopy.nbClickBeforeArchive > 0 && buttonCopy.nbClick >= buttonCopy.nbClickBeforeArchive)
                        {
                            buttonCopy.archived = true;
                        }

                        ecsRef->sendEvent(NexusButtonStateChange{buttonCopy});

                        // Update statistics using helper methods
                        wf->incrementStat("stat_total_button_clicks");
                        wf->incrementStat("stat_clicks_" + buttonCopy.id);

                        LOG_INFO("AutoClickerSystem", "Auto-clicker " << event.autoClickerId << " clicked button: " << event.targetButtonId);
                    }
                    else
                    {
                        LOG_WARNING("AutoClickerSystem", "Auto-clicker " << event.autoClickerId << " cannot afford button: " << event.targetButtonId);
                    }
                }
                else
                {
                    LOG_WARNING("AutoClickerSystem", "Auto-clicker " << event.autoClickerId << " cannot activate button " << event.targetButtonId << " - button already active or another is active");
                }
            }
            else
            {
                LOG_WARNING("AutoClickerSystem", "Auto-clicker " << event.autoClickerId << " cannot click button " << event.targetButtonId << " - button not clickable or archived");
            }
        }
        else
        {
            LOG_ERROR("AutoClickerSystem", "Auto-clicker " << event.autoClickerId << " target button not found: " << event.targetButtonId);
        }
    }

    void NexusSystem::onEvent(const StandardEvent& event)
    {
        if (event.name == "perform_prestige")
        {
            // Extract prestige level from the event
            auto levelIt = event.values.find("level");
            if (levelIt != event.values.end()) {
                int prestigeLevel = levelIt->second.get<int>();
                resetByPrestigeLevel(prestigeLevel);
            }
        }
    }

    void NexusSystem::resetByPrestigeLevel(int prestigeLevel)
    {
        LOG_INFO("NexusSystem", "Performing prestige level " << prestigeLevel);

        // Get all tags that should be reset (current tier and below)
        std::vector<std::string> tagsToReset;
        for (const auto& [tag, tier] : tagToTierMap) {
            if (tier <= prestigeLevel) {
                tagsToReset.push_back(tag);
                LOG_INFO("NexusSystem", "Adding tag '" << tag << "' (tier " << tier << ") to reset list");
            }
        }

        // Reset WorldFacts
        resetFactsByTags(tagsToReset);

        // Reset button states
        int buttonsReset = 0;
        for (auto& button : savedButtons) {
            bool shouldReset = false;
            for (const auto& buttonTag : button.prestigeTags) {
                if (std::find(tagsToReset.begin(), tagsToReset.end(), buttonTag) != tagsToReset.end()) {
                    shouldReset = true;
                    break;
                }
            }

            if (shouldReset) {
                LOG_INFO("NexusSystem", "Resetting button: " << button.id);
                button.nbClick = 0;
                button.archived = false;
                button.active = false;
                button.activeTime = 0.0f;
                buttonsReset++;
            }
        }

        // Reset resources
        resetResourcesByTags(tagsToReset);

        // Reset generators and converters
        resetGeneratorsByTags(tagsToReset);
        resetConvertersByTags(tagsToReset);

        // Apply starting values
        applyStartingValues(tagsToReset);

        // Todo this failes + the res are invisible when we switch tabs
        // Notify UI to update after prestige
        // ecsRef->sendEvent(StandardEvent("prestige_completed"));

        // LOG_INFO("NexusSystem", "Prestige complete! Reset " << buttonsReset << " buttons and WorldFacts with tags: " << tagsToReset.size());
    }

    void NexusSystem::resetFactsByTags(const std::vector<std::string>& tagsToReset)
    {
        auto* worldFacts = ecsRef->getSystem<WorldFacts>();
        std::vector<std::string> factsToRemove;

        for (const auto& [factName, metadata] : worldFacts->factMetadata) {
            // Check if this fact was created by a button with any of the reset tags
            auto nbTagsIt = metadata.meta.find("nbTags");
            if (nbTagsIt != metadata.meta.end()) {
                int nbTags = nbTagsIt->second.get<int>();

                for (int i = 0; i < nbTags; i++) {
                    auto tagIt = metadata.meta.find("tag" + std::to_string(i));
                    if (tagIt != metadata.meta.end()) {
                        std::string tag = tagIt->second.toString();

                        if (std::find(tagsToReset.begin(), tagsToReset.end(), tag) != tagsToReset.end()) {
                            factsToRemove.push_back(factName);
                            break; // Found matching tag, no need to check more
                        }
                    }
                }
            }
        }

        // Remove the facts using WorldFacts events
        for (const std::string& factName : factsToRemove) {
            ecsRef->sendEvent(RemoveFact{factName});
            LOG_INFO("NexusSystem", "Removed fact: " << factName);
        }
    }

    std::vector<std::string> NexusSystem::getFactsCreatedByTags(const std::vector<std::string>& tags) const
    {
        auto* worldFacts = ecsRef->getSystem<WorldFacts>();
        std::vector<std::string> matchingFacts;

        for (const auto& [factName, metadata] : worldFacts->factMetadata) {
            auto nbTagsIt = metadata.meta.find("nbTags");
            if (nbTagsIt != metadata.meta.end()) {
                int nbTags = nbTagsIt->second.get<int>();

                for (int i = 0; i < nbTags; i++) {
                    auto tagIt = metadata.meta.find("tag" + std::to_string(i));
                    if (tagIt != metadata.meta.end()) {
                        std::string tag = tagIt->second.toString();

                        if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
                            matchingFacts.push_back(factName);
                            break;
                        }
                    }
                }
            }
        }

        return matchingFacts;
    }

    void NexusSystem::resetResourcesByTags(const std::vector<std::string>& tagsToReset)
    {
        auto* worldFacts = ecsRef->getSystem<WorldFacts>();

        // Get resources that were created by buttons with these tags
        std::vector<std::string> resourcesToReset = getResourcesAffectedByTags(tagsToReset);

        LOG_INFO("NexusSystem", "Starting resource reset for " << resourcesToReset.size() << " resources");

        for (const std::string& resourceName : resourcesToReset) {
            // Reset the resource value (but don't remove the fact completely)
            worldFacts->setFact(resourceName, 0.0f);
            worldFacts->setFact("total_" + resourceName, 0.0f);

            // Reset max values if they exist
            if (worldFacts->hasFact(resourceName + "_max_value")) {
                worldFacts->setFact(resourceName + "_max_value", 0.0f);
            }

            LOG_INFO("NexusSystem", "Reset resource: " << resourceName);
        }

        // Store resources to reset for UI cleanup (will be handled by prestige_completed event)
        resourcesToRemoveFromDisplay = resourcesToReset;

        LOG_INFO("NexusSystem", "Finished resource reset");
    }

    void NexusSystem::resetGeneratorsByTags(const std::vector<std::string>& tagsToReset)
    {
        auto* genSystem = ecsRef->getSystem<RessourceGeneratorSystem>();
        if (!genSystem) return;

        // Reset generators that have matching tags
        for (auto gen : genSystem->view<RessourceGenerator>()) {
            bool shouldReset = false;
            for (const auto& genTag : gen->prestigeTags) {
                if (std::find(tagsToReset.begin(), tagsToReset.end(), genTag) != tagsToReset.end()) {
                    shouldReset = true;
                    break;
                }
            }

            if (shouldReset) {
                LOG_INFO("NexusSystem", "Resetting generator: " << gen->id);
                gen->currentMana = 0.0f;
                gen->active = false;
                // Don't reset productionRate and capacity here - they'll be set by starting values
            }
        }
    }

    void NexusSystem::resetConvertersByTags(const std::vector<std::string>& tagsToReset)
    {
        auto* convSystem = ecsRef->getSystem<ConverterSystem>();
        if (!convSystem) return;

        // Reset converters that have matching tags
        for (auto conv : convSystem->view<ConverterComponent>()) {
            bool shouldReset = false;
            for (const auto& convTag : conv->prestigeTags) {
                if (std::find(tagsToReset.begin(), tagsToReset.end(), convTag) != tagsToReset.end()) {
                    shouldReset = true;
                    break;
                }
            }

            if (shouldReset) {
                LOG_INFO("NexusSystem", "Resetting converter: " << conv->id);
                conv->active = false;
                // Don't reset cost/yield here - they'll be set by starting values
            }
        }
    }

    void NexusSystem::applyStartingValues(const std::vector<std::string>& tagsToReset)
    {
        auto* worldFacts = ecsRef->getSystem<WorldFacts>();
        auto* genSystem = ecsRef->getSystem<RessourceGeneratorSystem>();
        auto* convSystem = ecsRef->getSystem<ConverterSystem>();

        LOG_INFO("NexusSystem", "Starting to apply starting values");

        // Apply starting resource values
        std::vector<std::string> resourcesToReset = getResourcesAffectedByTags(tagsToReset);

        // Store resources that should be re-displayed
        std::vector<std::string> resourcesToReDisplay;

        for (const std::string& resourceName : resourcesToReset) {
            // Apply starting values if they exist
            if (worldFacts->hasFact("starting_" + resourceName)) {
                float startingValue = worldFacts->getFact<float>("starting_" + resourceName, 0.0f);
                worldFacts->setFact(resourceName, startingValue);

                // Store for re-display if starting value > 0
                if (startingValue > 0.0f) {
                    resourcesToReDisplay.push_back(resourceName);
                }

                LOG_INFO("NexusSystem", "Applied starting value for " << resourceName << ": " << startingValue);
            }

            if (worldFacts->hasFact("starting_max_" + resourceName)) {
                float startingMaxValue = worldFacts->getFact<float>("starting_max_" + resourceName, 0.0f);
                worldFacts->setFact(resourceName + "_max_value", startingMaxValue);
                LOG_INFO("NexusSystem", "Applied starting max value for " << resourceName << ": " << startingMaxValue);
            }
        }

        // Store for later UI handling
        resourcesToAddToDisplay = resourcesToReDisplay;

        // Apply starting values for generators
        if (genSystem) {
            for (auto gen : genSystem->view<RessourceGenerator>()) {
                bool shouldReset = false;
                for (const auto& genTag : gen->prestigeTags) {
                    if (std::find(tagsToReset.begin(), tagsToReset.end(), genTag) != tagsToReset.end()) {
                        shouldReset = true;
                        break;
                    }
                }

                if (shouldReset) {
                    if (worldFacts->hasFact("starting_" + gen->id + "_productionRate")) {
                        gen->productionRate = worldFacts->getFact<float>("starting_" + gen->id + "_productionRate", 1.0f);
                    }
                    if (worldFacts->hasFact("starting_" + gen->id + "_capacity")) {
                        gen->capacity = worldFacts->getFact<float>("starting_" + gen->id + "_capacity", 100.0f);
                    }
                    if (worldFacts->hasFact("starting_" + gen->id + "_currentMana")) {
                        gen->currentMana = worldFacts->getFact<float>("starting_" + gen->id + "_currentMana", 0.0f);
                    }
                    LOG_INFO("NexusSystem", "Applied starting values for generator: " << gen->id);
                }
            }
        }

        // Apply starting values for converters
        if (convSystem) {
            for (auto conv : convSystem->view<ConverterComponent>()) {
                bool shouldReset = false;
                for (const auto& convTag : conv->prestigeTags) {
                    if (std::find(tagsToReset.begin(), tagsToReset.end(), convTag) != tagsToReset.end()) {
                        shouldReset = true;
                        break;
                    }
                }

                if (shouldReset) {
                    // Apply starting costs and yields if they exist
                    for (size_t i = 0; i < conv->cost.size(); ++i) {
                        std::string costKey = "starting_" + conv->id + "_cost" + std::to_string(i);
                        if (worldFacts->hasFact(costKey)) {
                            conv->cost[i] = worldFacts->getFact<float>(costKey, conv->cost[i]);
                        }
                    }
                    for (size_t i = 0; i < conv->yield.size(); ++i) {
                        std::string yieldKey = "starting_" + conv->id + "_yield" + std::to_string(i);
                        if (worldFacts->hasFact(yieldKey)) {
                            conv->yield[i] = worldFacts->getFact<float>(yieldKey, conv->yield[i]);
                        }
                    }
                    LOG_INFO("NexusSystem", "Applied starting values for converter: " << conv->id);
                }
            }
        }
    }

    std::vector<std::string> NexusSystem::getResourcesAffectedByTags(const std::vector<std::string>& tags) const
    {
        std::set<std::string> uniqueResources;

        // Go through all buttons with matching tags and look for add_res_display events
        for (const auto& button : savedButtons) {
            bool buttonHasTag = false;
            for (const auto& buttonTag : button.prestigeTags) {
                if (std::find(tags.begin(), tags.end(), buttonTag) != tags.end()) {
                    buttonHasTag = true;
                    break;
                }
            }

            if (buttonHasTag) {
                // Check button outcomes for add_res_display events
                for (const auto& outcome : button.outcome) {
                    if (outcome.type == AchievementRewardType::Event) {
                        const auto& event = std::get<StandardEvent>(outcome.reward);
                        if (event.name == "add_res_display") {
                            auto resIt = event.values.find("res");
                            if (resIt != event.values.end()) {
                                uniqueResources.insert(resIt->second.toString());
                            }
                        }
                    }
                }
            }
        }

        return std::vector<std::string>(uniqueResources.begin(), uniqueResources.end());
    }

    void NexusScene::init()
    {
        auto themeSys = ecsRef->getSystem<ThemeSystem>();

        if (themeSys)
        {
            theme = themeSys->getCurrentTheme();
        }
        else
        {
            LOG_ERROR("Nexus", "Couldn't load theme !");
        }

        auto nexusSys = ecsRef->getSystem<NexusSystem>();

        maskedButtons = nexusSys->savedButtons;

        for (const auto& button : maskedButtons)
        {
            LOG_INFO("Nexus Scene", "Button loaded: " << button.id << " " << button.archived << " " << button.nbClick << " " << button.nbClickBeforeArchive);
        }

        for (const auto& res : nexusSys->resourceToBeDisplayed)
        {
            resourceToBeDisplayed.push(res);
        }

        // Todo add this in the converter sys as: <id>_<res>_<cost>
        ecsRef->sendEvent( AddFact{ "scrap_converter_mana_cost", ElementType{5.0f} } );

        auto windowEnt = ecsRef->getEntity("__MainWindow");
        auto windowAnchor = windowEnt->get<UiAnchor>();

        auto listView = makeListView(this, 1, 1, 240, 1);

        auto listViewComp = listView.get<ListView>();
        listViewComp->spacing = 8;

        auto listViewUi = listView.get<UiAnchor>();

        listViewUi->setTopAnchor(windowAnchor->top);
        listViewUi->setTopMargin(120);
        listViewUi->setBottomAnchor(windowAnchor->bottom);
        listViewUi->setLeftAnchor(windowAnchor->left);
        listViewUi->setLeftMargin(10);

        resLayout = listView.entity;

        // auto categoryListView = makeListView(this, 1, 1, 350, 1);
        auto categoryListView = makeVerticalLayout(this, 1, 1, 350, 1, true);
        auto categoryView = categoryListView.get<VerticalLayout>();
        auto categoryListUi = categoryListView.get<UiAnchor>();

        // categoryView->spacing = 8;

        categoryListUi->setTopAnchor(windowAnchor->top);
        categoryListUi->setTopMargin(120);
        categoryListUi->setBottomAnchor(windowAnchor->bottom);
        categoryListUi->setLeftAnchor(listViewUi->right);
        categoryListUi->setLeftMargin(10);

        auto logViewEnt = ecsRef->getEntity("logview");
        auto logViewUi = logViewEnt->get<UiAnchor>();

        categoryListUi->setRightAnchor(logViewUi->left);

        categoryList = categoryListView.entity;

        auto harvestCat = createCategoryUI("Harvest");
        auto mainCat = createCategoryUI("Main");
        auto taskCat = createCategoryUI("Task");
        auto upgradeCat = createCategoryUI("Upgrade");

        categoryView->addEntity(harvestCat);
        categoryView->addEntity(mainCat);
        categoryView->addEntity(taskCat);
        categoryView->addEntity(upgradeCat);

        for (const auto& category : nexusSys->categories)
        {
            if (category == "Main" or category == "Upgrade" or category == "Task" or category == "Harvest")
            {
                continue;
            }

            auto cat = createCategoryUI(category);

            categoryView->addEntity(cat);
        }

        // [Begin] Tooltip Ui definition

        auto tooltipBg = makeUiSimple2DShape(this, Shape2D::Square, theme.values["tooltip.width"].get<float>(), theme.values["tooltip.height"].get<float>(), {theme.values["tooltipBg.r"].get<float>(), theme.values["tooltipBg.g"].get<float>(), theme.values["tooltipBg.b"].get<float>(), theme.values["tooltipBg.a"].get<float>()});
        tooltipBg.get<PositionComponent>()->setVisibility(false);
        tooltipBg.get<PositionComponent>()->setZ(5);
        auto tooltipBgAnchor = tooltipBg.get<UiAnchor>();

        tooltipBgAnchor->setBottomMargin(-theme.values["tooltip.bottomMargin"].get<float>());

        auto tooltipBgHighLight = makeUiSimple2DShape(this, Shape2D::Square, 0, 0, {theme.values["tooltipBgHighlight.r"].get<float>(), theme.values["tooltipBgHighlight.g"].get<float>(), theme.values["tooltipBgHighlight.b"].get<float>(), theme.values["tooltipBgHighlight.a"].get<float>()});
        tooltipBgHighLight.get<PositionComponent>()->setVisibility(false);
        tooltipBgHighLight.get<PositionComponent>()->setZ(4);
        auto tooltipBgHighLightAnchor = tooltipBgHighLight.get<UiAnchor>();

        tooltipBgHighLightAnchor->setWidthConstrain(PosConstrain{tooltipBg.entity.id, AnchorType::Width, PosOpType::Add, 2});
        tooltipBgHighLightAnchor->setHeightConstrain(PosConstrain{tooltipBg.entity.id, AnchorType::Height, PosOpType::Add, 2});

        tooltipBgHighLightAnchor->setTopAnchor(tooltipBgAnchor->top);
        tooltipBgHighLightAnchor->setTopMargin(-1);
        tooltipBgHighLightAnchor->setLeftAnchor(tooltipBgAnchor->left);
        tooltipBgHighLightAnchor->setLeftMargin(-1);

        // Description UI

        auto descTextEnt = makeTTFText(this, 0, 0, 6, theme.values["tooltipTitle.font"].get<std::string>(), "", theme.values["tooltipTitle.scale"].get<float>());
        auto descTextPos = descTextEnt.get<PositionComponent>();
        auto descText = descTextEnt.get<TTFText>();
        auto descTextAnchor = descTextEnt.get<UiAnchor>();

        descTextPos->setVisibility(false);

        descText->setWrap(true);

        descTextAnchor->setTopAnchor(tooltipBgAnchor->top);
        descTextAnchor->setTopMargin(theme.values["tooltip.topMargin"].get<float>());
        descTextAnchor->setLeftAnchor(tooltipBgAnchor->left);
        descTextAnchor->setLeftMargin(theme.values["tooltip.leftMargin"].get<float>());
        descTextAnchor->setRightAnchor(tooltipBgAnchor->right);
        descTextAnchor->setRightMargin(theme.values["tooltip.rightMargin"].get<float>());

        // Cost Spacer UI

        auto tooltipCostSpacer = makeUiSimple2DShape(this, Shape2D::Square, 0, 1, {theme.values["tooltipSpacer.r"].get<float>(), theme.values["tooltipSpacer.g"].get<float>(), theme.values["tooltipSpacer.b"].get<float>(), theme.values["tooltipSpacer.a"].get<float>()});
        tooltipCostSpacer.get<PositionComponent>()->setVisibility(false);
        tooltipCostSpacer.get<PositionComponent>()->setZ(6);
        auto tooltipCostSpacerAnchor = tooltipCostSpacer.get<UiAnchor>();

        tooltipCostSpacerAnchor->setWidthConstrain(PosConstrain{tooltipBg.entity.id, AnchorType::Width, PosOpType::Mul, theme.values["tooltipSpacer.ratio"].get<float>()});

        tooltipCostSpacerAnchor->setTopAnchor(descTextAnchor->bottom);
        tooltipCostSpacerAnchor->setTopMargin(theme.values["tooltipCostSpacer.topMargin"].get<float>());
        tooltipCostSpacerAnchor->setHorizontalCenter(tooltipBgAnchor->horizontalCenter);

        // Cost Text UI

        auto costTextEnt = makeTTFText(this, 0, 0, 6, theme.values["tooltipTitle.font"].get<std::string>(), "Cost:", theme.values["tooltipTitle.scale"].get<float>());
        auto costTextPos = costTextEnt.get<PositionComponent>();
        auto costText = costTextEnt.get<TTFText>();
        auto costTextAnchor = costTextEnt.get<UiAnchor>();

        costTextPos->setVisibility(false);

        costText->setWrap(true);

        costTextAnchor->setTopAnchor(tooltipCostSpacerAnchor->bottom);
        costTextAnchor->setTopMargin(theme.values["tooltip.topMargin"].get<float>());
        costTextAnchor->setLeftAnchor(tooltipBgAnchor->left);
        costTextAnchor->setLeftMargin(theme.values["tooltip.leftMargin"].get<float>());
        costTextAnchor->setRightAnchor(tooltipBgAnchor->right);
        costTextAnchor->setRightMargin(theme.values["tooltip.rightMargin"].get<float>());

        // Cost Values UI
        auto costValuesEnt = makeTTFText(this, 0, 0, 6, theme.values["tooltipTitle.font"].get<std::string>(), "", theme.values["tooltipTitle.scale"].get<float>());
        auto costValuesPos = costValuesEnt.get<PositionComponent>();
        auto costValuesText = costValuesEnt.get<TTFText>();
        auto costValuesAnchor = costValuesEnt.get<UiAnchor>();

        costValuesPos->setVisibility(false);
        costValuesText->setWrap(true);
        costValuesText->spacing = 2.0f;

        costValuesAnchor->setTopAnchor(costTextAnchor->bottom);
        costValuesAnchor->setTopMargin(theme.values["tooltipCostValues.topMargin"].get<float>());
        costValuesAnchor->setLeftAnchor(tooltipBgAnchor->left);
        costValuesAnchor->setLeftMargin(theme.values["tooltipCostValues.leftMargin"].get<float>());
        costValuesAnchor->setRightAnchor(tooltipBgAnchor->right);
        costValuesAnchor->setRightMargin(theme.values["tooltipCostValues.rightMargin"].get<float>());

        // Register UI elements in map

        tooltipsEntities["background"] = tooltipBg.entity;
        tooltipsEntities["backHighlight"] = tooltipBgHighLight.entity;
        tooltipsEntities["desc"] = descTextEnt.entity;
        tooltipsEntities["costSpacer"] = tooltipCostSpacer.entity;
        tooltipsEntities["costTitle"] = costTextEnt.entity;
        tooltipsEntities["costValues"] = costValuesEnt.entity;

        // -- [End] Tooltip Ui definition

        // -- [Start] Active Focus

        auto activeFocusTop = makeUiSimple2DShape(this, Shape2D::Square, theme.values["nexusbutton.width"].get<float>() * 0.85, 1, {theme.values["activeFocus.r"].get<float>(), theme.values["activeFocus.g"].get<float>(), theme.values["activeFocus.b"].get<float>(), theme.values["activeFocus.a"].get<float>()});
        auto activeFocusLeft = makeUiSimple2DShape(this, Shape2D::Square, 1, theme.values["nexusbutton.height"].get<float>() * 0.70, {theme.values["activeFocus.r"].get<float>(), theme.values["activeFocus.g"].get<float>(), theme.values["activeFocus.b"].get<float>(), theme.values["activeFocus.a"].get<float>()});
        auto activeFocusRight = makeUiSimple2DShape(this, Shape2D::Square, 1, theme.values["nexusbutton.height"].get<float>() * 0.70, {theme.values["activeFocus.r"].get<float>(), theme.values["activeFocus.g"].get<float>(), theme.values["activeFocus.b"].get<float>(), theme.values["activeFocus.a"].get<float>()});
        auto activeFocusBottom = makeUiSimple2DShape(this, Shape2D::Square, theme.values["nexusbutton.width"].get<float>() * 0.85, 1, {theme.values["activeFocus.r"].get<float>(), theme.values["activeFocus.g"].get<float>(), theme.values["activeFocus.b"].get<float>(), theme.values["activeFocus.a"].get<float>()});

        auto activeFocusTopPos = activeFocusTop.get<PositionComponent>();
        auto activeFocusTopAnchor = activeFocusTop.get<UiAnchor>();
        activeFocusTopPos->setVisibility(false);
        activeFocusTopPos->setZ(3);

        auto activeFocusLeftPos = activeFocusLeft.get<PositionComponent>();
        auto activeFocusLeftAnchor = activeFocusLeft.get<UiAnchor>();
        activeFocusLeftPos->setVisibility(false);
        activeFocusLeftPos->setZ(3);
        activeFocusLeftAnchor->setTopAnchor(activeFocusTopAnchor->top);
        activeFocusLeftAnchor->setLeftAnchor(activeFocusTopAnchor->left);

        auto activeFocusRightPos = activeFocusRight.get<PositionComponent>();
        auto activeFocusRightAnchor = activeFocusRight.get<UiAnchor>();
        activeFocusRightPos->setVisibility(false);
        activeFocusRightPos->setZ(3);
        activeFocusRightAnchor->setTopAnchor(activeFocusTopAnchor->top);
        activeFocusRightAnchor->setRightAnchor(activeFocusTopAnchor->right);

        auto activeFocusBottomPos = activeFocusBottom.get<PositionComponent>();
        auto activeFocusBottomAnchor = activeFocusBottom.get<UiAnchor>();
        activeFocusBottomPos->setVisibility(false);
        activeFocusBottomPos->setZ(3);
        activeFocusBottomAnchor->setBottomAnchor(activeFocusLeftAnchor->bottom);
        activeFocusBottomAnchor->setLeftAnchor(activeFocusTopAnchor->left);

        activeButtonsUi["topHighlight"] = activeFocusTop.entity;
        activeButtonsUi["leftHighlight"] = activeFocusLeft.entity;
        activeButtonsUi["rightHighlight"] = activeFocusRight.entity;
        activeButtonsUi["bottomHighlight"] = activeFocusBottom.entity;

        // -- [End] Active Focus

        listenToStandardEvent("add_res_display", [this](const StandardEvent& event) {
            auto res = event.values.at("res").get<std::string>();
            LOG_INFO("NexusScene", "Adding resource display: " << res);
            addResourceDisplay(res);
        });

        listenToStandardEvent("remove_res_display", [this](const StandardEvent& event) {
            auto res = event.values.at("res").get<std::string>();
            LOG_INFO("NexusScene", "Removing resource display: " << res);
            removeResourceDisplay(res);
        });

        listenToStandardEvent("add_generator", [this](const StandardEvent& event) {
            if (event.values.find("id") == event.values.end() or event.values.find("res") == event.values.end())
            {
                LOG_ERROR("NexusScene", "Event 'add_generator' received without id and res.");
                return;
            }

            auto id = event.values.at("id").get<std::string>();
            auto res = event.values.at("res").get<std::string>();

            RessourceGenerator gen(id, res);

            if (event.values.find("prod") != event.values.end())
            {
                gen.productionRate = event.values.at("prod").get<float>();
            }

            if (event.values.find("storage") != event.values.end())
            {
                gen.capacity = event.values.at("storage").get<float>();
            }

            if (event.values.find("active") != event.values.end())
            {
                gen.active = event.values.at("active").get<bool>();
            }
            else
            {
                gen.active = true;
            }

            ecsRef->sendEvent(NewGeneratorEvent{gen});
        });

        listenToStandardEvent("one_shot_res", [this](const StandardEvent& event) {
            if (event.values.find("res") == event.values.end() or (event.values.find("value") == event.values.end() and event.values.find("valueId") == event.values.end()))
            {
                LOG_ERROR("NexusScene", "Event 'one_shot_res' received without res and value.");
                return;
            }

            auto res = event.values.at("res").get<std::string>();

            WorldFacts* wf = ecsRef->getSystem<WorldFacts>();
            if (!wf) return;

            ElementType givenValue;
            if (event.values.find("value") != event.values.end())
            {
                givenValue = event.values.at("value");
            }
            else if (event.values.find("valueId") != event.values.end())
            {
                auto valueId = event.values.at("valueId").get<std::string>();
                if (wf->hasFact(valueId))
                {
                    givenValue = ElementType{wf->getFact<float>(valueId, 0.0f)};
                }
                else
                {
                    LOG_ERROR("NexusScene", "Event 'one_shot_res' received with valueId '" << valueId << "' not found.");
                    return;
                }
            }

            // Use helper function to get resource with max value
            auto [value, maxValue] = getResourceWithMax(wf, res);
            bool hasMax = maxValue > 0.0f;

            float v = 0.0f;

            if (hasMax)
            {
                auto availableSpace = maxValue - value;

                if (availableSpace <= 0)
                {
                    LOG_INFO("RessourceGeneratorHarvest", "No space left for ressource '" << res << "'");
                    return;
                }

                v = std::min(availableSpace, givenValue.get<float>());
            }
            else
            {
                v = givenValue.get<float>();
            }

            // Use WorldFacts helpers for resource updates
            wf->addResource(res, v);
            wf->addResource("total_" + res, v);

            // Track resource generation statistics
            wf->incrementStat("stat_total_resources_generated", v);
            wf->incrementStat("stat_" + res + "_generated", v);
        });

        listenToEvent<UpdateGenView>([this](const UpdateGenView&) {
            updateGeneratorViews();
        });

        listenToStandardEvent("activate_gen", [this](const StandardEvent&) {
            updateGeneratorViews();
        });

        listenToStandardEvent("prestige_completed", [this](const StandardEvent&) {
            LOG_INFO("NexusScene", "Handling prestige_completed event");

            // Sync button visibility after prestige - move unarchived buttons back to masked list
            auto nexusSys = ecsRef->getSystem<NexusSystem>();
            if (nexusSys) {
                // Handle resource display updates
                for (const auto& resourceName : nexusSys->resourcesToRemoveFromDisplay) {
                    removeResourceDisplay(resourceName);
                    removeResourceDisplay("total_" + resourceName);
                }
                for (const auto& resourceName : nexusSys->resourcesToAddToDisplay) {
                    addResourceDisplay(resourceName);
                }

                // Clear the temporary storage
                nexusSys->resourcesToRemoveFromDisplay.clear();
                nexusSys->resourcesToAddToDisplay.clear();

                // Clear current visible buttons and rebuild from savedButtons
                maskedButtons.clear();
                visibleButtons.clear();

                for (const auto& savedButton : nexusSys->savedButtons) {
                    if (!savedButton.archived) {
                        auto buttonCopy = savedButton;
                        buttonCopy.entityId = 0; // Reset entity ID so it gets recreated
                        maskedButtons.push_back(buttonCopy);
                    }
                }

                LOG_INFO("NexusScene", "Rebuilt button lists: " << maskedButtons.size() << " masked, " << visibleButtons.size() << " visible");
            }

            // Force UI update
            updateUi = true;

            LOG_INFO("NexusScene", "Finished handling prestige_completed event");
        });

        // Listen for world fact updates to log mana or upgrades.
        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate&) {
            updateGeneratorViews();
            updateUi = true;
        });

        listenToEvent<OnBackgroundButtonHover>([this](const OnBackgroundButtonHover& event) {
            auto bgEnt = ecsRef->getEntity(event.id);

            auto buttonId = event.buttonId;

            auto it = std::find_if(visibleButtons.begin(), visibleButtons.end(), [buttonId](const DynamicNexusButton& button) {
                return button.id == buttonId;
            });

            if (it == visibleButtons.end())
            {
                LOG_ERROR("NexusScene", "Button not found: " << buttonId);
                return;
            }

            if (bgEnt)
            {
                tooltipsEntities["background"]->get<PositionComponent>()->setVisibility(false);
                tooltipsEntities["backHighlight"]->get<PositionComponent>()->setVisibility(false);

                tooltipsEntities["desc"]->get<PositionComponent>()->setVisibility(false);

                tooltipsEntities["costSpacer"]->get<PositionComponent>()->setVisibility(false);
                tooltipsEntities["costTitle"]->get<PositionComponent>()->setVisibility(false);

                tooltipsEntities["costValues"]->get<PositionComponent>()->setVisibility(false);

                if (event.state)
                {
                    // Track which button is being hovered for periodic updates
                    currentHoveredButtonId = buttonId;

                    tooltipsEntities["background"]->get<PositionComponent>()->setVisibility(true);
                    tooltipsEntities["backHighlight"]->get<PositionComponent>()->setVisibility(true);

                    tooltipsEntities["background"]->get<UiAnchor>()->clearBottomAnchor();

                    if (bgEnt->has<UiAnchor>())
                    {
                        tooltipsEntities["background"]->get<UiAnchor>()->setTopAnchor(bgEnt->get<UiAnchor>()->bottom);
                        tooltipsEntities["background"]->get<UiAnchor>()->setTopMargin(-2);
                        tooltipsEntities["background"]->get<UiAnchor>()->setHorizontalCenter(bgEnt->get<UiAnchor>()->horizontalCenter);

                        tooltipsEntities["background"]->get<UiAnchor>()->setBottomAnchor(bgEnt->get<UiAnchor>()->bottom);
                    }

                    if (it->description != "")
                    {
                        tooltipsEntities["desc"]->get<PositionComponent>()->setVisibility(true);

                        // Use enhanced description with time remaining for active buttons
                        std::string enhancedDesc = getEnhancedDescription(*it);
                        tooltipsEntities["desc"]->get<TTFText>()->setText(enhancedDesc);

                        tooltipsEntities["background"]->get<UiAnchor>()->setBottomAnchor(tooltipsEntities["desc"]->get<UiAnchor>()->bottom);
                    }

                    if (it->costs.size() > 0)
                    {
                        tooltipsEntities["costSpacer"]->get<PositionComponent>()->setVisibility(true);
                        tooltipsEntities["costTitle"]->get<PositionComponent>()->setVisibility(true);
                        tooltipsEntities["costValues"]->get<PositionComponent>()->setVisibility(true);

                        std::ostringstream costText;

                        bool addEscape = false;

                        WorldFacts* wf = ecsRef->getSystem<WorldFacts>();

                        for (size_t i = 0; i < it->costs.size(); ++i)
                        {
                            if (addEscape)
                            {
                                costText << "\n";
                            }

                            const auto& cost = it->costs[i];

                            auto str = cost.resourceId;
                            str[0] = std::toupper(str[0]);

                            auto value = cost.value;

                            if (it->costIncrease.size() > i)
                            {
                                value = value * std::pow(it->costIncrease[i], it->nbClick);
                            }

                            // Check if player has enough resources using helper
                            bool hasEnoughResources = wf ? wf->canAfford(cost.resourceId, value) : false;

                            // Add color coding: green if available, red if missing
                            if (hasEnoughResources)
                            {
                                costText << "\\c{0, 255, 0, 255}" << str << ": " << value << "\\c{}";
                            }
                            else
                            {
                                costText << "\\c{255, 0, 0, 255}" << str << ": " << value << "\\c{}";
                            }

                            addEscape = true;
                        }

                        tooltipsEntities["costValues"]->get<TTFText>()->setText(costText.str());

                        tooltipsEntities["background"]->get<UiAnchor>()->setBottomAnchor(tooltipsEntities["costValues"]->get<UiAnchor>()->bottom);
                    }
                }
                else
                {
                    // Clear the hovered button ID when hover ends
                    currentHoveredButtonId = "";
                }

                if (not it->clickable)
                    return;

                if (bgEnt->has<Simple2DObject>())
                {
                    auto colors = getButtonColors(theme, true, it->activable, event.state);

                    bgEnt->get<Simple2DObject>()->setColors(colors);
                }
            }
        });

        listenToEvent<HideButton>([this](const HideButton& button) {
            auto it = std::find_if(visibleButtons.begin(), visibleButtons.end(), [button](const DynamicNexusButton& b) {
                return b.id == button.buttonId;
            });

            if (it == visibleButtons.end())
            {
                LOG_ERROR("NexusScene", "Button not found: " << button.buttonId);
                return;
            }

            auto id = it->entityId;
            auto category = it->category;

            // Archive the button and remove it from the visible buttons.
            archivedButtons.push_back(*it);
            visibleButtons.erase(it);

            auto layout = categoryMap[category]->get<HorizontalLayout>();

            layout->removeEntity(id);

            auto nbVisible = getNbVisibleElementsInLayout(layout);

            if (nbVisible <= 1)
            {
                categoryMap[category + "_main"]->get<PositionComponent>()->setVisibility(false);
            }
        });

        listenToEvent<CurrentActiveButton>([this](const CurrentActiveButton& event) {
            if (event.ids.empty())
            {
                activeButtonsUi["topHighlight"]->get<PositionComponent>()->setVisibility(false);
                activeButtonsUi["leftHighlight"]->get<PositionComponent>()->setVisibility(false);
                activeButtonsUi["rightHighlight"]->get<PositionComponent>()->setVisibility(false);
                activeButtonsUi["bottomHighlight"]->get<PositionComponent>()->setVisibility(false);
            }
            else
            {
                auto it = std::find_if(visibleButtons.begin(), visibleButtons.end(), [&event](const DynamicNexusButton& button) {
                    return button.id == event.ids[0];
                });

                if (it != visibleButtons.end())
                {
                    activeButtonsUi["topHighlight"]->get<PositionComponent>()->setVisibility(true);
                    activeButtonsUi["leftHighlight"]->get<PositionComponent>()->setVisibility(true);
                    activeButtonsUi["rightHighlight"]->get<PositionComponent>()->setVisibility(true);
                    activeButtonsUi["bottomHighlight"]->get<PositionComponent>()->setVisibility(true);

                    auto ent = ecsRef->getEntity(it->backgroundId);

                    if (ent)
                    {
                        activeButtonsUi["topHighlight"]->get<UiAnchor>()->setTopAnchor(ent->get<UiAnchor>()->top);
                        activeButtonsUi["topHighlight"]->get<UiAnchor>()->setHorizontalCenter(ent->get<UiAnchor>()->horizontalCenter);

                        activeButtonsUi["topHighlight"]->get<UiAnchor>()->setTopMargin(theme.values["nexusbutton.height"].get<float>() * 0.15f);
                    }
                }
            }
        });

        listenToStandardEvent("nexus_button_clicked", [this](const StandardEvent& event) {
            auto buttonId = event.values.at("id").get<std::string>();

            auto it = std::find_if(visibleButtons.begin(), visibleButtons.end(), [buttonId](const DynamicNexusButton& button) {
                return button.id == buttonId;
            });

            if (it == visibleButtons.end())
            {
                LOG_ERROR("NexusScene", "Button not found: " << buttonId);
                return;
            }
            else
            {
                if (not it->clickable)
                {
                    LOG_WARNING("NexusScene", "Button is not clickable: " << buttonId);
                    return;
                }

                if (it->activable)
                {
                    // Todo
                    // it->active = not it->active;
                    it->active = true;

                    ecsRef->sendEvent(NexusButtonStateChange{*it});

                    return;
                }

                // Track button click statistics using WorldFacts helpers
                WorldFacts* wf = ecsRef->getSystem<WorldFacts>();
                wf->incrementStat("stat_total_button_clicks");
                wf->incrementStat("stat_clicks_" + buttonId);

                // Todo check if all the conditions are met
                for (auto it2 : it->outcome)
                {
                    it2.call(ecsRef);
                }

                if (!it->costs.empty())
                {
                    // Deduct costs using helper function
                    deductButtonCosts(wf, *it);

                    std::ostringstream costText;
                    bool addEscape = false;

                    for (size_t i = 0; i < it->costs.size(); ++i)
                    {
                        if (addEscape)
                        {
                            costText << "\n";
                        }

                        const auto& cost = it->costs[i];

                        auto str = cost.resourceId;
                        str[0] = std::toupper(str[0]);

                        auto value = cost.value;

                        if (it->costIncrease.size() > i)
                        {
                            value = value * std::pow(it->costIncrease[i], it->nbClick + 1);
                        }

                        // Check if player has enough resources for next click using helper
                        bool hasEnoughResources = wf->canAfford(cost.resourceId, value);

                        // Add color coding: green if available, red if missing
                        if (hasEnoughResources)
                        {
                            costText << "\\c{0, 255, 0, 255}" << str << ": " << value << "\\c{}";
                        }
                        else
                        {
                            costText << "\\c{255, 0, 0, 255}" << str << ": " << value << "\\c{}";
                        }

                        addEscape = true;
                    }

                    LOG_INFO("Nexus Scene", "Cost text: " << costText.str());

                    tooltipsEntities["costValues"]->get<TTFText>()->setText(costText.str());
                }

                it->nbClick++;

                LOG_INFO("NexusScene", "Button clicked: " << buttonId << ", clicked: " << it->nbClick << ", it->id: " << it->id);

                if (it->nbClickBeforeArchive != 0 and it->nbClick >= it->nbClickBeforeArchive)
                {
                    it->archived = true;

                    ecsRef->sendEvent(NexusButtonStateChange{*it});

                    auto id = it->entityId;
                    auto category = it->category;

                    // Archive the button and remove it from the visible buttons.
                    archivedButtons.push_back(*it);
                    visibleButtons.erase(it);

                    auto layout = categoryMap[category]->get<HorizontalLayout>();

                    layout->removeEntity(id);

                    auto nbVisible = getNbVisibleElementsInLayout(layout);

                    if (nbVisible <= 1)
                    {
                        categoryMap[category + "_main"]->get<PositionComponent>()->setVisibility(false);
                    }
                }
                else
                {
                    ecsRef->sendEvent(NexusButtonStateChange{*it});
                }
            }
        });
    }

    void NexusScene::startUp()
    {
        for (auto it = maskedButtons.begin(); it != maskedButtons.end();)
        {
            if (it->archived)
            {
                archivedButtons.push_back(*it);
                it = maskedButtons.erase(it);
            }
            else
            {
                ++it;
            }
        }

        ecsRef->sendEvent(UpdateGenView{});

        updateUi = true;
    }

    void NexusScene::execute()
    {
        if (updateUi)
        {
            auto sys = ecsRef->getSystem<WorldFacts>();

            updateDynamicButtons(sys->factMap);
            updateRessourceView();

            updateUi = false;
        }

        if (newRes)
        {
            updateRessourceView();
            newRes = false;
        }

        while (not resourceToBeDisplayed.empty())
        {
            const auto& res = resourceToBeDisplayed.front();

            _addResourceDisplay(res);

            resourceToBeDisplayed.pop();

            newRes = true;
        }

        // Update tooltip for active button every 4-5 ticks
        tooltipUpdateTicks++;
        if (tooltipUpdateTicks >= 4 && !currentHoveredButtonId.empty())
        {
            tooltipUpdateTicks = 0;

            // Find the currently hovered button
            auto it = std::find_if(visibleButtons.begin(), visibleButtons.end(),
                [this](const DynamicNexusButton& button) {
                    return button.id == currentHoveredButtonId;
                });

            if (it != visibleButtons.end() && it->activable)
            {
                // Check if tooltip is currently visible
                if (tooltipsEntities["desc"]->get<PositionComponent>()->visible)
                {
                    // Update the description with current time remaining
                    std::string enhancedDesc = getEnhancedDescription(*it);
                    tooltipsEntities["desc"]->get<TTFText>()->setText(enhancedDesc);
                }
            }
        }

        // This scene could be extended to update UI, handle animations, etc.
    }

    std::string NexusScene::formatTimeRemaining(float remainingMs)
    {
        if (remainingMs <= 0) return "0:00:00:00";

        // Convert milliseconds to seconds
        int totalSeconds = static_cast<int>(std::ceil(remainingMs / 1000.0f));

        int days = totalSeconds / (24 * 3600);
        totalSeconds %= (24 * 3600);

        int hours = totalSeconds / 3600;
        totalSeconds %= 3600;

        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;

        std::ostringstream oss;
        oss << days << ":"
            << std::setfill('0') << std::setw(2) << hours << ":"
            << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds;

        return oss.str();
    }

    std::string NexusScene::getEnhancedDescription(const DynamicNexusButton& button)
    {
        std::string description = button.description;

        // Check if this is an activable button (has duration)
        if (button.activable)
        {
            auto nexusSys = ecsRef->getSystem<NexusSystem>();

            // If this button is currently active, show remaining time
            if (nexusSys && nexusSys->activeButton && nexusSys->currentActiveButton &&
                nexusSys->currentActiveButton->id == button.id)
            {
                float remainingTime = button.activationTime - nexusSys->currentActiveButton->activeTime;
                if (remainingTime > 0)
                {
                    description += "\n\n\\c{255, 165, 0, 255}Time Remaining: " + formatTimeRemaining(remainingTime) + "\\c{}";
                }
                else
                {
                    description += "\n\n\\c{0, 255, 0, 255}Completing...\\c{}";
                }
            }
            else
            {
                // Show total duration for inactive activable buttons
                description += "\n\n\\c{100, 149, 237, 255}Task Duration: " + formatTimeRemaining(button.activationTime) + "\\c{}";
            }
        }

        return description;
    }

    EntityRef createButtonPrefab(NexusScene *scene, const std::string& text, const std::string& id, DynamicNexusButton* button)
    {
        auto prefabEnt = makeAnchoredPrefab(scene);
        auto prefab = prefabEnt.get<Prefab>();
        auto prefabAnchor = prefabEnt.get<UiAnchor>();

        // Todo lookup the colors from a theme instead of hardcoded
        constant::Vector4D colors = getButtonColors(scene->theme, button->clickable, button->activable);

        colors.w = 0.0f;

        auto background = makeUiSimple2DShape(scene->ecsRef, Shape2D::Square, scene->theme.values["nexusbutton.width"].get<float>(), scene->theme.values["nexusbutton.height"].get<float>(), colors);
        auto backgroundAnchor = background.get<UiAnchor>();

        scene->ecsRef->attach<MouseLeftClickComponent>(background.entity, makeCallable<StandardEvent>("nexus_button_clicked", "id", id));

        scene->ecsRef->attach<MouseEnterComponent>(background.entity, makeCallable<OnBackgroundButtonHover>(OnBackgroundButtonHover{button->id, background.entity.id, true}));
        scene->ecsRef->attach<MouseLeaveComponent>(background.entity, makeCallable<OnBackgroundButtonHover>(OnBackgroundButtonHover{button->id, background.entity.id, false}));

        auto tweenComp = TweenComponent{
            0.0f,
            255.0f,
            350.0f,
            [background](const TweenValue& value) { background.get<Simple2DObject>()->setOpacity(std::get<float>(value)); },
        };

        tweenComp.easing = TweenQuad;

        scene->ecsRef->attach<TweenComponent>(background.entity, tweenComp);

        prefabAnchor->setWidthConstrain(PosConstrain{background.entity.id, AnchorType::Width});
        prefabAnchor->setHeightConstrain(PosConstrain{background.entity.id, AnchorType::Height});
        backgroundAnchor->setTopAnchor(prefabAnchor->top);
        backgroundAnchor->setLeftAnchor(prefabAnchor->left);

        auto ttfText = makeTTFText(scene->ecsRef, 0, 0, 1, scene->theme.values["nexusbutton.font"].get<std::string>(), text, scene->theme.values["nexusbutton.scale"].get<float>());
        auto ttfTextAnchor = ttfText.get<UiAnchor>();

        ttfTextAnchor->centeredIn(backgroundAnchor);

        prefab->addToPrefab(background.entity);
        prefab->addToPrefab(ttfText.entity);

        button->entityId = prefabEnt.entity.id;
        button->backgroundId = background.entity.id;

        return prefabEnt.entity;
    }

    // Helper function to create a category UI panel in the Nexus scene.
    EntityRef NexusScene::createCategoryUI(const std::string &categoryName)
    {
        // Create a vertical layout to hold the category title, separator, and button layout.
        auto categoryLayout = makeVerticalLayout(this, 30, 100, 500, 150); // Adjust x, y, width, height as needed.
        auto verticalLayout = categoryLayout.get<VerticalLayout>();
        categoryLayout.get<PositionComponent>()->setVisibility(false); // Initially hide the category layout.
        verticalLayout->spacing = 5; // Space between elements in the category panel.

        // 1. Category Name: Create a TTFText element for the category title.
        auto titleEnt = makeTTFText(this, 0, 0, 1, theme.values["categoryTitle.font"].get<std::string>(), categoryName, theme.values["categoryTitle.scale"].get<float>());
        // Optionally, you can adjust the anchor of the title here.
        verticalLayout->addEntity(titleEnt.entity);

        // 2. Separator Line: Create a 2D shape rectangle with a height of 1.
        float lineWidth = 500; // Adjust as needed to match category panel width.
        auto lineEnt = makeUiSimple2DShape(this, Shape2D::Square, lineWidth, 1, {200, 200, 200, 255});
        lineEnt.get<UiAnchor>()->setWidthConstrain(PosConstrain{categoryList.id, AnchorType::Width, PosOpType::Mul, 0.9f});
        verticalLayout->addEntity(lineEnt.entity);

        // 3. Horizontal Layout for Buttons: Create an HLayout that will hold the buttons.
        auto buttonLayoutEnt = makeHorizontalLayout(this, 0, 0, lineWidth, 60); // Adjust height as needed.
        buttonLayoutEnt.get<UiAnchor>()->setWidthConstrain(PosConstrain{categoryList.id, AnchorType::Width, PosOpType::Mul, 0.9f});
        auto buttonLayout = buttonLayoutEnt.get<HorizontalLayout>();
        buttonLayout->spacing = 10;
        buttonLayout->fitToAxis = true;
        buttonLayout->spaced = false;

        // buttonLayout->setVisibility(false); // Initially hide the button layout.

        categoryMap[categoryName] = buttonLayoutEnt.entity;

        categoryMap[categoryName + "_main"] = categoryLayout.entity;

        // Add the horizontal button layout to the vertical category layout.
        verticalLayout->addEntity(buttonLayoutEnt.entity);

        // Return the category layout entity for further positioning or manipulation.
        return categoryLayout.entity;
    }

    void NexusScene::updateButtonsClickability(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in)
    {
        for (auto& button : in)
        {
            if (button.neededConditionsForVisibility.empty() and button.costs.empty())
            {
                continue;
            }

            bool clickable = isButtonClickable(factMap, button);

            if (button.clickable != clickable)
            {
                button.clickable = clickable;

                auto background = ecsRef->getEntity(button.backgroundId);
                if (not background or not background->has<PositionComponent>())
                {
                    LOG_ERROR("Nexus scene", "Background: " << button.backgroundId << " is not in a valid state!");
                    continue;
                }

                auto colors = getButtonColors(theme, clickable, button.activable);

                if (background->has<Simple2DObject>())
                    background->get<Simple2DObject>()->setColors(colors);
            }
        }
    }

    void NexusScene::updateButtonsVisibility(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in, std::vector<DynamicNexusButton>& out, bool visiblility)
    {
        for (auto it = in.begin(); it != in.end();)
        {
            bool reveal = true;
            bool clickable = false;

            if (it->neededConditionsForVisibility.empty())
            {
                for (const auto& it2 : it->conditions)
                {
                    if (not it2.check(factMap))
                    {
                        reveal = false;
                        break;
                    }
                }
            }
            else
            {
                // Check if needed conditions for visibility are met
                for (size_t idx : it->neededConditionsForVisibility)
                {
                    if (idx >= it->conditions.size() or not it->conditions[idx].check(factMap))
                    {
                        reveal = false;
                        break;
                    }
                }
            }

            if (visiblility == true)
            {
                clickable = isButtonClickable(factMap, *it.base());
            }

            if (reveal == visiblility)
            {
                if (visiblility)
                {
                    if (it->entityId != 0)
                    {
                        LOG_ERROR("Nexuse scene", "Button id: " << it->id << " should not have an entity already has we are creating right now!");
                    }
                    else
                    {
                        it->clickable = clickable;
                        // Create a new entity for the button
                        auto buttonEntity = createButtonPrefab(this, it->label, it->id, it.base());

                        categoryMap[it->category]->get<HorizontalLayout>()->addEntity(buttonEntity);

                        categoryMap[it->category + "_main"]->get<PositionComponent>()->setVisibility(true);
                    }
                }
                else
                {
                    if (it->entityId == 0)
                    {
                        LOG_ERROR("Nexus scene", "Button id: " << it->id << " should have an entity but it does not!");
                    }
                    else
                    {
                        LOG_INFO("Nexus scene", "Button id: " << it->entityId);
                        auto category = it->category;
                        auto layout = categoryMap[category]->get<HorizontalLayout>();

                        layout->removeEntity(it->entityId);

                        it->entityId = 0;

                        auto nbVisible = getNbVisibleElementsInLayout(layout);

                        if (nbVisible <= 1)
                        {
                            categoryMap[category + "_main"]->get<PositionComponent>()->setVisibility(false);
                        }
                    }
                }

                out.push_back(*it);  // Move button to visible
                it = in.erase(it);   // Remove from masked list
            }
            else
            {
                ++it;
            }
        }
    }

    void NexusScene::updateDynamicButtons(const std::unordered_map<std::string, ElementType>& factMap)
    {
        updateButtonsClickability(factMap, visibleButtons);
        updateButtonsVisibility(factMap, maskedButtons, visibleButtons, true);
        updateButtonsVisibility(factMap, visibleButtons, maskedButtons, false);

        // Todo remove this
        ecsRef->sendEvent(SkipRenderPass{4});

        // Update value cost of visible buttons
        for (auto& button : visibleButtons)
        {
            for (size_t i = 0; i < button.costs.size(); ++i)
            {
                auto& cost = button.costs[i];

                if (not cost.valueId.empty() and factMap.find(cost.valueId) != factMap.end()) {
                    cost.value = factMap.at(cost.valueId).get<float>();
                }
            }
        }
    }

    void NexusScene::addResourceDisplay(const std::string& resourceName)
    {
        // Check if resource is already displayed to prevent duplicates
        if (!hasResourceDisplay(resourceName)) {
            resourceToBeDisplayed.push(resourceName);
        }
    }

    // Helper method to add a new resource display entry.
    void NexusScene::_addResourceDisplay(const std::string& resourceName)
    {
        // Create a new UI text entity using your existing TTFText helper.
        // We start with an empty text; it'll be updated in execute().
        auto textEntity = makeTTFText(this, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), "", theme.values["resourcedisplay.scale"].get<float>());
        textEntity.get<PositionComponent>()->setVisibility(false);

        resLayout->get<ListView>()->addEntity(textEntity.entity);

        // Create a ResourceDisplayEntry and add it to the list.
        ResourceDisplayEntry entry;
        entry.resourceName = resourceName;
        entry.uiEntity = textEntity.entity;
        resourceList.push_back(entry);
    }

    void NexusScene::removeResourceDisplay(const std::string& resourceName)
    {
        // Find the resource in the display list
        auto it = std::find_if(resourceList.begin(), resourceList.end(),
            [&resourceName](const ResourceDisplayEntry& entry) {
                return entry.resourceName == resourceName;
            });

        if (it != resourceList.end()) {
            // Remove from ListView
            if (resLayout && resLayout->has<ListView>()) {
                resLayout->get<ListView>()->removeEntity(it->uiEntity);
            }

            // Remove from resource list
            resourceList.erase(it);
        }
    }

    bool NexusScene::hasResourceDisplay(const std::string& resourceName) const
    {
        return std::find_if(resourceList.begin(), resourceList.end(),
            [&resourceName](const ResourceDisplayEntry& entry) {
                return entry.resourceName == resourceName;
            }) != resourceList.end();
    }

    void NexusScene::updateRessourceView()
    {
        // Update the resource list view.
        WorldFacts* wf = ecsRef->getSystem<WorldFacts>();
        if (not wf) return;

        for (auto& entry : resourceList)
        {
            // Use helper function to get resource with max value
            auto [value, maxValue] = getResourceWithMax(wf, entry.resourceName);
            bool hasMax = maxValue > 0.0f;

            std::ostringstream oss;
            oss << entry.resourceName << ": " << value;

            if (hasMax)
                oss << "/" << maxValue;

            // Update the text of the corresponding TTFText component.
            if (entry.uiEntity and entry.uiEntity->has<TTFText>())
            {
                auto textComp = entry.uiEntity->get<TTFText>();

                auto str = oss.str();
                str[0] = std::toupper(str[0]);
                textComp->setText(str);
            }
        }
    }

    EntityRef NexusScene::createGeneratorView(const RessourceGenerator& gen)
    {
        auto prefabEnt = makeAnchoredPrefab(this);
        auto prefab = prefabEnt.get<Prefab>();
        auto prefabAnchor = prefabEnt.get<UiAnchor>();

        auto genEnt = makeTTFText(ecsRef, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), gen.id, theme.values["resourcedisplay.scale"].get<float>());
        auto genEntAnchor = genEnt.get<UiAnchor>();

        genEntAnchor->setTopAnchor(prefabAnchor->top);
        genEntAnchor->setLeftAnchor(prefabAnchor->left);
        genEntAnchor->setLeftMargin(2);

        auto genResEnt = makeTTFText(ecsRef, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), gen.ressource, theme.values["resourcedisplay.scale"].get<float>());
        auto genResEntAnchor = genResEnt.get<UiAnchor>();

        genResEntAnchor->setTopAnchor(genEntAnchor->bottom);
        genResEntAnchor->setTopMargin(5);
        genResEntAnchor->setLeftAnchor(prefabAnchor->left);
        genResEntAnchor->setLeftMargin(15);

        auto genProdEnt = makeTTFText(ecsRef, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), floatToString(gen.productionRate, 2), theme.values["resourcedisplay.scale"].get<float>());
        auto genProdEntAnchor = genProdEnt.get<UiAnchor>();

        genProdEntAnchor->setBottomAnchor(genResEntAnchor->bottom);
        // genProdEntAnchor->setRightAnchor(prefabAnchor->right);
        genProdEntAnchor->setLeftAnchor(genResEntAnchor->right);
        genProdEntAnchor->setLeftMargin(5);

        auto genCapaEnt = makeTTFText(ecsRef, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), "/" + std::to_string(int(gen.capacity)), theme.values["resourcedisplay.scale"].get<float>());
        auto genCapaEntAnchor = genCapaEnt.get<UiAnchor>();

        genCapaEntAnchor->setBottomAnchor(genResEntAnchor->bottom);
        genCapaEntAnchor->setRightAnchor(prefabAnchor->right);
        genCapaEntAnchor->setRightMargin(20);

        auto genCurrentEnt = makeTTFText(ecsRef, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), std::to_string(int(gen.currentMana)), theme.values["resourcedisplay.scale"].get<float>());
        auto genCurrentEntAnchor = genCurrentEnt.get<UiAnchor>();

        genCurrentEntAnchor->setBottomAnchor(genResEntAnchor->bottom);
        genCurrentEntAnchor->setRightAnchor(genCapaEntAnchor->left);
        // genCurrentEntAnchor->setRightMargin(5);

        prefabAnchor->setBottomAnchor(genResEntAnchor->bottom);

        generatorViews[gen.id] = prefabEnt.entity;
        generatorViews[gen.id + "_gen"] = genEnt.entity;
        generatorViews[gen.id + "_res"] = genResEnt.entity;
        generatorViews[gen.id + "_prod"] = genProdEnt.entity;
        generatorViews[gen.id + "_capa"] = genCapaEnt.entity;
        generatorViews[gen.id + "_current"] = genCurrentEnt.entity;

        prefab->addToPrefab(genEnt.entity);
        prefab->addToPrefab(genResEnt.entity);
        prefab->addToPrefab(genProdEnt.entity);
        prefab->addToPrefab(genCapaEnt.entity);
        prefab->addToPrefab(genCurrentEnt.entity);

        return prefabEnt.entity;
    }

    void NexusScene::updateGeneratorViews()
    {
        auto generators = ecsRef->view<RessourceGenerator>();

        for (const auto& gen : generators)
        {
            // Check if a view for this generator already exists.
            if (generatorViews.find(gen->id) == generatorViews.end())
            {
                // Create a new generator view UI element.
                EntityRef viewEnt = createGeneratorView(*gen);
                auto anchor = viewEnt->get<UiAnchor>();
                auto layoutAnchor = resLayout->get<UiAnchor>();

                anchor->setLeftAnchor(layoutAnchor->left);

                anchor->setWidthConstrain(PosConstrain{layoutAnchor.entityId, AnchorType::Width});

                // anchor->setRightAnchor(layoutAnchor->right);

                // Optionally add the view to a ListView layout dedicated to generators.
                resLayout->get<ListView>()->addEntity(viewEnt);
            }

            auto ent1 = generatorViews[gen->id + "_gen"];
            auto ent2 = generatorViews[gen->id + "_res"];
            auto ent3 = generatorViews[gen->id + "_prod"];
            auto ent4 = generatorViews[gen->id + "_capa"];
            auto ent5 = generatorViews[gen->id + "_current"];

            if (ent1 and ent1->has<TTFText>())
            {
                auto textComp = ent1->get<TTFText>();
                textComp->setText(gen->id);
            }

            if (ent2 and ent2->has<TTFText>())
            {
                auto textComp = ent2->get<TTFText>();
                textComp->setText(gen->ressource);
            }

            if (ent3 and ent3->has<TTFText>())
            {
                auto textComp = ent3->get<TTFText>();
                textComp->setText(floatToString(gen->productionRate, 2));
            }

            if (ent4 and ent4->has<TTFText>())
            {
                auto textComp = ent4->get<TTFText>();
                textComp->setText(" / " + std::to_string(int(gen->capacity)));
            }

            if (ent5 and ent5->has<TTFText>())
            {
                auto textComp = ent5->get<TTFText>();
                textComp->setText(std::to_string(int(gen->currentMana)));
            }

            auto prefab = generatorViews[gen->id];

            if (prefab and prefab->has<Prefab>())
            {
                if (gen->active)
                {
                    // Todo a change of visibility of the position component should trigger the set visibility of the prefab component
                    prefab->get<Prefab>()->setVisibility(true);
                    // prefab->get<PositionComponent>()->setVisibility(true);
                }
                else
                {
                    prefab->get<Prefab>()->setVisibility(false);
                    // prefab->get<PositionComponent>()->setVisibility(false);
                }
            }
        }
    }

    // AutoClickerSystem implementation
    void AutoClickerSystem::init()
    {
        // Register StandardEvent types we listen to
        addListenerToStandardEvent("purchase_autoclicker");
        addListenerToStandardEvent("toggle_autoclicker");

        // Initialize global auto-clicker multiplier if it doesn't exist
        auto* factSys = ecsRef->getSystem<WorldFacts>();
        factSys->setFactIfNotExists("autoclicker_global_multiplier", 1.0f);
    }

    void AutoClickerSystem::execute()
    {
        // Process owned auto-clickers that are active
        for (auto& clicker : ownedAutoClickers)
        {
            if (clicker.active && clicker.owned)
            {
                clicker.timer += deltaTime;

                float interval = getClickInterval(clicker);
                if (clicker.timer >= interval)
                {
                    // Send auto-click event
                    LOG_INFO("AutoClickerSystem", "Auto-clicker " << clicker.id << " firing! Timer: " << clicker.timer << ", Interval: " << interval);
                    ecsRef->sendEvent(AutoClickerAction{clicker.id, clicker.targetButtonId});

                    // Reset timer AFTER logging
                    clicker.timer = 0.0f;

                    // Update statistics
                    clicker.clickCount++;
                    auto* factSys = ecsRef->getSystem<WorldFacts>();
                    factSys->incrementStat("stat_autoclicker_clicks_" + clicker.id);
                    factSys->incrementStat("stat_total_autoclicker_clicks");
                }
            }
        }

        // Reset deltaTime after processing all auto-clickers
        deltaTime = 0;
    }

    void AutoClickerSystem::onEvent(const StandardEvent& event)
    {
        if (event.name == "purchase_autoclicker")
        {
            auto autoClickerId = event.values.at("id").get<std::string>();

            // Find the auto-clicker definition
            auto it = std::find_if(availableAutoClickers.begin(), availableAutoClickers.end(),
                                  [&autoClickerId](const AutoClicker& clicker) { return clicker.id == autoClickerId; });

            if (it != availableAutoClickers.end())
            {
                auto* factSys = ecsRef->getSystem<WorldFacts>();

                // Check if already owned using WorldFacts helper
                if (factSys->getFact<bool>(it->id + "_owned", false))
                {
                    LOG_WARNING("AutoClickerSystem", "Auto-clicker " << it->id << " is already owned");
                    return;
                }

                // Check costs and conditions
                bool canPurchase = true;

                // Check unlock conditions
                for (const auto& condition : it->unlockConditions)
                {
                    if (!condition.check(factSys->factMap))
                    {
                        canPurchase = false;
                        break;
                    }
                }

                // Check costs using WorldFacts helpers
                if (canPurchase)
                {
                    for (const auto& cost : it->costs)
                    {
                        float requiredAmount = cost.value;
                        if (!cost.valueId.empty())
                        {
                            requiredAmount = factSys->getFact<float>(cost.valueId, cost.value);
                        }

                        if (!factSys->canAfford(cost.resourceId, requiredAmount))
                        {
                            canPurchase = false;
                            break;
                        }
                    }
                }

                if (canPurchase)
                {
                    // Deduct costs using WorldFacts helpers
                    for (const auto& cost : it->costs)
                    {
                        if (cost.consumed)
                        {
                            float requiredAmount = cost.value;
                            if (!cost.valueId.empty())
                            {
                                requiredAmount = factSys->getFact<float>(cost.valueId, cost.value);
                            }
                            factSys->spendResource(cost.resourceId, requiredAmount);
                        }
                    }

                    // Add to owned auto-clickers
                    AutoClicker ownedClicker = *it;
                    ownedClicker.owned = true;
                    ownedClicker.active = true; // Start active by default!
                    ownedAutoClickers.push_back(ownedClicker);

                    // Set ownership fact
                    factSys->setFact(it->id + "_owned", true);

                    // Create auto-clicker specific facts
                    createAutoClickerFacts(ownedClicker);

                    LOG_INFO("AutoClickerSystem", "Purchased and activated auto-clicker: " << it->id << " (targeting: " << it->targetButtonId << ")");
                }
                else
                {
                    LOG_WARNING("AutoClickerSystem", "Cannot purchase auto-clicker " << it->id << " - requirements not met");
                }
            }
            else
            {
                LOG_ERROR("AutoClickerSystem", "Auto-clicker definition not found: " << autoClickerId);
            }
        }
        else if (event.name == "toggle_autoclicker")
        {
            auto autoClickerId = event.values.at("id").get<std::string>();
            bool enable = event.values.at("enable").get<bool>();

            auto it = std::find_if(ownedAutoClickers.begin(), ownedAutoClickers.end(),
                                  [&autoClickerId](const AutoClicker& clicker) { return clicker.id == autoClickerId; });

            if (it != ownedAutoClickers.end())
            {
                if (it->owned)
                {
                    it->active = enable;
                    it->timer = 0.0f; // Reset timer when toggling

                    LOG_INFO("AutoClickerSystem", "Auto-clicker " << it->id << " is now " << (it->active ? "active" : "inactive") << " (targeting: " << it->targetButtonId << ")");
                }
                else
                {
                    LOG_WARNING("AutoClickerSystem", "Cannot toggle auto-clicker " << it->id << " - not owned");
                }
            }
            else
            {
                LOG_ERROR("AutoClickerSystem", "Owned auto-clicker not found: " << autoClickerId);
            }
        }
    }

    void AutoClickerSystem::onEvent(const TickEvent& event)
    {
        deltaTime += event.tick; // event.tick is already in milliseconds
    }

    void AutoClickerSystem::save(Archive& archive)
    {
        serialize(archive, "availableAutoClickers", availableAutoClickers);
        serialize(archive, "ownedAutoClickers", ownedAutoClickers);
    }

    void AutoClickerSystem::load(const UnserializedObject& serializedString)
    {
        defaultDeserialize(serializedString, "availableAutoClickers", availableAutoClickers);
        defaultDeserialize(serializedString, "ownedAutoClickers", ownedAutoClickers);
    }

    float AutoClickerSystem::getClickInterval(const AutoClicker& clicker)
    {
        auto* factSys = ecsRef->getSystem<WorldFacts>();

        float baseInterval = factSys->getFact<float>("autoclicker_interval_" + clicker.id, clicker.baseInterval);
        float individualMultiplier = factSys->getFact<float>("autoclicker_multiplier_" + clicker.id, 1.0f);
        float globalMultiplier = factSys->getFact<float>("autoclicker_global_multiplier", 1.0f);

        return baseInterval / (individualMultiplier * globalMultiplier);  // Smaller = faster
    }

    void AutoClickerSystem::createAutoClickerFacts(const AutoClicker& clicker)
    {
        auto* factSys = ecsRef->getSystem<WorldFacts>();

        // Create auto-clicker facts using WorldFacts helpers
        factSys->setFactIfNotExists("autoclicker_interval_" + clicker.id, clicker.baseInterval);
        factSys->setFactIfNotExists("autoclicker_multiplier_" + clicker.id, 1.0f);
        factSys->setFactIfNotExists("stat_autoclicker_clicks_" + clicker.id, 0.0f);
    }
}