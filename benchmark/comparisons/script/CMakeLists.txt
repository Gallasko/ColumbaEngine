# Script-language comparison benchmarks.
#
# Single C++ target so PgScript can be invoked from a shell harness like every
# other interpreter. The Python/Lua/Wren/GDScript runners are the interpreters
# themselves; the harness lives in run_scripts.py.

add_executable(pgscript_run pgscript_run.cpp)
target_link_libraries(pgscript_run PRIVATE ColumbaEngine)
target_compile_features(pgscript_run PRIVATE cxx_std_17)
set_target_properties(pgscript_run PROPERTIES EXCLUDE_FROM_ALL TRUE)
