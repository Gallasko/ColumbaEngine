# TTF Text Rendering System Analysis

## Executive Summary

The TTF text rendering system in PgEngine is a mature, FreeType-based text rendering implementation that creates individual render calls for each glyph. It mirrors the texture rendering system architecture but with one significant architectural difference: **TTF text stores render calls in a dedicated `TTFTextCall` component, while textures store them in the system itself**.

---

## Key Files Involved

### Core TTF Text Files

1. **`/home/gallasko/PgEngine/src/Engine/UI/ttftext.h`**
   - Main system header
   - Defines `TTFTextSystem` (extends `AbstractRenderer`)
   - Defines `TTFTextCall` wrapper component
   - Defines `Character` glyph metadata structure
   - Font management via FreeType library

2. **`/home/gallasko/PgEngine/src/Engine/UI/ttftext.cpp`**
   - Complete implementation
   - Font atlas generation and management
   - Render call creation (per-glyph)
   - Text formatting and parsing
   - Text wrapping logic

3. **`/home/gallasko/PgEngine/src/Engine/Components/TTFText.generated.h`**
   - Component definition
   - Properties: text, fontPath, scale, colors, wrap, spacing, viewport
   - Getters/setters with event emission
   - Serialization interface

4. **`/home/gallasko/PgEngine/src/Engine/Components/TTFText.generated.cpp`**
   - Setter implementations that trigger `TTFTextChangedEvent`
   - Serialization/deserialization code

### Comparison Texture Files

1. **`/home/gallasko/PgEngine/src/Engine/2D/texture.h`**
   - `Texture2DComponentSystem` for comparison
   - Different render call storage strategy

2. **`/home/gallasko/PgEngine/src/Engine/Components/Texture2DComponent.generated.h`**
   - Single simple component (no call wrapper)

---

## How TTF Text is Initialized and Managed

### 1. System Creation (Application Level)

```
application.cpp:
├─ Create TTFTextSystem with MasterRenderer reference
├─ System depends on MasterRenderer
├─ Register fonts via ttfSys->registerFont()
└─ System dependency: MasterRenderer → TTFTextSystem
```

### 2. System Initialization Flow

```cpp
TTFTextSystem::init()
├─ Set up base material preset
│  ├─ Shader: "ttfTexture"
│  ├─ Number of textures: 1
│  ├─ Uniforms: ScreenWidth, ScreenHeight
│  └─ Mesh: {3, 2, 1, 1, 3, 1, 4}
├─ Register entity group for (PositionComponent, TTFText)
│  ├─ On add: Queue entity for text update
│  ├─ On remove: Detach TTFTextCall component
│  └─ Mark changed = true
└─ Create FreeType library instance
```

### 3. Font Registration

```cpp
void registerFont(const std::string& fontPath, const std::string& fontName, int size)
├─ Initialize FreeType face for font file
├─ Generate glyph atlas (1024x1024 texture)
│  ├─ Render each glyph (chars 32-126) to atlas
│  ├─ Store UV coordinates for each glyph
│  └─ Store glyph metrics (size, bearing, advance)
├─ Upload atlas to OpenGL as GL_RED texture
├─ Store Character metadata in charactersMap[fontName][char]
└─ Queue texture registration in MasterRenderer
```

### 4. Component Creation

```cpp
makeTTFText() helper creates:
├─ Entity
├─ PositionComponent (position, size, z-depth)
├─ UiAnchor (UI anchoring)
└─ TTFText component
    ├─ text: string content
    ├─ fontPath: reference to registered font
    ├─ scale: text scale factor
    ├─ colors: RGBA color
    ├─ wrap: word wrapping enabled
    └─ spacing: line spacing
```

---

## Rendering Preparation Workflow

### Event-Driven Update Mechanism

```
Trigger Event:
├─ PositionComponentChangedEvent
│  └─ Fired by PositionComponent setters (x, y, z, width, height, rotation)
├─ TTFTextChangedEvent
│  └─ Fired by TTFText setters (text, fontPath, scale, colors, wrap, spacing, viewport)
└─ Entity added to group
    └─ Fired when (PositionComponent, TTFText) group gets new member

    ↓

onEvent() / onEventUpdate():
├─ Push entity ID to textUpdateQueue
└─ Set changed = true

    ↓

execute() method:
├─ Clear previous renderCallList
├─ Clear materialId cache
├─ Collect all TTFTextCall components from entities
│  └─ Add their render calls to renderCallList
├─ Process textUpdateQueue
│  ├─ For each queued entity:
│  │  ├─ Get PositionComponent (ui)
│  │  ├─ Get TTFText (obj)
│  │  ├─ Create render calls via createRenderCall()
│  │  └─ Either update or create TTFTextCall component
│  └─ Pop from queue
├─ Call finishChanges()
│  └─ Set changed = false, dirty = true (marks for rendering)
└─ Exit (if no changes and queue empty)
```

---

## Render Call Creation Pipeline

### createRenderCall() - Core Method

```cpp
std::vector<RenderCall> createRenderCall(
    CompRef<PositionComponent> ui,
    CompRef<TTFText> obj
)
```

**Process:**

1. **Text Parsing**
   ```
   parseFormattedText(obj)
   ├─ Parse escape sequences:
   │  ├─ \n or \n{} → newline
   │  └─ \c{r,g,b,a} → color change
   ├─ Split into segments
   │  └─ Each segment: (text, color)
   └─ Return vector<TTFText> segments
   ```

2. **Layout Calculation**
   ```
   For each segment:
   ├─ Compute line height from glyphs
   ├─ Determine max width from PositionComponent
   ├─ Track current X, Y position
   └─ Process character by character
   ```

3. **Per-Character Processing**
   ```
   For each character in segment:
   ├─ If space:
   │  └─ Advance X by space width
   ├─ If non-space (start of word):
   │  ├─ Calculate full word width
   │  ├─ Check if wraps beyond maxWidth
   │  │  └─ If yes: advance Y, reset X
   │  └─ Create glyph render call
   │      └─ createGlyphRenderCall()
   └─ Advance X by glyph advance
   ```

4. **Glyph Render Call Creation**
   ```cpp
   RenderCall createGlyphRenderCall(
       ui, fontPath, materialId,
       character, currentX, currentY, z,
       scale, lineHeight, colors, viewport
   )
   
   Creates:
   ├─ RenderCall object
   ├─ Position from current cursor + glyph bearing
   ├─ Size from glyph dimensions
   ├─ UV coordinates from atlas
   ├─ Color from segment
   ├─ Material ID (font atlas + shader)
   ├─ Opacity: Additive (supports transparency)
   └─ data[15]:
       ├─ [0-4]: x, y, z, width, height
       ├─ [5-9]: rotation, opacity, color RGB
       ├─ [10]: 1.0f (flag)
       └─ [11-14]: UV coords (top-left, bottom-right)
   ```

5. **Dimensions Update**
   ```
   After layout:
   ├─ Calculate total text width and height
   ├─ Update obj->textWidth / obj->textHeight
   └─ Update ui->width / ui->height
       (only if values changed significantly)
   ```

---

## Component/System Architecture

### ECS Structure

```
Entity
├─ Components:
│  ├─ PositionComponent (Own)
│  │  └─ Position, size, rotation, z-depth
│  ├─ UiAnchor (typically added)
│  │  └─ UI anchoring logic
│  ├─ TTFText (Own)
│  │  └─ Text content and styling
│  └─ TTFTextCall (Own)
│     └─ Vector of RenderCall objects
│        (Created by system, not by user)
│
└─ System: TTFTextSystem
   ├─ Extends: AbstractRenderer
   ├─ Registered with: Own<TTFText>, Own<TTFTextCall>, Ref<PositionComponent>
   ├─ Listeners:
   │  ├─ PositionComponentChangedEvent
   │  └─ TTFTextChangedEvent
   └─ InitSys marker
```

### System Dependencies and Initialization

```
Dependency Chain:
MasterRenderer → TTFTextSystem

Because:
├─ TTFTextSystem uses MasterRenderer for:
│  ├─ Shader retrieval ("ttfTexture")
│  ├─ Texture registration (atlas)
│  ├─ Material registration (shader + atlas)
│  └─ Material ID lookups
└─ Font registration must happen after system init
```

### Key Data Structures

```cpp
// Glyph metadata (one per character per font)
struct Character {
    glm::ivec2 size;            // Glyph bitmap size
    glm::ivec2 bearing;         // Offset from baseline
    unsigned int advance;       // Pixels to next glyph (×64)
    glm::vec2 uvTopLeft;        // Atlas UV coordinates
    glm::vec2 uvBottomRight;    // Atlas UV coordinates
};

// Maps: fontPath → character → Character metadata
std::unordered_map<std::string, 
    std::unordered_map<char, Character>> charactersMap;

// Wrapper component for render calls
struct TTFTextCall {
    std::vector<RenderCall> calls;  // All glyphs for one text entity
};

// Queue for deferred updates
std::queue<_unique_id> textUpdateQueue;

// Cache material IDs by font to avoid lookups
std::map<std::string, size_t> currentLoadedMaterialId;
```

---

## Critical Differences from Texture System

### Texture System (Texture2DComponent)

```
Component Storage:
Entity
├─ PositionComponent
├─ Texture2DComponent
    └─ Single texture reference

System Storage:
Texture2DComponentSystem
├─ entityRenderCalls: Map<entityId → RenderCall>
│                    (ONE call per entity)
├─ entitiesInRenderGroup: Vector<entityId>
└─ textureUpdateSet: Set<entityId>

Render Call Count: 1 per entity
```

### TTF Text System (TTFText)

```
Component Storage:
Entity
├─ PositionComponent
├─ TTFText
│   └─ Text content, styling, metrics
└─ TTFTextCall
    └─ Vector<RenderCall>
         (ONE per glyph character!)

System Storage:
TTFTextSystem
├─ (No per-entity storage)
├─ textUpdateQueue: Queue<entityId>
└─ charactersMap: Font glyph metadata
└─ currentLoadedMaterialId: Cache

Render Call Count: N per entity (N = number of characters)
```

### Key Architectural Difference

| Aspect | Texture System | TTF Text System |
|--------|---|---|
| **Render Call Storage** | System-owned map | Component-owned vector |
| **Render Calls per Entity** | 1 (simple) | N (per glyph) |
| **Data Location** | textures live in system | all data in components |
| **Rebuild Strategy** | Rebuild single call per entity | Rebuild vector of calls per entity |
| **Memory Model** | System keeps references | Component owns calls |

**Why TTFTextCall exists:**
- Each character needs its own render call with:
  - Unique position (X, Y + glyph metrics)
  - Unique UV coordinates (atlas location)
  - Unique color (if color tags present)
- A single call cannot represent multiple glyphs
- Storing in component allows clean ECS iteration

---

## Rendering Pipeline Integration

### AbstractRenderer Base Class

```cpp
class AbstractRenderer {
    std::vector<RenderCall> renderCallList;  // Output for MasterRenderer
    RenderStage renderStage;                 // When to render (Render/PreRender/PostProcess)
    bool changed, dirty;                     // Change tracking
    
    finishChanges() → sets dirty=true for MasterRenderer
};
```

### TTF Text Integration

```
TTFTextSystem::execute()
├─ Build renderCallList from TTFTextCall components
├─ Process update queue (create/update TTFTextCall)
└─ finishChanges() → dirty = true

MasterRenderer::render()
├─ Collect renderCallList from all systems
├─ Sort by RenderCall::key (bitfield)
├─ Batch and render calls
└─ Each call uses:
    ├─ Material (shader + font atlas texture)
    ├─ Mesh (simple quad)
    ├─ Data (position, UV, color, etc.)
    └─ RenderStage: Render (normal opaque/transparent pass)
```

---

## Glyph Atlas System

### Generation Process

```
registerFont():
├─ Create 1024×1024 pixel buffer (RGBA or L format)
├─ For each character (32-126):
│  ├─ Render glyph to FreeType bitmap
│  ├─ Pack into atlas using simple rectangle packing
│  │  └─ Left-to-right, wrap to next row
│  ├─ Store UV coordinates
│  └─ Store glyph metrics
├─ Upload to OpenGL as GL_RED texture (GL_LUMINANCE on WebGL)
├─ Set texture parameters (LINEAR filter, CLAMP_TO_EDGE)
└─ Register with MasterRenderer for caching
```

### Multiple Font Support

```
Each font gets:
├─ Separate FreeType face
├─ Separate glyph atlas texture
├─ Separate entry in charactersMap[fontName]
├─ Separate material (same shader, different texture)
└─ Independent advance/bearing metrics
```

---

## Formatting Support

### Inline Escape Sequences

```
Text string patterns:
├─ Newline:
│  ├─ Literal: '\n'
│  └─ Escape: '\n'
├─ Color change:
│  └─ Format: '\c{r,g,b,a}'
│  └─ Values: 0-255 floats

Example:
"Hello\c{255,0,0,255} RED\c{0,255,0,255} GREEN\n" +
"Next line\c{0,0,255,255} BLUE"
```

### Segment Parsing

```
parseFormattedText():
├─ Scan text for escape sequences
├─ Split into segments at boundaries
├─ Each segment carries color context
└─ Return vector<TTFText> with per-segment colors

Then createRenderCall() uses segment colors
for each character in that segment.
```

---

## Performance Characteristics

### Strengths

- **Per-font optimization**: Single atlas texture per font
- **Lazy updates**: Only regenerates render calls on change
- **Batching-friendly**: Same material = same shader + texture
- **Format support**: Inline color changes without rebuilding

### Potential Issues

- **Glyph count scaling**: 100 characters = 100 render calls
  - vs. texture system: 1 render call
- **Atlas lookup**: O(1) per glyph via map
- **Memory**: All render calls stored in component
- **Update cost**: Full rebuild of all glyph calls on text change
  - Could be optimized with partial updates

---

## Missing Features vs. Texture System

1. **No atlas support for text** 
   - Textures support "atlas.texture" naming
   - TTF could theoretically pre-render to atlas

2. **No per-entity render call map in system**
   - System could cache calls like Texture2D does
   - Currently relies on component storage

3. **No opacity/blending control** 
   - Always uses Additive (hardcoded)
   - Textures support configurable opacity

4. **Limited atlas size**
   - Fixed 1024×1024 for all fonts
   - Could exceed for very large fonts

---

## Code Flow Summary

```
Initialization:
application.cpp → ecs.createSystem<TTFTextSystem>()
    ↓
system.init() → Set up material, register groups, init FreeType
    ↓
application.cpp → registerFont()
    ↓
FreeType atlas generation, texture upload, character mapping

Runtime:
User modifies text property
    ↓
TTFText::setText() → ecsRef->sendEvent(TTFTextChangedEvent)
    ↓
TTFTextSystem::onEvent() → push to textUpdateQueue, set changed=true
    ↓
Engine calls TTFTextSystem::execute()
    ↓
Process textUpdateQueue → createRenderCall() → TTFTextCall component
    ↓
Collect renderCallList from TTFTextCall components
    ↓
MasterRenderer collects all render calls, sorts, and renders
    ↓
GPU renders each glyph quad with font atlas texture
```

