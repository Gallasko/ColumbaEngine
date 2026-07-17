# Systems

ECS gameplay systems that drive the simulation: grid state, input, mining, transport, crafting, and the player's own inventory.

- `gridsystem.h` / `.cpp` — authoritative grid state, building placement, conveyor animation, terrain render
- `gamesystem.h` / `.cpp` — mouse/keyboard input, ghost cursor, line-drag belt placement
- `minersystem.h` / `.cpp` — miners: timed ore production into 1-slot output inventory
- `transportsystem.h` / `.cpp` — conveyor item transport with cycle detection + round-robin arbitration
- `insertersystem.h` / `.cpp` — robotic-arm state machine (Idle/Swinging/Returning) between belts and machines
- `craftingsystem.h` / `.cpp` — furnaces and assemblers pulling from belts and running recipes
- `handcraftingsystem.h` / `.cpp` — single active player hand-craft driver (consumes inputs, emits outputs, bumps facts)
- `playerinventory.h` / `.cpp` — player inventory ECS system, seeds discovery facts on first pickup
