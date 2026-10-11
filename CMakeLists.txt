cmake_minimum_required(VERSION 3.25)

project(FuseCAD
    VERSION 0.1.0
    DESCRIPTION "Open-source parametric CAD"
    HOMEPAGE_URL "https://github.com/OpenDeskOrg/fusecad"
    LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use: cmake --preset dev")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(ProjectOptions)

# --- Dependencies -------------------------------------------------------------
find_package(nlohmann_json 3.11 CONFIG REQUIRED)

if(FUSE_WITH_OCCT)
    # No version argument: OCCT's config file only accepts an exact patch match.
    find_package(OpenCASCADE CONFIG REQUIRED)
    if(OpenCASCADE_VERSION VERSION_LESS 7.8)
        message(FATAL_ERROR "OpenCASCADE >= 7.8 is required, found ${OpenCASCADE_VERSION} in ${OpenCASCADE_DIR}")
    endif()
    message(STATUS "OpenCASCADE ${OpenCASCADE_VERSION}: ${OpenCASCADE_DIR}")
endif()

if(FUSE_BUILD_APP AND NOT FUSE_WITH_OCCT)
    message(FATAL_ERROR "FUSE_BUILD_APP requires FUSE_WITH_OCCT")
endif()

# --- Targets ------------------------------------------------------------------
if(FUSE_BUILD_TESTS)
    enable_testing() # before apps/, which registers the CLI smoke test
endif()

add_subdirectory(src)
add_subdirectory(apps)

if(FUSE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# --- Summary ------------------------------------------------------------------
message(STATUS "")
message(STATUS "FuseCAD ${PROJECT_VERSION}")
message(STATUS "  OpenCASCADE kernel : ${FUSE_WITH_OCCT}")
message(STATUS "  Desktop app (Qt 6) : ${FUSE_BUILD_APP}")
message(STATUS "  Tests              : ${FUSE_BUILD_TESTS}")
message(STATUS "  Sanitizers         : ${FUSE_SANITIZERS}")
message(STATUS "  Warnings as errors : ${FUSE_WARNINGS_AS_ERRORS}")
message(STATUS "")
