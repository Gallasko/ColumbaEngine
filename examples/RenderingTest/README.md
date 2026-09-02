# RenderingTest

**Build target:** `RenderingTest`

A UI-layout showcase built on `pg::Engine`. It registers the `TTFTextSystem` with the bundled Inter fonts, then builds a dark-themed "Properties" panel out of anchored prefabs.

What it demonstrates:
- `TTFTextSystem` setup and `registerFont` (fonts are referenced by alias, not path)
- `makeTTFText` labels and `makeTTFTextInput` editable fields
- `makeAnchoredPrefab`, `makeRoundedRect2DShape`, `makeVerticalLayout`
- UI anchors and focus handling (`MouseLeftClickComponent` + `OnFocus`)

Run from the build directory: `./RenderingTest`

Read this one when you want to build menus, HUDs, or tool UIs.
