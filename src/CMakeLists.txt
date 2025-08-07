cmake_minimum_required(VERSION 3.18)

project(ColumbaEngineSrc VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


include_directories(${GLM_DIR}/glm)
include_directories(${TASKFLOW_DIR}/taskflow)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    include_directories(${SDL2_DIR}/include)
    include_directories(${SDL2MIXER_DIR}/include)
    include_directories(${GLEW_DIR}/include)
endif()

# include(GNUInstallDirs)

# install(TARGETS ColumbaEngineSrc
#         EXPORT ColumbaEngineSrcTargets
#         LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#         ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#         RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#         INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# # install(FILES Engine/window.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# install(EXPORT ColumbaEngineSrcTargets
#         FILE ColumbaEngineSrcTargets.cmake
#         NAMESPACE pg::
#         DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ColumbaEngineSrc)