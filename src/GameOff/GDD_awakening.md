
# Game Design Document – Early Game MVP

## 1. Overview

**Title:** Forgotten Archivist
**Theme:**
You awaken as the last surviving Archivist in a ruined, ancient library. Your power stems from your ability to harness mana, convert it into magical scrap, and craft rudimentary equipment. Through a series of interactions and narrative revelations, you gradually rediscover your forgotten legacy and unlock deeper magical abilities, eventually leading to a combat phase that will pave the way to unlocking the scriptorium—a hidden chamber of advanced knowledge.

**Scope of Early Game MVP:**
- Duration: Approximately 1–2 hours of active play.
- Stages:
  1. **The Awakening:** Introduction, basic mana harvesting, and narrative setup.
  2. **The Salvage Conversion & Equipment Crafting:** Converting collected mana into scrap to craft a rudimentary wand.
  3. **Transition to Combat:** Introduction of basic combat encounters with minor monsters, setting the stage for the scriptorium unlock.

---

## 2. Narrative & Setting

### 2.1. The Awakening

**Premise:**
You awaken with no memories in the crumbling ruins of the Celestial Library—a once-grand repository of arcane knowledge now overrun by chaos. The only remnant of the library’s power is the Runic Altar, pulsing with latent mana.

**Key Narrative Elements:**
- **Amnesia and Rediscovery:**
  The player’s journey begins with an overwhelming sense of loss and mystery, as the protagonist struggles to remember their past.
- **The Runic Altar:**
  Serves as the focal point of early interactions. Its activation triggers the first lore fragments and sets the stage for further progression.
- **Initial Lore Fragments:**
  Short narrative entries reveal hints of your forgotten identity and the history of the Archive.

### 2.2. Transition to the Salvage Conversion

**Premise:**
After awakening, you begin harvesting mana from the altar. However, raw mana alone is not enough. You discover that by processing mana through ancient machines—called **Scrap Processors**—you can convert it into a new resource: **Scrap**.

**Key Narrative Elements:**
- **Discovery of Scrap Processors:**
  Hidden within the ruins, these devices were once used by your predecessors to salvage and repurpose magical energy.
- **Resource Conversion:**
  The conversion mechanic symbolizes the Archivist’s initial attempts to reclaim lost knowledge, as raw mana is transformed into Scrap, the foundation for crafting equipment.
- **Lore Entries:**
  Additional lore explains how the Archivists used scrap to forge basic equipment and record ancient spells.

---

## 3. Gameplay Mechanics

### 3.1. Resource Generation

#### 3.1.1. Mana Harvesting (The Runic Altar)
- **Component:** `ManaGenerator`
  - **Attributes:**
    - `currentMana`: The stored mana.
    - `productionRate`: The rate at which mana is generated.
    - `capacity`: The maximum mana the altar can hold.
- **Mechanic:**
  - Initially, the only interactive button is **"Touch Altar"**.
  - Upon clicking, the world fact `altar_touched` is set to true and a lore event is triggered.
  - Once touched, the **"Harvest"** button appears, allowing the player to manually collect mana.
  - Each harvest updates world facts (`mana` and `total_mana_collected`).

#### 3.1.2. Scrap Conversion (The Scrap Processor)
- **Concept:**
  - Instead of a passive scrap generator, the player uses a **Scrap Processor**.
  - The mechanic requires the player to input a set amount of mana to convert it into Scrap (e.g., 3 mana → 1 scrap).
- **Mechanic:**
  - A dynamic button **"Process Scrap"** appears when sufficient mana is available.
  - Clicking this button deducts the mana and adds the corresponding amount of Scrap to a new world fact or resource pool.

### 3.2. Equipment Crafting

#### 3.2.1. Basic Grimoire and Wand Crafting
- **Narrative:**
  - After processing scrap, the Archivist finds a basic grimoire—an ancient tome that contains rudimentary spell instructions.
- **Mechanic:**
  - A dynamic button **"Read Grimoire / Craft Wand"** becomes available when enough Scrap has been accumulated.
  - Clicking this button consumes a predetermined amount of Scrap (and possibly a bit of mana) and crafts the player’s first wand.
  - Crafting the wand triggers a lore event, revealing more about your lineage and the Archive’s history.
- **Outcome:**
  - The crafted wand becomes the player's first piece of combat equipment, providing basic combat abilities.

### 3.3. Transition to Combat
- **Pre-Combat Narrative:**
  - With a rudimentary wand in hand and scraps processed, you are now better prepared to face the minor guardians of the Archive.
- **Enemy Types (to be introduced later):**
  - **Ink Slimes:** Amorphous creatures formed of dark ink.
  - **Paper Golems:** Clumsy constructs made of crumpled parchment.
  - **Dust Wraiths:** Ethereal figures of swirling dust.
- **Mechanic:**
  - Combat is turn-based. Early encounters are designed to teach the player the basics of combat.
  - Combat rewards might include additional scraps or rare components that further enhance your equipment and magical abilities.
- **Narrative Tie-In:**
  - Combat serves as a rite of passage, with each victory triggering more lore and unlocking further progression, ultimately leading to the discovery of the Scriptorium.

---

## 4. UI and Dynamic Button System

### 4.1. Dynamic Button Structure

- **DynamicNexusButton:**
  - **Attributes:**
    - `id`: Unique identifier.
    - `label`: Button text.
    - `conditions`: A list of FactChecker objects that determine if the button should trigger its outcome.
    - `neededConditionsForVisibility`: A subset of conditions that must be met for the button to appear.
    - `outcome`: A list of AchievementReward objects to execute on click.
    - `category`: Used for sorting/layout (e.g., "main", "mana upgrade").
    - `nbClickBeforeArchive`: Number of clicks allowed before the button is archived.
    - `archived`: Whether the button is archived.
    - `entityId`: The UI entity identifier for the button.
- **Vectors Managed:**
  - **maskedButtons:** Buttons not yet revealed.
  - **visibleButtons:** Buttons currently visible and interactive.
  - **archivedButtons:** Buttons that have been used and are no longer active.

### 4.2. UI Integration

- **Layout:**
  - Use a HorizontalLayout for the Nexus scene’s main buttons.
- **Dynamic Update:**
  - The `updateDynamicButtons` function iterates over the three vectors and creates, removes, or archives buttons based on world fact conditions.
- **Thematic UI:**
  - Keep UI elements primarily text-based with simple shapes for buttons.
  - Use TTFText for lore popups, button labels, and game logs.

---

## 5. Progression Flow

### 5.1. Stage 1: The Awakening
- **Initial Actions:**
  - Player begins by touching the altar, triggering `altar_touched` and a lore event.
  - The Harvest button appears, allowing mana collection.
- **Resource Accumulation:**
  - Harvesting increases `mana` and `total_mana_collected` world facts.
- **Dynamic Button Updates:**
  - As world facts change, dynamic buttons are revealed or hidden based on conditions.

### 5.2. Stage 2: Salvage and Equipment Crafting
- **Mana-to-Scrap Conversion:**
  - When sufficient mana is collected, the **Process Scrap** button appears.
  - Using the Scrap Processor converts mana into Scrap.
- **Crafting the Wand:**
  - When enough Scrap is available, the **Read Grimoire / Craft Wand** button becomes active.
  - Crafting the wand triggers further lore, unlocking basic combat equipment.
- **Transition to Combat:**
  - With a wand crafted, the player is set to face basic enemies in a turn-based combat scenario.

---

## 6. Next Steps & Future Expansion

- **Combat Integration:**
  - Design and implement basic turn-based combat with introductory enemies (Ink Slimes, Paper Golems, etc.).
  - Use combat rewards to further enrich the resource pool (e.g., additional Scrap, rare components).
- **Lore Deepening:**
  - Expand narrative entries that describe the history of the Archive, the role of the Archivists, and the evolution of grimoires.
  - Tie each progression milestone to a lore event.
- **Advanced Specializations & Prestige:**
  - After the initial run (roughly 1–2 hours), introduce more advanced specializations (e.g., Runic Conjurer, Reality Weaver) and a prestige system with randomized grimoires.
- **Asset Reuse & Minimal Visuals:**
  - Focus on maximizing text-based storytelling and reuse UI components to keep scope small.
  - Reserve detailed 2D assets only for key game elements (Runic Altar, Grimoire, Equipment).

---

## 7. Conclusion

This design document outlines the initial MVP for the game, focusing on the early narrative and gameplay mechanics—from awakening through resource conversion and initial equipment crafting, setting up the transition to combat. The approach maximizes reuse of assets, leverages my dynamic button system and world fact mechanics, and creates a narrative-rich, text-driven experience.

---
