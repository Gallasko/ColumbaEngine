Position Component
==================

The **PositionComponent** is responsible for managing the position and visibility of UI elements in the ECS (Entity-Component-System) framework. It controls an entity's `x` and `y` coordinates on the screen, as well as its visibility. The `PositionComponent` is part of the UI system and works in tandem with other components such as `UiAnchor`, `MouseLeftClickComponent`, and `Timer`.

This component is essential for creating and positioning UI elements dynamically within a game or application.

Main implementation file: `position.h <https://github.com/Gallasko/ColumbaEngine/tree/main/src/Engine/2D/position.h>`_, `position.cpp <https://github.com/Gallasko/ColumbaEngine/tree/main/src/Engine/2D/position.cpp>`_

Attributes
----------

- *float* ``x``:
    The X coordinate of the UI element's position on the screen.

- *float* ``y``:
    The Y coordinate of the UI element's position on the screen.

- *float* ``z``:
    The Z coordinate of the UI element's position on the screen

- *float* ``width``:
    The width of the UI element's position on the screen

- *float* ``height``:
    The height of the UI element's position on the screen

- *bool*  ``visibility``:
    A boolean value indicating whether the UI element is visible (``true``) or hidden (``false``).

Methods
-------

.. warning::
    All those methods will send a ``PositionComponentChangedEvent`` with the id of the entity holding the position component, **if and only if** the underlying value is modified !

- ``setX(float x)``
    Sets the X position of the UI element.

- ``setY(float y)``
    Sets the Y position of the UI element.

- ``setZ(float z)``
    Sets the Z position of the UI element.

- ``setWidth(float width)``
    Sets the width of the UI element.

- ``setHeight(float height)``
    Sets the height of the UI element.

- ``setVisibility(bool visibility)``
    Sets the visibility of the UI element. If ``true``, the element is visible. If ``false``, the element is hidden.

Working with the PositionComponent
----------------------------------

The **PositionComponent** allows you to control where UI elements are placed within the window or screen. It is often used in combination with the ``UiAnchor`` component to adjust the positioning relative to other elements.

Basic Usage of PositionComponent
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    // Create a new UI texture (e.g., an icon)
    auto icon = makeUiTexture(this, 120, 120, "NoneIcon");

    // Get the PositionComponent attached to the icon
    auto iconUi = icon.get<PositionComponent>();

    // Set the position of the icon at x = 45 and y = 35
    iconUi->setX(45);
    iconUi->setY(35);

    // Initially hide the icon
    iconUi->setVisibility(false);


In this example:
    - A UI texture is created (an icon with the image ``"NoneIcon"``).
    - The ``PositionComponent`` is retrieved from the created UI texture.
    - The position of the icon is set to ``(45, 35)``, meaning it will be positioned 45 pixels from the left and 35 pixels from the top of the window or parent container.
    - The icon's visibility is set to ``false``, meaning the icon will be hidden when it is first created.

Using PositionComponent with UiAnchor
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``PositionComponent`` can also be used alongside the ``UiAnchor`` component to position an element relative to another UI element.

.. code-block:: cpp

    // Create a new UI texture (e.g., an icon)
    auto icon = makeUiTexture(this, 120, 120, "NoneIcon");
    auto iconUi = icon.get<PositionComponent>();
    auto iconAnchor = icon.get<UiAnchor>();

    // Position the icon at x = 45, y = 35
    iconUi->setX(45);
    iconUi->setY(35);

    // Create a new name input field
    auto name = makeTTFTextInput(this, 0, 0, StandardEvent("CharaNameChange"), "res/font/Inter/static/Inter_28pt-Light.ttf", "Character 1", 0.7);
    auto nameUi = name.get<PositionComponent>();
    auto nameAnchor = name.get<UiAnchor>();

    // Anchor the name input field below the icon
    nameAnchor->setTopAnchor(iconAnchor->bottom);
    nameAnchor->setTopMargin(5);
    nameAnchor->setLeftAnchor(iconAnchor->left);
    nameAnchor->setLeftMargin(15);


In this example:
    - The position of the icon is defined explicitly using ``PositionComponent``.
    - A ``name`` input field is created, and its position is defined relative to the ``icon`` using ``UiAnchor``.
    - The ``name`` input field will appear below the icon, with a margin of 5 pixels from the icon's bottom and 15 pixels from the left side of the icon.

.. note::
    In this example, the position of the name entity is relative to the position of the icon entity, and so moving the icon will **automatically** move the name !

Visibility and Layout Management
--------------------------------

The ``PositionComponent`` also provides a mechanism for controlling the visibility of UI elements. This can be particularly useful when dynamically showing or hiding components, such as in a menu or pop-up system.

To make a UI element visible:

.. code-block:: cpp

    iconUi->setVisibility(true);  // Make the icon visible

To hide the UI element:

.. code-block:: cpp

    iconUi->setVisibility(false);  // Hide the icon


Conclusion
----------

The **PositionComponent** plays a crucial role in positioning and managing the visibility of UI elements. When combined with other components like ``UiAnchor``, it allows for flexible UI layouts that can be dynamically adjusted. This makes it an essential part of any UI system that requires interactive or responsive elements.

For more complex UI layouts and interactions, the **PositionComponent** can be used alongside event systems (like mouse clicks or timers) to trigger visibility changes and animations.
