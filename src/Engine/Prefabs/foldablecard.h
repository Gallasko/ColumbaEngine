#pragma once

#include "ECS/system.h"

#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/sizer.h"

namespace pg
{
    /**
     * @brief Event to toggle the folded/expanded state of a foldable card.
     * 
     * This event is typically triggered by user interaction (such as clicking
     * the card's title bar) and causes the card to toggle between collapsed
     * and expanded states by showing/hiding its content area.
     * 
     * The event contains the ID of the foldable card entity that should
     * have its state toggled.
     * 
     * @see FoldCardSystem::onEvent()
     * @see makeFoldableCard()
     */
    struct FoldCardEvent
    {
        _unique_id id;  ///< ID of the foldable card entity to toggle
    };

    /**
     * @brief System that handles foldable card toggle events.
     * 
     * The FoldCardSystem processes FoldCardEvent events to toggle the
     * visibility state of foldable card contents. When a fold event is
     * received, it calls the "toggleCard" helper function on the target
     * card's Prefab component.
     * 
     * ## Functionality
     * - Listens for FoldCardEvent events
     * - Validates that target entities exist and have Prefab components
     * - Delegates toggle logic to the card's "toggleCard" helper function
     * 
     * @see FoldCardEvent
     * @see makeFoldableCard()
     */
    struct FoldCardSystem : public System<Listener<FoldCardEvent>>
    {
        /**
         * @brief Handles fold card toggle events.
         * 
         * When a FoldCardEvent is received, this method:
         * 1. Retrieves the target entity by ID
         * 2. Validates the entity exists and has a Prefab component
         * 3. Calls the "toggleCard" helper function to toggle visibility
         * 
         * @param event The fold card event containing the target entity ID
         * 
         * @note If the entity doesn't exist or lacks a Prefab component,
         *       the event is silently ignored.
         */
        void onEvent(const FoldCardEvent& event)
        {
            auto ent = ecsRef->getEntity(event.id);

            if (not ent or not ent->has<Prefab>())
                return;

            ent->get<Prefab>()->callHelper("toggleCard");
        }
    };


    /**
     * @brief Factory function to create a foldable card UI component.
     * 
     * Creates a complete foldable card interface consisting of:
     * 
     * ## Structure
     * - **Title Bar**: Clickable header with card title and background
     * - **Content Area**: Collapsible vertical layout for card contents
     * - **Visual Indicators**: Left border line that shows/hides with content
     * 
     * ## Components Created
     * - Main prefab entity with Prefab, UiAnchor, and layout components
     * - Title background (clickable, triggers fold/unfold)
     * - Title text element
     * - Content layout (initially collapsed)
     * - Visual separator line
     * 
     * ## Behavior
     * - Clicking the title bar toggles the card between folded/expanded states
     * - Content area and separator line visibility toggle together
     * - Card maintains its width but adjusts height based on state
     * 
     * ## Usage Example
     * ```cpp
     * auto card = makeFoldableCard(ecs, "Settings");
     * auto layout = card.get<VerticalLayout>();
     * 
     * // Add content to the card
     * auto textElement = makeText(ecs, "Card content here");
     * layout->addEntity(textElement);
     * ```
     * 
     * @tparam Type ECS system type
     * @param ecsRef Pointer to the ECS system
     * @param cardTitle Title text to display in the card header
     * @return ComponentList containing the main entity and key components
     * 
     * @see FoldCardEvent
     * @see FoldCardSystem
     * @see Prefab
     * 
     * @note The returned layout is the content area where you should add
     *       your card contents, not the main card container.
     * 
     * @todo Fix visibility issue where card children are invisible without
     *       adding a dummy element to the layout.
     */
    template <typename Type>
    CompList<Prefab, UiAnchor, VerticalLayout> makeFoldableCard(Type *ecsRef, const std::string& cardTitle = "New Card")
    {
        auto prefabEnt = makeAnchoredPrefab(ecsRef, 100, 100, 1);
        auto prefab = prefabEnt.template get<Prefab>();
        auto prefabAnchor = prefabEnt.template get<UiAnchor>();

        auto mainLayoutEnt = makeVerticalLayout(ecsRef, 0, 0, 250, 0);
        auto mainLayout = mainLayoutEnt.template get<VerticalLayout>();
        auto mainLayoutAnchor = mainLayoutEnt.template get<UiAnchor>();
        mainLayout->setScrollable(false);

        prefabAnchor->setWidthConstrain(PosConstrain{mainLayout.entityId, AnchorType::Width});
        prefabAnchor->setHeightConstrain(PosConstrain{mainLayout.entityId, AnchorType::Height});

        mainLayoutAnchor->setTopAnchor(prefabAnchor->top);
        mainLayoutAnchor->setLeftAnchor(prefabAnchor->left);
        mainLayoutAnchor->setZConstrain(PosConstrain{prefabEnt.id, AnchorType::Z});

        prefab->addToPrefab(mainLayoutEnt, "MainEntity");

        auto titleBg = makeUiSimple2DShape(ecsRef, Shape2D::Square, 250, 50, {55, 55, 125, 255});
        titleBg.template attach<MouseLeftClickComponent>(makeCallable<FoldCardEvent>(FoldCardEvent{prefabEnt.id}));
        auto titleBgAnchor = titleBg.template get<UiAnchor>();

        auto title = makeTTFText(ecsRef, 0, 0, 2, "light", cardTitle, 0.4f);
        auto titleAnchor = title.template get<UiAnchor>();

        prefab->addToPrefab(title, "CardTitle");

        titleAnchor->setVerticalCenter(titleBgAnchor->verticalCenter);
        titleAnchor->setLeftAnchor(titleBgAnchor->left);
        titleAnchor->setLeftMargin(5);

        auto leftLineEnt = makeUiSimple2DShape(ecsRef, Shape2D::Square, 2, 2, {180, 180, 180, 255});
        auto leftLineAnchor = leftLineEnt.template get<UiAnchor>();

        auto layoutEnt = makeVerticalLayout(ecsRef, 0, 0, 250, 100);
        auto layout = layoutEnt.template get<VerticalLayout>();
        // layout->fitToAxis = true;
        layout->setScrollable(false);
        auto layoutAnchor = layoutEnt.template get<UiAnchor>();

        layoutAnchor->setZConstrain(PosConstrain{mainLayoutEnt.entity.id, AnchorType::Z, PosOpType::Add, 1});

        leftLineAnchor->setHeightConstrain(PosConstrain{layoutEnt.entity.id, AnchorType::Height});

        layoutAnchor->setTopAnchor(titleBgAnchor->bottom);
        layoutAnchor->setLeftAnchor(leftLineAnchor->right);
        layoutAnchor->setLeftMargin(5);

        prefab->addToPrefab(layoutEnt, "Layout");

        // Todo fix this (without the test here the child of the layout are invisible !)
        // auto test1 = makeTTFText(ecsRef, 0, 0, 2, "light", "Test 1", 0.5);
        // auto test2 = makeTTFText(ecsRef, 0, 0, 2, "light", "Test 2", 0.5);

        // layout->addEntity(test1);
        // layout->addEntity(test2);

        // Todo fix this this is a dirty fix for now
        layout->addEntity(makeSimple2DShape(ecsRef, Shape2D::Square, 1, 1));

        mainLayout->addEntity(titleBg);
        mainLayout->addEntity(leftLineEnt);
        // mainLayout->addEntity(layoutEnt);

        prefab->addHelper("toggleCard", [](Prefab *prefab) -> void {
            LOG_INFO("Foldable card", "Fold");

            auto leftLine = prefab->getEntity("MainEntity")->get<VerticalLayout>()->entities[1];

            auto leftLinePos = leftLine->template get<PositionComponent>();

            auto visi = leftLinePos->isVisible();

            leftLinePos->setVisibility(not visi);

            auto vLayoutEnt = prefab->getEntity("Layout");

            auto vLayoutPos = vLayoutEnt->template get<PositionComponent>();

            vLayoutPos->setVisibility(not visi);
        });

        return {prefabEnt, prefab, prefabAnchor, layout};
    }
}