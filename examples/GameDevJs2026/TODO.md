# GameDevJs2026 - Game Loop Roadmap (9 Days)

## Phase 1: Complete the Core Loop (Days 1-2)

- [ ] **Building hand-craft recipes** — Add recipes for Conveyor Belt, Furnace, Miner, Inserter, Assembler to hand-crafting menu
- [ ] **Furnace UI** — Click on furnace to see input slot, progress bar, output slot. Insert/extract items manually
- [ ] **Assembler UI** — Click on assembler to see 2 input slots, progress bar, output slot. Auto-detects recipe
- [ ] **Starting items** — Give new players a starter kit (pickaxe, belts, furnace, miner, inserters)

## Phase 2: Depot / Storage (Days 3-4)

- [ ] **Depot building** — 1x1 storage chest with 8 slots. Inserters can push/pull from it. DepotUI with 2x4 grid

## Phase 3: Idle Mission System (Days 5-7)

- [ ] **Mission system** — Send machines on quests via depots. Consume items from depot, get rewards back after timer. 3-5 escalating missions. MissionUI accessible from DepotUI

## Phase 4: Currency + Canvas Expansion (Day 7-8)

- [ ] **Currency (Tickets)** — New item earned from missions, displayed in HUD
- [ ] **Canvas expansion** — Spend tickets to expand the grid (32x32 -> 64x64). Regenerate terrain for new area

## Phase 5: Tutorial (Day 8-9)

- [ ] **Basic tutorial** — Fact-gated text prompts: Mine a tree -> Open inventory -> Craft pickaxe -> Place a miner -> Connect with belts

---

## Cut List (if time runs short)

1. Canvas expansion — 32x32 is enough
2. Mission system — core mine/smelt/assemble loop IS the game
3. Tutorial — replace with static controls overlay
4. Assembler UI — leave as automation-only
5. **Never cut:** Building recipes + Furnace UI + Starting items

replacing belt actually destroy them
Launching a hand craft and have it complete in the background make all the inventory items visible for nothing
Remove entitysystem.h include from generated component headers (make the method be able to put in cpp)
Move the generated component code to the build folder
Fix all ui creation with correct anchor code (ui1->setLeftAnchor(ui2->left))