UiAnchor Component
==================

The **UiAnchor** component provides a flexible anchoring mechanism for UI elements in the ECS (Entity-Component-System) framework. It enables dynamic positioning by anchoring one element relative to another, thereby ensuring responsive layouts. The **UiAnchor** works closely with the **PositionComponent** to update element positions based on defined anchors, margins, and constraints.

Main implementation files:
`position.h <https://github.com/Gallasko/PgEngine/tree/main/src/Engine/2D/position.h>`_,
`position.cpp <https://github.com/Gallasko/PgEngine/tree/main/src/Engine/2D/position.cpp>`_

Attributes
----------

- *PosAnchor* ``top``:
    The current top anchor value for the component.
- *PosAnchor* ``left``:
    The current left anchor value.
- *PosAnchor* ``right``:
    The current right anchor value.
- *PosAnchor* ``bottom``:
    The current bottom anchor value.
- *bool* ``hasTopAnchor``:
    Indicates if a top anchor is set.
- *bool* ``hasLeftAnchor``:
    Indicates if a left anchor is set.
- *bool* ``hasRightAnchor``:
    Indicates if a right anchor is set.
- *bool* ``hasBottomAnchor``:
    Indicates if a bottom anchor is set.
- *PosAnchor* ``topAnchor``:
    The target top anchor used for anchoring calculations.
- *PosAnchor* ``leftAnchor``:
    The target left anchor used for anchoring calculations.
- *PosAnchor* ``rightAnchor``:
    The target right anchor used for anchoring calculations.
- *PosAnchor* ``bottomAnchor``:
    The target bottom anchor used for anchoring calculations.
- *PosAnchor* ``verticalCenter``:
    Holds the current vertical center value.
- *PosAnchor* ``horizontalCenter``:
    Holds the current horizontal center value.
- *bool* ``hasVerticalCenter``:
    Indicates if a vertical center anchor is set.
- *bool* ``hasHorizontalCenter``:
    Indicates if a horizontal center anchor is set.
- *PosAnchor* ``verticalCenterAnchor``:
    The target vertical center anchor for calculations.
- *PosAnchor* ``horizontalCenterAnchor``:
    The target horizontal center anchor for calculations.
- *float* ``topMargin``:
    Margin applied to the top anchor.
- *float* ``leftMargin``:
    Margin applied to the left anchor.
- *float* ``rightMargin``:
    Margin applied to the right anchor.
- *float* ``bottomMargin``:
    Margin applied to the bottom anchor.
- *bool* ``hasWidthConstrain``:
    Indicates if a width constraint is applied.
- *bool* ``hasHeightConstrain``:
    Indicates if a height constraint is applied.
- *bool* ``hasZConstrain``:
    Indicates if a Z (depth) constraint is applied.
- *PosConstrain* ``widthConstrain``:
    Constraint used to adjust the width.
- *PosConstrain* ``heightConstrain``:
    Constraint used to adjust the height.
- *PosConstrain* ``zConstrain``:
    Constraint used to adjust the Z coordinate.
- *EntitySystem* ``ecsRef``:
    Reference to the ECS world.
- *\_unique_id* ``id``:
    Unique identifier for the component instance.

Methods
-------

.. warning::
        All methods trigger a ``PositionComponentChangedEvent`` (and sometimes a ``ParentingEvent``) when the underlying value is modified.

- ``setTopAnchor(const PosAnchor &anchor)``
    Sets the top anchor and marks it as active. Sends a parenting event linking the target entity to this component, and a position change event.

- ``clearTopAnchor()``
    Clears the top anchor (if active), sending a clear parenting event and triggering a position change event.

- ``setLeftAnchor(const PosAnchor &anchor)``
    Sets the left anchor, marks it as active, and dispatches the necessary events.

- ``clearLeftAnchor()``
    Clears the left anchor and sends the corresponding events.

- ``setRightAnchor(const PosAnchor &anchor)``
    Sets the right anchor, marks it as active, and sends events to update the system.

- ``clearRightAnchor()``
    Clears the right anchor and triggers the appropriate events.

- ``setBottomAnchor(const PosAnchor &anchor)``
    Sets the bottom anchor, marks it as active, and sends events.

- ``clearBottomAnchor()``
    Clears the bottom anchor and sends a clear parenting event along with a position change event.

- ``setVerticalCenter(const PosAnchor &anchor)``
    Sets the vertical center anchor and marks it as active, sending necessary events.

- ``clearVerticalCenter()``
    Clears the vertical center anchor and triggers a change event.

- ``setHorizontalCenter(const PosAnchor &anchor)``
    Sets the horizontal center anchor, marks it as active, and dispatches the corresponding events.

- ``clearHorizontalCenter()``
    Clears the horizontal center anchor and sends events.

- ``fillIn(const UiAnchor &anchor)``
    Sets all basic cardinal anchors (top, left, right, bottom) based on another **UiAnchor**.

- ``fillIn(const UiAnchor *anchor)``
    Same as above but accepts a pointer to a **UiAnchor**.

- ``centeredIn(const UiAnchor &anchor)``
    Sets the vertical and horizontal center anchors based on another **UiAnchor**.

- ``centeredIn(const UiAnchor *anchor)``
    Same as above for pointer input.

- ``clearAnchors()``
    Clears all anchors, including basic (top, left, right, bottom) and advanced (vertical and horizontal center).

- ``setTopMargin(float value)``
    Sets the top margin and triggers a position change event.

- ``setLeftMargin(float value)``
    Sets the left margin and triggers a position change event.

- ``setRightMargin(float value)``
    Sets the right margin and triggers a position change event.

- ``setBottomMargin(float value)``
    Sets the bottom margin and triggers a position change event.

- ``setWidthConstrain(const PosConstrain &constrain)``
    Sets a width constraint, marks it as active, and sends parenting and change events.

- ``setHeightConstrain(const PosConstrain &constrain)``
    Sets a height constraint, marks it as active, and dispatches events.

- ``setZConstrain(const PosConstrain &constrain)``
    Sets a Z coordinate constraint, marks it as active, and sends the relevant events.

- ``updateAnchor(bool hasAnchor, PosAnchor &anchor)``
    Updates the specified anchor's value if it is active, by querying the associated **PositionComponent**.

- ``update(CompRef<PositionComponent> pos)``
    Synchronizes anchor values with the current state of the **PositionComponent**. Returns `true` if any anchor value has changed.

- ``onCreation(EntityRef entity)``
    Initializes the **UiAnchor** component upon creation. Sets up default anchors and stores a reference to the ECS.

Working with the UiAnchor Component
-------------------------------------

The **UiAnchor** component is used to define how a UI element is anchored relative to other elements. This is crucial for building dynamic and responsive layouts.

Basic Usage Example
^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    // Create a UI texture (e.g., an icon) and get its components.
    auto icon = makeUiTexture(this, 120, 120, "NoneIcon");
    auto iconUi = icon.get<PositionComponent>();
    auto iconAnchor = icon.get<UiAnchor>();

    // Set the icon's position.
    iconUi->setX(45);
    iconUi->setY(35);

    // Create a text input for the character name.
    auto name = makeTTFTextInput(this, 0, 0, StandardEvent("CharaNameChange"),
                                                                "res/font/Inter/static/Inter_28pt-Light.ttf", "Character 1", 0.7);
    auto nameUi = name.get<PositionComponent>();
    auto nameAnchor = name.get<UiAnchor>();

    // Anchor the name input field relative to the icon.
    nameAnchor->setTopAnchor(iconAnchor->bottom);
    nameAnchor->setTopMargin(5);
    nameAnchor->setLeftAnchor(iconAnchor->left);
    nameAnchor->setLeftMargin(15);

In this example:
    - A UI texture (icon) and a text input (name) are created.
    - The **UiAnchor** attached to the name input is used to anchor it below the icon, with specified margins.
    - As a result, if the icon's position changes, the name input will automatically adjust its position accordingly.

Conclusion
----------

The **UiAnchor** component is a key element in creating dynamic UI layouts in the ECS framework. It works in tandem with the **PositionComponent** to provide relative positioning and responsive UI designs. For more details, please refer to the source files:
`position.h <https://github.com/Gallasko/PgEngine/tree/main/src/Engine/2D/position.h>`_ and
`position.cpp <https://github.com/Gallasko/PgEngine/tree/main/src/Engine/2D/position.cpp>`_.
