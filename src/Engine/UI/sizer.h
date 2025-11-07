#pragma once

#include "ECS/system.h"

#include "2D/texture.h"

namespace pg
{
    /**
     * @brief Enumeration defining layout orientation types.
     * 
     * Specifies whether a layout arranges its child elements horizontally or vertically.
     * Used by BaseLayout and its derived classes to determine element positioning.
     * 
     * @see BaseLayout
     * @see HorizontalLayout
     * @see VerticalLayout
     */
    enum class LayoutOrientation
    {
        Horizontal, ///< Elements arranged horizontally (left to right)
        Vertical    ///< Elements arranged vertically (top to bottom)
    };

    /**
     * @brief Event to clear multiple entities from layouts.
     * 
     * Removes all specified entities from their respective layouts and destroys them.
     * This is a batch operation for efficiently clearing multiple entities at once.
     * 
     * @see LayoutSystem::onProcessEvent(const ClearLayoutEvent&)
     * @see BaseLayout::clear()
     */
    struct ClearLayoutEvent
    {
        std::vector<_unique_id> entityIds; ///< IDs of entities to remove and destroy
    };

    /**
     * @brief Event to add an element to the end of a layout.
     * 
     * Adds the specified UI entity to the end of the layout with the given ID.
     * The element is positioned according to the layout's orientation and settings.
     * 
     * @see LayoutSystem::onProcessEvent(const AddLayoutElementEvent&)
     * @see BaseLayout::addEntity()
     */
    struct AddLayoutElementEvent
    {
        _unique_id id;          ///< ID of the layout entity
        _unique_id ui;          ///< ID of the UI entity to add
        LayoutOrientation orientation; ///< Layout orientation (horizontal/vertical)
    };

    /**
     * @brief Event to insert an element at a specific position in a layout.
     * 
     * Inserts the specified UI entity at the given index within the layout.
     * Negative indices count from the end (-1 = last position).
     * 
     * @see LayoutSystem::onProcessEvent(const InsertLayoutElementEvent&)
     * @see BaseLayout::insertEntity()
     */
    struct InsertLayoutElementEvent
    {
        _unique_id id;          ///< ID of the layout entity
        _unique_id ui;          ///< ID of the UI entity to insert
        LayoutOrientation orientation; ///< Layout orientation (horizontal/vertical)
        int index;              ///< Position to insert at (negative = from end)
    };

    /**
     * @brief Event to remove a specific entity from a layout by ID.
     * 
     * Removes the entity with the specified ID from the layout and destroys it.
     * The entity is searched for within the layout's entities list.
     * 
     * @see LayoutSystem::onProcessEvent(const RemoveLayoutElementEvent&)
     * @see BaseLayout::removeEntity()
     */
    struct RemoveLayoutElementEvent
    {
        _unique_id id;          ///< ID of the layout entity
        _unique_id index;       ///< ID of the entity to remove (not an index!)
        LayoutOrientation orientation; ///< Layout orientation (horizontal/vertical)
    };

    /**
     * @brief Event to remove an element from a layout at a specific index.
     * 
     * Removes the entity at the specified index position within the layout.
     * Negative indices count from the end (-1 = last element).
     * 
     * @see LayoutSystem::onProcessEvent(const RemoveLayoutElementAtEvent&)
     * @see BaseLayout::removeAt()
     */
    struct RemoveLayoutElementAtEvent
    {
        _unique_id id;          ///< ID of the layout entity
        int index;              ///< Index position to remove from (negative = from end)
        LayoutOrientation orientation; ///< Layout orientation (horizontal/vertical)
    };

    /**
     * @brief Event to update the scrollable state of a layout.
     * 
     * Enables or disables scrolling for the specified layout. When enabled,
     * mouse wheel events are handled and child elements are clipped to the layout bounds.
     * 
     * @see LayoutSystem::onProcessEvent(const UpdateLayoutScrollable&)
     * @see BaseLayout::setScrollable()
     */
    struct UpdateLayoutScrollable
    {
        _unique_id id;                  ///< ID of the layout entity
        bool scrollable;                ///< Whether the layout should be scrollable
        LayoutOrientation orientation;  ///< Layout orientation (horizontal/vertical)
    };

    /**
     * @brief Counts the number of visible elements in a layout.
     * 
     * Iterates through all entities in the layout and counts those that have
     * a PositionComponent and are marked as visible.
     * 
     * @tparam Layout The layout type (BaseLayout* or derived)
     * @param layout Pointer to the layout to analyze
     * @return Number of visible elements in the layout
     * 
     * @note Only counts entities that have PositionComponent attached
     * @see PositionComponent::visible
     */
    template <typename Layout>
    size_t getNbVisibleElementsInLayout(Layout layout)
    {
        size_t nb = 0;

        for (auto& ent : layout->entities)
        {
            if (ent->template has<PositionComponent>())
            {
                auto pos = ent->template get<PositionComponent>();

                if (pos->visible)
                {
                    nb++;
                }
            }
        }

        return nb;
    }

    /**
     * @brief Base class for all layout components in the UI system.
     * 
     * BaseLayout provides the fundamental functionality for arranging child entities
     * in either horizontal or vertical orientations. It supports features like:
     * 
     * - Scrolling with mouse wheel support
     * - Element spacing and wrapping
     * - Scroll bars (horizontal and vertical)
     * - Visibility culling for performance
     * - Automatic size calculation
     * 
     * Key Features:
     * - **Spacing**: Configurable spacing between elements
     * - **Scrolling**: Optional scrollable content with overflow handling
     * - **Wrapping**: Elements can wrap to new lines/columns when `fitToAxis` is enabled
     * - **Stick to End**: New elements can be added to maintain scroll position at end
     * 
     * @see HorizontalLayout
     * @see VerticalLayout
     * @see LayoutSystem
     */
    struct BaseLayout : public Ctor, public Dtor
    {
        /**
         * @brief Called when the layout component is attached to an entity.
         * 
         * Initializes the layout with the entity's ID and ECS reference.
         * This is automatically called by the ECS system.
         * 
         * @param entity The entity this layout is attached to
         */
        virtual void onCreation(EntityRef entity) override
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        /**
         * @brief Called when the layout component is about to be destroyed.
         * 
         * If clearOnDeletion is true, removes and destroys all child entities.
         * This prevents memory leaks and orphaned entities.
         * 
         * @param entity The entity being destroyed (unused)
         */
        virtual void onDeletion(EntityRef) override
        {
            if (clearOnDeletion)
                clear();
        }

        /**
         * @brief Adds an entity to the end of this layout.
         * 
         * The entity will be positioned according to the layout's orientation
         * and current settings. Sends an AddLayoutElementEvent to the LayoutSystem.
         * 
         * @param entity The entity to add to this layout
         * @see AddLayoutElementEvent
         */
        void addEntity(EntityRef entity)
        {
            ecsRef->sendEvent(AddLayoutElementEvent{id, entity.id, orientation});
        }

        /**
         * @brief Inserts an entity at a specific position in this layout.
         * 
         * @param entity The entity to insert
         * @param index Position to insert at (negative values count from end)
         * @see InsertLayoutElementEvent
         */
        void insertEntity(EntityRef entity, int index)
        {
            ecsRef->sendEvent(InsertLayoutElementEvent{id, entity.id, orientation, index});
        }

        /**
         * @brief Removes a specific entity from this layout by reference.
         * 
         * The entity is removed from the layout and destroyed.
         * 
         * @param entity The entity to remove
         * @see RemoveLayoutElementEvent
         */
        void removeEntity(EntityRef entity)
        {
            ecsRef->sendEvent(RemoveLayoutElementEvent{id, entity.id, orientation});
        }

        /**
         * @brief Removes a specific entity from this layout by ID.
         * 
         * The entity is removed from the layout and destroyed.
         * 
         * @param entityId ID of the entity to remove
         * @see RemoveLayoutElementEvent
         */
        void removeEntity(_unique_id entityId)
        {
            ecsRef->sendEvent(RemoveLayoutElementEvent{id, entityId, orientation});
        }

        /**
         * @brief Removes an entity at a specific index position.
         * 
         * @param index Position to remove from (negative values count from end)
         * @see RemoveLayoutElementAtEvent
         */
        void removeAt(int index)
        {
            ecsRef->sendEvent(RemoveLayoutElementAtEvent{id, index, orientation});
        }

        /**
         * @brief Enables or disables scrolling for this layout.
         * 
         * When enabled, the layout will handle mouse wheel events and clip
         * child elements to its bounds. Scroll bars will be shown if configured.
         * 
         * @param scrollable Whether the layout should be scrollable
         * @see UpdateLayoutScrollable
         */
        void setScrollable(bool scrollable)
        {
            if (this->scrollable != scrollable)
            {
                this->scrollable = scrollable;

                ecsRef->sendEvent(UpdateLayoutScrollable{id, scrollable, orientation});
            }
        }

        /**
         * @brief Removes and destroys all entities in this layout.
         * 
         * This is a batch operation that efficiently clears all child entities.
         * The entities list is cleared immediately, but actual entity destruction
         * is handled by the LayoutSystem.
         * 
         * @see ClearLayoutEvent
         */
        void clear()
        {
            std::vector<_unique_id> entityIds;

            entityIds.reserve(entities.size());

            for (const auto& ent : entities)
            {
                entityIds.push_back(ent.id);
            }

            ecsRef->sendEvent(ClearLayoutEvent{entityIds});

            entities.clear();
        }

        // Public configuration properties
        
        bool fitToAxis = false;    ///< If true, elements wrap to new lines/columns when exceeding layout bounds
        bool spaced = false;       ///< If true, elements are evenly spaced across the available area
        size_t spacing = 0;        ///< Spacing between elements in pixels

        // Scrollbar parameters
        EntityRef horizontalScrollBar; ///< Optional horizontal scrollbar entity
        EntityRef verticalScrollBar;   ///< Optional vertical scrollbar entity
        float xOffset = 0.0f;          ///< Current horizontal scroll offset
        float yOffset = 0.0f;          ///< Current vertical scroll offset
        float contentWidth = 0.0f;     ///< Total width of all content
        float contentHeight = 0.0f;    ///< Total height of all content
        float scrollSpeed = 25.0f;     ///< Scroll sensitivity multiplier

        bool stickToEnd = false;       ///< If true, maintains scroll position at end when adding elements
        bool clearOnDeletion = true;   ///< If true, destroys all child entities when layout is destroyed

        // Internal properties (do not modify directly)
        
        LayoutOrientation orientation = LayoutOrientation::Horizontal; ///< Layout direction
        bool scrollable = true;        ///< Whether this layout handles scroll events
        std::vector<EntityRef> entities; ///< List of child entities in this layout
        bool childrenAdded = false;    ///< Flag indicating children were added this frame
        
        _unique_id id;                 ///< ID of the entity owning this layout
        EntitySystem *ecsRef;          ///< Reference to the ECS system
    };


    /**
     * @brief Layout that arranges child elements horizontally (left to right).
     * 
     * HorizontalLayout extends BaseLayout with horizontal orientation.
     * Elements are positioned from left to right, with optional wrapping
     * to new rows when fitToAxis is enabled.
     * 
     * @see BaseLayout
     * @see VerticalLayout
     */
    struct HorizontalLayout : public BaseLayout
    {
        /**
         * @brief Constructs a horizontal layout.
         * 
         * Sets orientation to Horizontal automatically.
         */
        HorizontalLayout() : BaseLayout()
        {
            orientation = LayoutOrientation::Horizontal;
        }
    };

    /**
     * @brief Layout that arranges child elements vertically (top to bottom).
     * 
     * VerticalLayout extends BaseLayout with vertical orientation.
     * Elements are positioned from top to bottom, with optional wrapping
     * to new columns when fitToAxis is enabled.
     * 
     * @see BaseLayout
     * @see HorizontalLayout
     */
    struct VerticalLayout : public BaseLayout
    {
        /**
         * @brief Constructs a vertical layout.
         * 
         * Sets orientation to Vertical automatically.
         */
        VerticalLayout() : BaseLayout()
        {
            orientation = LayoutOrientation::Vertical;
        }
    };

    /**
     * @brief System responsible for managing UI layouts and their positioning.
     * 
     * The LayoutSystem handles all layout-related operations including:
     * 
     * ## Core Functionality
     * - **Element Positioning**: Arranges child elements according to layout orientation
     * - **Scrolling Support**: Handles mouse wheel events and scroll offset management
     * - **Visibility Culling**: Optimizes rendering by hiding off-screen elements
     * - **Dynamic Updates**: Responds to entity changes and layout modifications
     * 
     * ## Layout Types Supported
     * - HorizontalLayout: Left-to-right arrangement
     * - VerticalLayout: Top-to-bottom arrangement
     * 
     * ## Advanced Features
     * - **Wrapping**: Elements can wrap to new lines/columns with `fitToAxis`
     * - **Spacing**: Configurable spacing between elements and even distribution
     * - **Scroll Bars**: Optional horizontal and vertical scroll bar support
     * - **Clipping**: Child elements are clipped to layout bounds when scrolling
     * 
     * ## Event Handling
     * The system processes various layout events:
     * - AddLayoutElementEvent: Add elements to layouts
     * - RemoveLayoutElementEvent: Remove elements from layouts
     * - UpdateLayoutScrollable: Enable/disable scrolling
     * - EntityChangedEvent: Update layouts when child entities change
     * 
     * @see BaseLayout
     * @see HorizontalLayout
     * @see VerticalLayout
     * 
     * @warning This system modifies entity positions and visibility. Ensure proper
     *          initialization order with rendering systems.
     */
    struct LayoutSystem : public System<
        Listener<StandardEvent>,
        QueuedListener<EntityChangedEvent>,
        QueuedListener<AddLayoutElementEvent>,
        QueuedListener<InsertLayoutElementEvent>,
        QueuedListener<RemoveLayoutElementEvent>,
        QueuedListener<RemoveLayoutElementAtEvent>,
        QueuedListener<ClearLayoutEvent>,
        QueuedListener<UpdateLayoutScrollable>,
        Own<HorizontalLayout>,
        Own<VerticalLayout>,
        InitSys>
    {
        /**
         * @brief Returns the system name for debugging and logging.
         * @return System name as string
         */
        virtual std::string getSystemName() const override { return "Layout System"; }

        /**
         * @brief Initializes the layout system.
         * 
         * Sets up entity groups for horizontal and vertical layouts,
         * registers event listeners, and configures scroll handling.
         * Called automatically by the ECS framework.
         */
        virtual void init() override;

        /**
         * @brief Handles standard events, primarily mouse wheel scrolling.
         * 
         * Processes "layoutScroll" events to update scroll offsets for
         * scrollable layouts. Extracts entity ID and scroll delta from event.
         * 
         * @param event The standard event to process
         * @see StandardEvent
         */
        virtual void onEvent(const StandardEvent& event) override;

        /**
         * @brief Processes requests to add elements to layouts.
         * 
         * Adds the specified entity to the end of the target layout.
         * Validates that both entities exist and have required components.
         * 
         * @param event Event containing layout ID, entity ID, and orientation
         * @see AddLayoutElementEvent
         */
        virtual void onProcessEvent(const AddLayoutElementEvent& event) override;

        /**
         * @brief Processes requests to insert elements at specific positions.
         * 
         * Inserts the specified entity at the given index within the layout.
         * Supports negative indices for insertion from the end.
         * 
         * @param event Event containing layout ID, entity ID, orientation, and index
         * @see InsertLayoutElementEvent
         */
        virtual void onProcessEvent(const InsertLayoutElementEvent& event) override;

        /**
         * @brief Processes requests to remove specific entities from layouts.
         * 
         * Removes the entity with the specified ID from the layout and destroys it.
         * Searches through the layout's entities to find the target.
         * 
         * @param event Event containing layout ID and entity ID to remove
         * @see RemoveLayoutElementEvent
         */
        virtual void onProcessEvent(const RemoveLayoutElementEvent& event) override;

        /**
         * @brief Processes requests to remove elements at specific indices.
         * 
         * Removes the entity at the specified index position and destroys it.
         * Supports negative indices for removal from the end.
         * 
         * @param event Event containing layout ID and index position
         * @see RemoveLayoutElementAtEvent
         */
        virtual void onProcessEvent(const RemoveLayoutElementAtEvent& event) override;

        /**
         * @brief Processes requests to clear multiple entities from layouts.
         * 
         * Batch removes and destroys all specified entities. This is more
         * efficient than removing entities individually.
         * 
         * @param event Event containing list of entity IDs to remove
         * @see ClearLayoutEvent
         */
        virtual void onProcessEvent(const ClearLayoutEvent& event) override;

        /**
         * @brief Processes entity change notifications.
         * 
         * When an entity changes (position, size, etc.), this updates any layouts
         * containing that entity. Uses optimization to avoid searching all layouts
         * for entities not known to be in layouts.
         * 
         * @param event Event containing the ID of the changed entity
         * @see EntityChangedEvent
         */
        virtual void onProcessEvent(const EntityChangedEvent& event) override;

        /**
         * @brief Processes requests to update layout scrollable state.
         * 
         * Enables or disables scrolling for the specified layout. When enabled,
         * attaches mouse wheel components and sets up clipping. When disabled,
         * removes scroll components and hides scroll bars.
         * 
         * @param event Event containing layout ID, scrollable state, and orientation
         * @see UpdateLayoutScrollable
         */
        virtual void onProcessEvent(const UpdateLayoutScrollable& event) override;

        /**
         * @brief Main execution loop for layout updates.
         * 
         * Processes all layouts marked for update, recalculating positions
         * and visibility for their child elements. Called every frame.
         */
        virtual void execute() override;

        /**
         * @brief Helper function for updating layout scrollable state.
         * 
         * Handles the complex logic of enabling/disabling scroll functionality:
         * 
         * **When enabling scrolling:**
         * - Attaches MouseWheelComponent for scroll event handling
         * - Clips all child entities to the layout bounds
         * 
         * **When disabling scrolling:**
         * - Removes MouseWheelComponent
         * - Removes clipping from child entities (if layout itself isn't clipped)
         * - Hides horizontal and vertical scroll bars
         * 
         * @param entity The layout entity to modify
         * @param view The BaseLayout component of the entity
         * @param scrollable Whether to enable or disable scrolling
         * 
         * @see MouseWheelComponent
         * @see ClippedTo
         */
        void processScrollHelper(Entity* entity, BaseLayout* view, bool scrollable);

        /**
         * @brief Recalculates positions of all child elements in a layout.
         * 
         * This is the main layout calculation function that:
         * - Adjusts scroll offsets to valid ranges
         * - Updates scroll bar positions and visibility
         * - Delegates to appropriate spacing/positioning algorithms
         * 
         * @param viewEnt The layout entity
         * @param view The BaseLayout component
         * 
         * @todo Calculate the lesser axis of the view (biggest width for Vertical layouts)
         */
        void recalculateChildrenPos(EntityRef viewEnt, BaseLayout* view);

        /**
         * @brief Internal method to add an entity to a layout.
         * 
         * Performs the actual work of adding entities to layouts, including:
         * - Validation of entity components
         * - Setting up parent-child relationships
         * - Configuring clipping if needed
         * - Handling "stick to end" scroll behavior
         * 
         * @param viewEnt The layout entity
         * @param ui ID of the entity to add
         * @param orientation Layout orientation
         * @param index Position to insert at (-1 for end)
         */
        void addEntity(EntityRef viewEnt, _unique_id ui, LayoutOrientation orientation, int index = -1);

        /**
         * @brief Removes an entity from a layout by entity ID.
         * 
         * Searches for the entity in the layout's entities list and removes it.
         * Also handles parent-child relationship cleanup.
         * 
         * @param view The layout to remove from
         * @param index The entity ID to remove (poorly named parameter)
         */
        void removeEntity(BaseLayout* view, _unique_id index);

        /**
         * @brief Removes an entity from a layout by index position.
         * 
         * Removes the entity at the specified index, with support for
         * negative indices counting from the end.
         * 
         * @param view The layout to remove from
         * @param index The position to remove from
         */
        void removeEntityAt(BaseLayout* view, int index);

        /**
         * @brief Updates visibility state of child elements for culling optimization.
         * 
         * Performs frustum culling by checking which child elements intersect
         * with the layout's visible bounds. Hidden elements are marked as
         * non-observable to optimize rendering.
         * 
         * @param viewEnt The layout entity
         * @param view The BaseLayout component
         * 
         * @see PositionComponent::setObservable()
         */
        void updateVisibility(EntityRef viewEnt, BaseLayout* view);

        /**
         * @brief Internal method to clear multiple entities.
         * 
         * Batch removes entities from tracking sets and destroys them.
         * More efficient than individual removal operations.
         * 
         * @param entityIds List of entity IDs to remove and destroy
         */
        void clear(const std::vector<_unique_id>& entityIds);

        /**
         * @brief Updates a layout by recalculating positions and visibility.
         * 
         * Simple wrapper that calls both recalculateChildrenPos and updateVisibility.
         * 
         * @param viewEnt The layout entity
         * @param view The BaseLayout component
         */
        void updateLayout(EntityRef viewEnt, BaseLayout* view);

        /**
         * @brief Adjusts scroll offsets to ensure they remain within valid bounds.
         * 
         * Clamps scroll offsets to prevent scrolling beyond content boundaries.
         * Special handling when children are added to avoid premature clamping.
         * 
         * @param viewEnt The layout entity
         * @param view The BaseLayout component
         * @param childrenAdded Whether children were added this frame
         */
        void adjustOffsets(EntityRef viewEnt, BaseLayout* view, bool childrenAdded);

        /**
         * @brief Updates scroll bar positions and visibility.
         * 
         * Updates both horizontal and vertical scroll bars if they exist,
         * calculating thumb position and size based on content and view dimensions.
         * 
         * @param viewEnt The layout entity
         * @param view The BaseLayout component
         */
        void updateScrollBars(EntityRef viewEnt, BaseLayout* view);

        /**
         * @brief Updates horizontal scroll bar appearance and position.
         * 
         * Calculates thumb width and position based on the ratio of view width
         * to content width. Hides scroll bar if scrolling is not needed.
         * 
         * @param viewUi Position component of the layout
         * @param view The BaseLayout component
         * @param sbPos Position component of the scroll bar
         */
        void updateHorizontalScrollBar(PositionComponent* viewUi, BaseLayout* view, PositionComponent* sbPos);

        /**
         * @brief Updates vertical scroll bar appearance and position.
         * 
         * Calculates thumb height and position based on the ratio of view height
         * to content height. Hides scroll bar if scrolling is not needed.
         * 
         * @param viewUi Position component of the layout
         * @param view The BaseLayout component
         * @param sbPos Position component of the scroll bar
         */
        void updateVerticalScrollBar(PositionComponent* viewUi, BaseLayout* view, PositionComponent* sbPos);

        /**
         * @brief Positions elements without advanced spacing or wrapping.
         * 
         * Simple linear layout algorithm that:
         * - Places elements sequentially along the primary axis
         * - Aligns elements to the layout's secondary axis anchor
         * - Adds configured spacing between elements
         * - Automatically sizes the layout to fit content (if not constrained)
         * 
         * Used when both `fitToAxis` and `spaced` are false.
         * 
         * @param viewEnt The layout entity
         * @param view The BaseLayout component
         */
        void layoutWithoutSpacing(EntityRef viewEnt, BaseLayout* view);

        /**
         * @brief Positions elements with advanced spacing and wrapping support.
         * 
         * Complex layout algorithm that supports:
         * - Element wrapping when `fitToAxis` is enabled
         * - Even spacing distribution when `spaced` is enabled
         * - Handling of oversized elements that exceed layout bounds
         * - Multi-line/multi-column layout calculations
         * 
         * This is the most sophisticated layout method and handles edge cases
         * like elements larger than the available space.
         * 
         * @param viewEnt The layout entity
         * @param view The BaseLayout component
         * 
         * @warning This function is complex and handles many edge cases.
         *          Modifications should be tested thoroughly.
         */
        void layoutWithSpacing(EntityRef viewEnt, BaseLayout* view);

        std::set<EntityRef> layoutUpdate;    ///< Layouts that need position recalculation this frame
        std::set<_unique_id> entitiesInLayout; ///< Optimization: tracks which entities are in layouts
    };

    /**
     * @brief Factory function to create a horizontal layout entity.
     * 
     * Creates a complete horizontal layout with all required components:
     * - HorizontalLayout component for layout logic
     * - PositionComponent for size and position
     * - UiAnchor for anchoring and constraints
     * 
     * @tparam Type ECS system type
     * @param ecs Pointer to the ECS system
     * @param x Initial X position
     * @param y Initial Y position
     * @param width Initial width
     * @param height Initial height
     * @param scrollable Whether the layout should be scrollable
     * @return ComponentList containing entity and attached components
     * 
     * @see HorizontalLayout
     * @see makeVerticalLayout()
     */
    template <typename Type>
    CompList<PositionComponent, UiAnchor, HorizontalLayout> makeHorizontalLayout(Type *ecs, float x, float y, float width, float height, bool scrollable = false)
    {
        auto entity = ecs->createEntity();

        auto view = ecs->template attach<HorizontalLayout>(entity);

        view->scrollable = scrollable;

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setWidth(width);
        ui->setHeight(height);

        return {entity, ui, anchor, view};
    }

    /**
     * @brief Factory function to create a vertical layout entity.
     * 
     * Creates a complete vertical layout with all required components:
     * - VerticalLayout component for layout logic
     * - PositionComponent for size and position
     * - UiAnchor for anchoring and constraints
     * 
     * @tparam Type ECS system type
     * @param ecs Pointer to the ECS system
     * @param x Initial X position
     * @param y Initial Y position
     * @param width Initial width
     * @param height Initial height
     * @param scrollable Whether the layout should be scrollable
     * @return ComponentList containing entity and attached components
     * 
     * @see VerticalLayout
     * @see makeHorizontalLayout()
     */
    template <typename Type>
    CompList<PositionComponent, UiAnchor, VerticalLayout> makeVerticalLayout(Type *ecs, float x, float y, float width, float height, bool scrollable = false)
    {
        auto entity = ecs->createEntity();

        auto view = ecs->template attach<VerticalLayout>(entity);

        view->scrollable = scrollable;

        auto ui = ecs->template attach<PositionComponent>(entity);

        auto anchor = ecs->template attach<UiAnchor>(entity);

        ui->setX(x);
        ui->setY(y);
        ui->setWidth(width);
        ui->setHeight(height);

        return {entity, ui, anchor, view};
    }

}