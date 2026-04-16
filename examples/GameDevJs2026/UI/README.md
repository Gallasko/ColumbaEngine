# UI

UI ECS systems (all render on viewport 2, which is an orthographic screen-space camera set up by `ToolbarSystem::createUICamera`).

- `toolbarsystem.h` / `.cpp` — bottom-of-screen building toolbar with 1–9 keyboard hotkeys
- `inventoryui.h` / `.cpp` — modal inventory panel with drag-and-drop, pluggable external slot support
- `minerui.h` / `.cpp` — miner inspection panel (side-docked with inventory), pickup/drop into miner output
- `craftingui.h` / `.cpp` — hand-craft recipe browser (opens alongside inventory, progress bar + Craft/Cancel)
