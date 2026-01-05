# StandardSystem Source Code Examination

## Source Files Examined

### Core StandardSystem Files

1. **standardsystem.h** (162 lines)
   Location: `/home/gallasko/PgEngine/src/Engine/ECS/standardsystem.h`
   
   Contents:
   - StandardSystemBuilder class (fluent builder pattern)
   - StandardSystemHandle class (interface for scripts)
   - Callback type definitions (_S_InitCallback, _S_EventCallback, etc.)
   - Helper function: createStandardSystem()
   
   Key Classes:
   - StandardSystemBuilder: Lines 76-150 (builder pattern)
   - StandardSystemHandle: Lines 118-145 (callback interface)
   - BuilderData: Lines 125-147 (internal configuration)

2. **standardsystem.cpp** (313 lines)
   Location: `/home/gallasko/PgEngine/src/Engine/ECS/standardsystem.cpp`
   
   Contents:
   - StandardSystemHandle implementation (methods)
   - StandardSystemBuilder implementation (methods)
   - Component operations (create, get, remove)
   - Event sending methods
   - System data access methods
   
   Key Methods:
   - createComponent(): Lines 50-77
   - getComponent(): Lines 106-123
   - removeComponent(): Lines 79-104
   - getData/setData(): Lines 125-147
   - sendEvent() overloads: Lines 27-48

### Related Component Files

3. **component.h** (205 lines)
   Location: `/home/gallasko/PgEngine/src/Engine/ECS/component.h`
   
   Contents:
   - StandardComponent struct (component with dynamic properties)
   - Ctor interface (onCreation hook)
   - Dtor interface (onDeletion hook)
   - Copy interface (onCopy hook)
   - Component base class
   
   Key Structs:
   - StandardComponent: Lines 111-204
   - Ctor: Lines 29-34
   - Dtor: Lines 70-75
   - Component: Lines 40-63

4. **standardevent.h** (119 lines)
   Location: `/home/gallasko/PgEngine/src/Engine/ECS/standardevent.h`
   
   Contents:
   - StandardEvent struct (event with dynamic data)
   - STANDARD_EVENT_CONVERTIBLE macro
   - STANDARD_EVENT_CONVERSION_IMPL macro
   - Type traits for event conversion
   - ElementType reference
   
   Key Structs:
   - StandardEvent: Lines 60-118
   - has_to_standard_event type trait: Lines 28-37

### ECS System Framework

5. **system.h** (822 lines)
   Location: `/home/gallasko/PgEngine/src/Engine/ECS/system.h`
   
   Contents:
   - AbstractSystem base class (all systems inherit from this)
   - Execution policies (ExecutionPolicy enum)
   - Template System<> class (traditional component systems)
   - Group management
   - Entity iteration methods
   
   Key Classes:
   - AbstractSystem: Lines 67-109
   - StandardSystemImpl: Lines 160-399 (embedded in header)
   - System<>: Lines 642-822 (template)
   - ExecutionPolicy enum: Lines 22-29
   
   Key Methods:
   - view<Type>(): Lines 780-785 (entity iteration)
   - registerGroup<Types...>(): Lines 788-813 (entity filtering)
   - setPolicy(): Line 76

### Input System Events

6. **inputcomponent.h** (376 lines)
   Location: `/home/gallasko/PgEngine/src/Engine/Input/inputcomponent.h`
   
   Contents:
   - OnMouseClick, OnMouseRelease events
   - OnMouseMove, OnSDLMouseWheel events
   - OnSDLScanCode, OnSDLScanCodeReleased, OnSDLTextInput events
   - OnSDLGamepad* events
   - Mouse click system implementation
   - Mouse hover system implementation
   
   Key Event Structs:
   - OnSDLScanCode: Lines 116-122
   - OnMouseClick: Lines 60-70
   - OnMouseMove: Lines 101-107
   - OnSDLMouseWheel: Lines 132-138
   - OnSDLTextInput: Lines 109-114
   
   Key System Classes:
   - MouseClickSystem: Lines 172-193
   - MouseHoverSystem: Lines 283-367

### Core Engine Events

7. **coresystems.h** (250 lines)
   Location: `/home/gallasko/PgEngine/src/Engine/Systems/coresystems.h`
   
   Contents:
   - TickEvent struct (per-frame timing)
   - TickingSystem class (sends TickEvents)
   - TimerSystem class (timer component handling)
   - EntityNameSystem class
   - TextInputTriggeredEvent
   - OnEventComponentSystem
   
   Key Structs:
   - TickEvent: Lines 56-61
   - Timer component: Lines 157-181
   
   Key Systems:
   - TickingSystem: Lines 63-155 (sends tick events)
   - TimerSystem: Lines 183-228

### Element Type System

8. **elementtype.h** (365 lines)
   Location: `/home/gallasko/PgEngine/src/Engine/Memory/elementtype.h`
   
   Contents:
   - ElementType struct (universal type container)
   - Union for type storage
   - Type conversions (int, float, double, bool, string, size_t)
   - Operators (+, -, *, /, %, comparisons)
   - Type checking methods
   
   Key Unions:
   - U union: Lines 33-55 (data storage)
   
   Key Methods:
   - setValue() overloads: Lines 207-278
   - get<T>(): Lines 280-284
   - isNumber(), isBool(), isLiteral(): Lines 286-299
   - Operator overloads: Lines 305-321
   
   Supported Types:
   - float (FLOAT)
   - double (DOUBLE)
   - int (INT)
   - size_t (SIZE_T)
   - string (STRING)
   - bool (BOOL)

### Entity Lifecycle Components

9. **oneventcomponent.h** (69 lines)
   Location: `/home/gallasko/PgEngine/src/Engine/Systems/oneventcomponent.h`
   
   Contents:
   - OnEventComponent (for typed events)
   - OnStandardEventComponent (for named events)
   - Both implement Ctor and Dtor for lifecycle management
   
   Key Structs:
   - OnEventComponent: Lines 7-45
   - OnStandardEventComponent: Lines 47-68

### Documentation Files Created

10. **STANDARD_SYSTEM_INDEX.md** (280 lines)
    Location: `/home/gallasko/PgEngine/docs/STANDARD_SYSTEM_INDEX.md`
    
    Contents:
    - Quick start guide
    - Documentation overview
    - Common use cases (5 examples)
    - Key concepts
    - Performance characteristics
    - Integration notes

11. **STANDARD_SYSTEM_CAPABILITIES.md** (752 lines)
    Location: `/home/gallasko/PgEngine/docs/STANDARD_SYSTEM_CAPABILITIES.md`
    
    Contents:
    - 12 comprehensive sections
    - All available callbacks
    - Method signatures and documentation
    - Event types and usage
    - Component management
    - System data storage
    - Entity lifecycle hooks
    - Complete examples

12. **STANDARD_SYSTEM_QUICK_REFERENCE.md** (237 lines)
    Location: `/home/gallasko/PgEngine/docs/STANDARD_SYSTEM_QUICK_REFERENCE.md`
    
    Contents:
    - One-page cheat sheet
    - Method signatures in table format
    - Common patterns
    - Code snippets
    - Important notes and limitations

## File Statistics

### Source Code Summary

| File | Lines | Type | Key Content |
|------|-------|------|------------|
| standardsystem.h | 162 | Header | Builder interface |
| standardsystem.cpp | 313 | Implementation | System implementation |
| system.h | 822 | Header | Base system classes |
| component.h | 205 | Header | Component definitions |
| standardevent.h | 119 | Header | Event structures |
| inputcomponent.h | 376 | Header | Input event types |
| coresystems.h | 250 | Header | Core engine events |
| elementtype.h | 365 | Header | Type system |
| oneventcomponent.h | 69 | Header | Lifecycle components |
| **TOTAL SOURCE** | **2,681** | **C++** | **Code examined** |

### Documentation Created

| File | Lines | Type | Audience |
|------|-------|------|----------|
| STANDARD_SYSTEM_INDEX.md | 280 | Markdown | All developers |
| STANDARD_SYSTEM_CAPABILITIES.md | 752 | Markdown | Reference/learning |
| STANDARD_SYSTEM_QUICK_REFERENCE.md | 237 | Markdown | Quick lookup |
| **TOTAL DOCS** | **1,269** | **Markdown** | **Knowledge base** |

## Examination Methodology

### Search Patterns Used

1. File discovery: `*.h` pattern matching
2. Code search: 
   - `TickEvent|OnSDL|onEntity|SDL` 
   - `StandardSystem*` files
   - `Event|struct` patterns
3. Grep searches: 
   - `grep -r "TickEvent|OnSDL"` for events
   - `grep -r "struct.*Event"` for event definitions
   - `grep -r "onEvent|registerGroup"` for methods

### Key Classes Examined

1. **StandardSystemHandle** - Public interface
2. **StandardSystemBuilder** - Builder pattern
3. **StandardSystemImpl** - Implementation
4. **StandardEvent** - Event type
5. **StandardComponent** - Component type
6. **ElementType** - Type system
7. **System<>** - Base system template
8. **AbstractSystem** - System interface
9. **ExecutionPolicy** - Execution modes

### Code Flow Analysis

Examined the complete flow:
1. System creation via StandardSystemBuilder
2. Callback registration (init, event, execute, delta)
3. Component creation/removal lifecycle
4. Event sending and reception
5. System data storage access
6. Save/load serialization
7. Entity iteration limitations

## Conclusions

### Complete Coverage

The StandardSystem provides a complete, non-template-based interface for ECS system creation covering:
- 7 different callback types
- 4 component operation methods
- 3 event sending variants
- 5 execution policies
- Full ECS integration
- Script support

### Documentation Completeness

Created 3 complementary documents:
- Quick reference (237 lines) - for immediate lookup
- Detailed capabilities (752 lines) - for learning
- Index (280 lines) - for navigation

Total: ~1,269 lines of documentation covering ~2,681 lines of source code examined.

### Key Findings

1. StandardSystem is a complete wrapper avoiding template complexity
2. All major ECS features are accessible through the handle
3. Dynamic component and event systems enable runtime flexibility
4. ElementType provides type-safe dynamic storage
5. Full event routing and component lifecycle hooks present
6. 5 execution policies allow flexible system scheduling
7. Entity iteration is limited (workaround: access world directly)

## Files Created/Modified

### New Documentation Files
- `/home/gallasko/PgEngine/docs/STANDARD_SYSTEM_INDEX.md`
- `/home/gallasko/PgEngine/docs/STANDARD_SYSTEM_CAPABILITIES.md`
- `/home/gallasko/PgEngine/docs/STANDARD_SYSTEM_QUICK_REFERENCE.md`
- `/home/gallasko/PgEngine/docs/SOURCE_FILES_EXAMINED.md` (this file)

### No Source Files Modified
All examination was read-only. No changes to source code.

