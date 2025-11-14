/**
 * Template Instantiation Optimization Starter File
 *
 * This file explicitly instantiates common template types used across your codebase.
 * By instantiating them here once, other .cpp files won't re-instantiate them,
 * significantly reducing compilation time.
 *
 * USAGE:
 * 1. Add this file to CMakeLists.txt under ENGINESOURCE
 * 2. Add corresponding 'extern template' declarations to the headers
 * 3. Rebuild and measure improvements
 *
 * Expected savings: 20-30 seconds off total compile time
 */

#include "stdafx.h"

// ECS headers
#include "ECS/system.h"
#include "ECS/eventlistener.h"
#include "ECS/entity.h"
#include "ECS/componentregistry.h"

// TODO: Include your component headers
// Example:
#include "2D/position.h"
// #include "2D/texture.h"
// #include "Input/inputcomponent.h"
// #include "UI/focusable.h"
// ... etc

// TODO: Include your event headers
// Example:
// #include "Event/mouseclick.h"
// #include "Event/keypress.h"
// ... etc

namespace pg
{

// =============================================================================
// STEP 1: Find your most common component types
// =============================================================================
// To find which types to instantiate, look at the compilation trace files
// or check which components are most frequently used in your systems.

// Example explicit instantiations (UNCOMMENT AND MODIFY FOR YOUR TYPES):

// template struct Own<UiComponent>;

/*
// Component ownership templates

template struct Own<Position>;
template struct Own<Texture>;
template struct Own<Simple2DObject>;
template struct Own<Focusable>;
template struct Own<TextInput>;
template struct Own<Button>;

template struct Ref<InputComponent>;
template struct Ref<Position>;
template struct Ref<Renderer>;
*/

// =============================================================================
// STEP 2: Find your most common event types
// =============================================================================

// Example event listener instantiations (UNCOMMENT AND MODIFY FOR YOUR TYPES):

/*
template struct Listener<MouseClickEvent>;
template struct Listener<MouseMoveEvent>;
template struct Listener<KeyPressEvent>;
template struct Listener<KeyReleaseEvent>;
template struct Listener<WindowResizeEvent>;

template struct QueuedListener<MouseClickEvent>;
template struct QueuedListener<KeyPressEvent>;
template struct QueuedListener<UpdateEvent>;
*/

// =============================================================================
// STEP 3: Find your most common System<...> combinations
// =============================================================================
// Look at your actual system declarations to find common patterns

// Example system instantiations (UNCOMMENT AND MODIFY FOR YOUR TYPES):

/*
// Common system combinations
template class System<Own<Position>, Own<Texture>>;
template class System<Ref<InputComponent>, Own<Position>>;
template class System<Own<Focusable>, Own<TextInput>>;
template class System<Listener<MouseClickEvent>, Own<Button>>;

// Systems with multiple components
template class System<Own<Position>, Own<Simple2DObject>, Ref<Camera2D>>;
template class System<Own<Sizer>, Own<ListView>, Listener<WindowResizeEvent>>;
*/

// =============================================================================
// STEP 4: Add function template instantiations
// =============================================================================

// Example registerComponents instantiations (UNCOMMENT AND MODIFY):

/*
template void registerComponents<Own<Position>>(
    System<Own<Position>>*,
    ComponentRegistry*,
    const tag<Own<Position>>&
);

template void registerComponents<Own<Position>, Own<Texture>>(
    System<Own<Position>, Own<Texture>>*,
    ComponentRegistry*,
    const tag<Own<Position>>&,
    const tag<Own<Texture>>&
);
*/

} // namespace pg

// =============================================================================
// NEXT STEPS: Add extern declarations to headers
// =============================================================================
//
// In src/Engine/ECS/eventlistener.h, add:
/*
namespace pg {
    template<typename Event> struct Listener { ... };

    // Add these extern declarations:
    extern template struct Listener<MouseClickEvent>;
    extern template struct Listener<KeyPressEvent>;
    // ... etc for all your event types
}
*/
//
// In src/Engine/ECS/system.h, add:
/*
namespace pg {
    template<typename... Comps> struct System { ... };

    // Add these extern declarations:
    extern template class System<Own<Position>, Own<Texture>>;
    extern template class System<Ref<InputComponent>, Own<Position>>;
    // ... etc for all your system combinations
}
*/
//
// =============================================================================

/*
 * HOW TO POPULATE THIS FILE:
 *
 * Option 1: Manually (based on your knowledge)
 *   - Look at your most-used systems
 *   - Add their template instantiations here
 *
 * Option 2: From trace analysis (more thorough)
 *   1. Analyze the window.cpp trace file:
 *      python3 build_profile/analyze_trace.py build_profile/CMakeFiles/ColumbaEngine.dir/src/Engine/window.cpp.json
 *
 *   2. Look for lines with "InstantiateClass" and "InstantiateFunction"
 *
 *   3. Identify the template types being instantiated
 *
 *   4. Add explicit instantiations for the most time-consuming ones
 *
 * Option 3: Gradual approach
 *   - Start with just your most common components (Position, Texture, etc.)
 *   - Measure improvement
 *   - Add more types iteratively
 *   - Stop when you've captured the "heavy hitters"
 */

// =============================================================================
// TESTING YOUR CHANGES
// =============================================================================
//
// Before:
//   cd build
//   make clean
//   time make -j1 ColumbaEngine  # Note the time
//
// After adding this file:
//   cd build
//   make clean
//   time make -j1 ColumbaEngine  # Compare the time
//
// Detailed analysis:
//   cd build_profile
//   python3 analyze_all_traces.py
//   # Look for reduction in "Template Instantiation" time
