# Cross-engine ECS comparison benchmarks.
#
# Gated by PGE_BUILD_COMPARISON_BENCHMARKS at the top level. Each engine has
# its own standalone executable. They all emit the same CSV schema (see
# scenarios.h) so results aggregate cleanly.
#
# Targets so far:
#   pgengine_ecs_bench   — PgEngine (always when this dir is included)
#
# Coming next:
#   entt_ecs_bench, flecs_ecs_bench, bevy (via Cargo, not CMake)

add_executable(pgengine_ecs_bench pgengine_bench.cpp)

target_link_libraries(pgengine_ecs_bench PRIVATE ColumbaEngine)

target_compile_features(pgengine_ecs_bench PRIVATE cxx_std_17)

# Keep this target out of "all" — explicit build avoids surprising casual users.
set_target_properties(pgengine_ecs_bench PROPERTIES EXCLUDE_FROM_ALL TRUE)
