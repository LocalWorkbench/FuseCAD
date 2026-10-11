cmake_minimum_required(VERSION 3.24)

project(OpenCircuit
    VERSION 0.1.0
    DESCRIPTION "Open-source parametric CAD"
    HOMEPAGE_URL "https://github.com/jareer/opencircuit"
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
find_package(nlohmann_json 3.11 QUIET CONFIG)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

if(OC_WITH_OCCT)
    find_package(OpenCASCADE CONFIG REQUIRED)
    message(STATUS "OpenCASCADE ${OpenCASCADE_VERSION}: ${OpenCASCADE_DIR}")
endif()

if(OC_BUILD_APP)
    if(NOT OC_WITH_OCCT)
        message(FATAL_ERROR "OC_BUILD_APP requires OC_WITH_OCCT")
    endif()
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
    qt_standard_project_setup()
endif()

# --- Targets ------------------------------------------------------------------
if(OC_BUILD_TESTS)
    enable_testing() # before apps/, which registers the CLI smoke test
endif()

add_subdirectory(src)
add_subdirectory(apps)

if(OC_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# --- Summary ------------------------------------------------------------------
message(STATUS "")
message(STATUS "OpenCircuit ${PROJECT_VERSION}")
message(STATUS "  OpenCASCADE kernel : ${OC_WITH_OCCT}")
message(STATUS "  Desktop app (Qt 6) : ${OC_BUILD_APP}")
message(STATUS "  Tests              : ${OC_BUILD_TESTS}")
message(STATUS "  Sanitizers         : ${OC_SANITIZERS}")
message(STATUS "  Warnings as errors : ${OC_WARNINGS_AS_ERRORS}")
message(STATUS "")
