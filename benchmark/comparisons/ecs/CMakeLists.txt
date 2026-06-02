# Cross-engine ECS comparison benchmarks.
#
# Gated by PGE_BUILD_COMPARISON_BENCHMARKS at the top level. Each engine has
# its own standalone executable. They all emit the same CSV schema (see
# scenarios.h) so results aggregate cleanly.
#
# Targets:
#   pgengine_ecs_bench   PgEngine
#   entt_ecs_bench       EnTT  (fetched, sparse-set)
#   flecs_ecs_bench      flecs (fetched, archetype)
#
# Coming next:
#   bevy (via Cargo, not CMake)

include(FetchContent)

# ---------------- PgEngine ----------------
add_executable(pgengine_ecs_bench pgengine_bench.cpp)
target_link_libraries(pgengine_ecs_bench PRIVATE ColumbaEngine)
target_compile_features(pgengine_ecs_bench PRIVATE cxx_std_17)
set_target_properties(pgengine_ecs_bench PROPERTIES EXCLUDE_FROM_ALL TRUE)

# ---------------- EnTT ----------------
# Pinned to a known-good release (v3.13.2 — late 2024). Bump intentionally.
FetchContent_Declare(
    EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(EnTT)

add_executable(entt_ecs_bench entt_bench.cpp)
target_link_libraries(entt_ecs_bench PRIVATE EnTT::EnTT)
target_compile_features(entt_ecs_bench PRIVATE cxx_std_17)
target_include_directories(entt_ecs_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(entt_ecs_bench PROPERTIES EXCLUDE_FROM_ALL TRUE)

# ---------------- flecs ----------------
# Archetype ECS, the "other school" of design (vs sparse-set in EnTT/PgEngine).
# Pinned to v4.0.5. Disable flecs's own tests/examples — we only want the lib.
set(FLECS_TESTS OFF CACHE BOOL "" FORCE)
set(FLECS_BENCHMARKS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    flecs
    GIT_REPOSITORY https://github.com/SanderMertens/flecs.git
    GIT_TAG        v4.0.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(flecs)

add_executable(flecs_ecs_bench flecs_bench.cpp)
target_link_libraries(flecs_ecs_bench PRIVATE flecs::flecs_static)
target_compile_features(flecs_ecs_bench PRIVATE cxx_std_17)
target_include_directories(flecs_ecs_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(flecs_ecs_bench PROPERTIES EXCLUDE_FROM_ALL TRUE)
