cmake_minimum_required(VERSION 3.18)
project(ColumbaEngineTestApp VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Try to find ColumbaEngine with multiple fallback strategies
find_package(ColumbaEngine QUIET)

if(NOT ColumbaEngine_FOUND)
    # Try with common installation paths
    set(CMAKE_PREFIX_PATH
        /usr/local/lib/cmake/ColumbaEngine
        /usr/lib/cmake/ColumbaEngine
        ~/.local/lib/cmake/ColumbaEngine
        ${CMAKE_PREFIX_PATH}
    )
    find_package(ColumbaEngine QUIET)
endif()

if(NOT ColumbaEngine_FOUND)
    # Try setting ColumbaEngine_DIR directly
    if(EXISTS "/usr/local/lib/cmake/ColumbaEngine/ColumbaEngineConfig.cmake")
        set(ColumbaEngine_DIR "/usr/local/lib/cmake/ColumbaEngine")
        find_package(ColumbaEngine REQUIRED)
    elseif(EXISTS "$ENV{HOME}/.local/lib/cmake/ColumbaEngine/ColumbaEngineConfig.cmake")
        set(ColumbaEngine_DIR "$ENV{HOME}/.local/lib/cmake/ColumbaEngine")
        find_package(ColumbaEngine REQUIRED)
    else()
        message(FATAL_ERROR "ColumbaEngine not found. Please ensure it's installed or set ColumbaEngine_DIR manually.")
    endif()
endif()

# Create executable
add_executable(ColumbaEngineTestApp
    src/main.cpp
    src/application.cpp
)

# Link with ColumbaEngine
target_link_libraries(ColumbaEngineTestApp PRIVATE ColumbaEngine::ColumbaEngine)

# Print debug info
message(STATUS "ColumbaEngine found: ${ColumbaEngine_FOUND}")
message(STATUS "ColumbaEngine include dir: ${ColumbaEngine_INCLUDE_DIR}")
message(STATUS "ColumbaEngine library: ${ColumbaEngine_LIBRARY}")
